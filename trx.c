/*
 * Copyright (C) 2020 Mark Hills <mark@xwax.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "defaults.h"
#include "device.h"
#include "notice.h"
#include "sched.h"
#include "rx_alsalib.h"
#include "rx_runlib.h"
#include "tx_alsalib.h"
#include "tx_runlib.h"
#include "trx_rtplib.h"

unsigned int verbose = DEFAULT_VERBOSE;

static void usage(FILE *fd)
{
	fprintf(fd, "Usage: trx [<parameters>]\n"
		"Real-time audio transmitter over IP\n");

	fprintf(fd, "\nAudio device (ALSA) parameters:\n");
	fprintf(fd, "  -d <dev>    Device name (default '%s')\n",
		DEFAULT_DEVICE);
	fprintf(fd, "  -m <ms>     Buffer time (default %d milliseconds)\n",
		DEFAULT_BUFFER);

	fprintf(fd, "\nNetwork parameters:\n");
	fprintf(fd, "  -h <addr>   IP address to send to (default %s)\n",
		DEFAULT_ADDR);
	fprintf(fd, "  -p <port>   UDP port number to receive on (default %d)\n",
		DEFAULT_PORT);
	fprintf(fd, "  -s <port>   UDP port number to send to (default %d)\n",
		DEFAULT_PORT);
	fprintf(fd, "  -j <ms>     Jitter buffer (default %d milliseconds)\n",
		DEFAULT_JITTER);
	fprintf(fd, "  -S <ssrc>   SSRC (default 0x%x)\n",
		DEFAULT_SSRC);

	fprintf(fd, "\nEncoding parameters:\n");
	fprintf(fd, "  -r <rate>   Sample rate (default %dHz)\n",
		DEFAULT_RATE);
	fprintf(fd, "  -c <n>      Number of channels (default %d)\n",
		DEFAULT_CHANNELS);
	fprintf(fd, "  -f <n>      Frame size (default %d samples, see below)\n",
		DEFAULT_FRAME);
	fprintf(fd, "  -b <kbps>   Bitrate (approx., default %d)\n",
		DEFAULT_BITRATE);

	fprintf(fd, "\nProgram parameters:\n");
	fprintf(fd, "  -v <n>      Verbosity level (default %d)\n",
		DEFAULT_VERBOSE);
	fprintf(fd, "  -D <file>   Run as a daemon, writing process ID to the given file\n");

	fprintf(fd, "\nAllowed frame sizes (-f) are defined by the Opus codec. For example,\n"
		"at 48000Hz the permitted values are 120, 240, 480 or 960.\n");
}

static RtpSession *session = NULL;

static void report_rtcp_info(int signal)
{
	const struct jitter_stats *jitter = rtp_session_get_jitter_stats(session);
	fprintf(stdout, "{\n");
	fprintf(stdout, "  \"round-trip\": %f,\n", rtp_session_get_round_trip_propagation(session) * 1000);
	fprintf(stdout, "  \"cum-loss\": %d,\n", rtp_session_get_cum_loss(session));
	fprintf(stdout, "  \"recv-bandwidth\": %.0f,\n", rtp_session_get_recv_bandwidth(session));
	fprintf(stdout, "  \"send-bandwidth\": %.0f,\n", rtp_session_compute_send_bandwidth(session));
	fprintf(stdout, "  \"jitter\": [%d, %d, %f]\n", jitter->jitter, jitter->max_jitter, jitter->jitter_buffer_size_ms);
	fprintf(stdout, "}\n");
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	int r, error;
	tx_args tx = {
		.channels = DEFAULT_CHANNELS,
		.frame = DEFAULT_FRAME,
	};
	rx_args rx = {
		.channels = DEFAULT_CHANNELS,
		.rate = DEFAULT_RATE
	};
	pthread_t tx_thread, rx_thread;

	/* command-line options */
	const char *device = DEFAULT_DEVICE,
		*tx_addr = DEFAULT_ADDR,
		*pid = NULL;
	unsigned int buffer = DEFAULT_BUFFER,
		jitter = DEFAULT_JITTER,
		kbps = DEFAULT_BITRATE,
		rx_port = DEFAULT_PORT,
		tx_port = DEFAULT_PORT;
	uint32_t ssrc = DEFAULT_SSRC;

	struct sigaction action = {
 		.sa_handler = &report_rtcp_info
	};

	for (;;) {
		int c;

		c = getopt(argc, argv, "b:c:d:f:h:j:m:p:r:s:v:D:S:");
		if (c == -1)
			break;

		switch (c) {
		case 'b':
			kbps = atoi(optarg);
			break;
		case 'c':
			rx.channels = tx.channels = atoi(optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 'f':
			tx.frame = atol(optarg);
			break;
		case 'h':
			tx_addr = optarg;
			break;
		case 'j':
			jitter = atoi(optarg);
			break;
		case 'm':
			buffer = atoi(optarg);
			break;
		case 'p':
			rx_port = atoi(optarg);
			break;
		case 'r':
			rx.rate = atoi(optarg);
			break;
		case 's':
			tx_port = atoi(optarg);
			break;
		case 'S':
		  ssrc = atoi(optarg);
			break;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 'D':
			pid = optarg;
			break;
		default:
			usage(stderr);
			return -1;
		}
	}

	tx.encoder = opus_encoder_create(rx.rate, tx.channels, OPUS_APPLICATION_AUDIO,
				&error);
	if (tx.encoder == NULL) {
		fprintf(stderr, "opus_encoder_create: %s\n",
			opus_strerror(error));
		return -1;
	}

	rx.decoder = opus_decoder_create(rx.rate, rx.channels, &error);
	if (rx.decoder == NULL) {
		fprintf(stderr, "opus_decoder_create: %s\n",
			opus_strerror(error));
		return -1;
	}

	tx.bytes_per_frame = kbps * 1024 * tx.frame / rx.rate / 8;

	/* Follow the RFC, payload 0 has 8kHz reference rate */

	tx.ts_per_frame = tx.frame * 8000 / rx.rate;

	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(NULL, ORTP_WARNING|ORTP_ERROR);
	session = rx.session  = tx.session = create_rtp_send_recv(tx_addr, tx_port, "0.0.0.0", rx_port, jitter, ssrc);
	assert(session != NULL);

 	sigaction(SIGUSR1, &action, NULL);

	r = snd_pcm_open(&tx.snd, device, SND_PCM_STREAM_CAPTURE, 0);
	if (r < 0) {
		aerror("snd_pcm_open", r);
		return -1;
	}
	if (set_alsa_hw(tx.snd, rx.rate, tx.channels, buffer * 1000) == -1)
		return -1;
	if (set_alsa_sw(tx.snd) == -1)
		return -1;

	r = snd_pcm_open(&rx.snd, device, SND_PCM_STREAM_PLAYBACK, 0);
	if (r < 0) {
		aerror("snd_pcm_open", r);
		return -1;
	}
	if (set_alsa_hw(rx.snd, rx.rate, rx.channels, buffer * 1000) == -1)
		return -1;
	if (set_alsa_sw(rx.snd) == -1)
		return -1;

	if (pid)
		go_daemon(pid);

	go_realtime();

	pthread_create(&tx_thread, NULL, (void * (*)(void *))run_tx, &tx);
	pthread_create(&rx_thread, NULL, (void * (*)(void *))run_rx, &rx);

	pthread_join(tx_thread, NULL);
	pthread_join(rx_thread, NULL);

	if (snd_pcm_close(tx.snd) < 0)
		abort();

	if (snd_pcm_close(rx.snd) < 0)
		abort();

	rtp_session_destroy(rx.session);
	ortp_exit();
	ortp_global_stats_display();

	opus_encoder_destroy(tx.encoder);
	opus_decoder_destroy(rx.decoder);

	return r;
}
