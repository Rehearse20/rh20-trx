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
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "defaults.h"
#include "device.h"
#include "notice.h"
#include "sched.h"
#include "rx_alsalib.h"
#include "rx_rtplib.h"
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

int main(int argc, char *argv[])
{
	int r, error;
	size_t bytes_per_frame;
	unsigned int ts_per_frame;
	snd_pcm_t *tx_snd;
	snd_pcm_t *rx_snd;
	OpusEncoder *encoder;
	OpusDecoder *decoder;
	RtpSession *session;

	/* command-line options */
	const char *device = DEFAULT_DEVICE,
		*tx_addr = DEFAULT_ADDR,
		*pid = NULL;
	unsigned int buffer = DEFAULT_BUFFER,
		rate = DEFAULT_RATE,
		jitter = DEFAULT_JITTER,
		channels = DEFAULT_CHANNELS,
		frame = DEFAULT_FRAME,
		kbps = DEFAULT_BITRATE,
		rx_port = DEFAULT_PORT,
		tx_port = DEFAULT_PORT;

	fputs(COPYRIGHT "\n", stderr);

	for (;;) {
		int c;

		c = getopt(argc, argv, "b:c:d:f:h:j:m:p:r:s:v:D:");
		if (c == -1)
			break;

		switch (c) {
		case 'b':
			kbps = atoi(optarg);
			break;
		case 'c':
			channels = atoi(optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 'f':
			frame = atol(optarg);
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
			rate = atoi(optarg);
			break;
		case 's':
			tx_port = atoi(optarg);
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

	encoder = opus_encoder_create(rate, channels, OPUS_APPLICATION_AUDIO,
				&error);
	if (encoder == NULL) {
		fprintf(stderr, "opus_encoder_create: %s\n",
			opus_strerror(error));
		return -1;
	}

	decoder = opus_decoder_create(rate, channels, &error);
	if (decoder == NULL) {
		fprintf(stderr, "opus_decoder_create: %s\n",
			opus_strerror(error));
		return -1;
	}

	bytes_per_frame = kbps * 1024 * frame / rate / 8;

	/* Follow the RFC, payload 0 has 8kHz reference rate */

	ts_per_frame = frame * 8000 / rate;

	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(NULL, ORTP_WARNING|ORTP_ERROR);
	session = create_rtp_send_recv(tx_addr, tx_port, "0.0.0.0", rx_port, jitter);
	assert(session != NULL);

	r = snd_pcm_open(&tx_snd, device, SND_PCM_STREAM_CAPTURE, 0);
	if (r < 0) {
		aerror("snd_pcm_open", r);
		return -1;
	}
	if (set_alsa_hw(tx_snd, rate, channels, buffer * 1000) == -1)
		return -1;
	if (set_alsa_sw(tx_snd) == -1)
		return -1;

	r = snd_pcm_open(&rx_snd, device, SND_PCM_STREAM_PLAYBACK, 0);
	if (r < 0) {
		aerror("snd_pcm_open", r);
		return -1;
	}
	if (set_alsa_hw(rx_snd, rate, channels, buffer * 1000) == -1)
		return -1;
	if (set_alsa_sw(rx_snd) == -1)
		return -1;

	if (pid)
		go_daemon(pid);

	go_realtime();

	r = run_tx(tx_snd, channels, frame, encoder, bytes_per_frame,
		ts_per_frame, session);

	r = run_rx(session, decoder, rx_snd, channels, rate);

	if (snd_pcm_close(tx_snd) < 0)
		abort();

	if (snd_pcm_close(rx_snd) < 0)
		abort();

	rtp_session_destroy(session);
	ortp_exit();
	ortp_global_stats_display();

	opus_encoder_destroy(encoder);
	opus_decoder_destroy(decoder);

	return r;
}
