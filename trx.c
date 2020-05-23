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

#include <stdbool.h>
#include <netdb.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
//#include <regex.h> // or #include <pcre2.h>?

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
	fprintf(fd, "  -n <n>      Number of host properties passed in\n");
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
	fprintf(fd, "  -x <data>   Extended Connections (comma seperated ssrc@localport!remoteip:remoteport)\n");
	fprintf(fd, "\nExtended connections (-x) cannot be combined with explicit settings (-h, -p -s -S)\n");

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

/* The following is the size of a buffer to contain any error messages
   encountered when the regular expression is compiled. */

#define MAX_ERROR_MSG 0x1000

/* Compile the regular expression described by "regex_text" into
   "r". */
/*
static int compile_regex(regex_t *r, const char *regex_text)
{
	int status = regcomp(r, regex_text, REG_EXTENDED); // | REG_NEWLINE);
	if (status != 0)
	{
		char error_message[MAX_ERROR_MSG];
		regerror(status, r, error_message, MAX_ERROR_MSG);
		printf("Regex error compiling '%s': %s\n",
			   regex_text, error_message);
		return 1;
	}
	return 0;
}
*/

int main(int argc, char *argv[])
{
	int i, r, error, nr_hosts = 1;
	struct tx_args tx;
	struct rx_args *rx;
	pthread_t tx_thread, *rx_threads;

	/* command-line options */
	const char *device = DEFAULT_DEVICE,
			   *tx_addr = DEFAULT_ADDR,
			   *pid = NULL;
	char *extended_connections = NULL;
	unsigned int buffer = DEFAULT_BUFFER,
				 channels = DEFAULT_CHANNELS,
				 frame = DEFAULT_FRAME,
				 jitter = DEFAULT_JITTER,
				 kbps = DEFAULT_BITRATE,
				 rate = DEFAULT_RATE,
				 rx_port = DEFAULT_PORT,
				 tx_port = DEFAULT_PORT;
	uint32_t ssrc = DEFAULT_SSRC;
	bool using_extended_connections = false;
	bool using_explicit_connection = false;

	fputs(COPYRIGHT "\n", stderr);

	for (;;)
	{
		int c;

		c = getopt(argc, argv, "b:c:d:f:h:j:m:n:p:r:s:v:D:S:x:");
		if (c == -1)
			break;

		switch (c)
		{
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
			using_explicit_connection = true;
			break;
		case 'j':
			jitter = atoi(optarg);
			break;
		case 'm':
			buffer = atoi(optarg);
			break;
		case 'n':
			nr_hosts = atoi(optarg);
			break;
		case 'p':
			rx_port = atoi(optarg);
			using_explicit_connection = true;
			break;
		case 'r':
			rate = atoi(optarg);
			break;
		case 's':
			tx_port = atoi(optarg);
			using_explicit_connection = true;
			break;
		case 'S':
			ssrc = atoi(optarg);
			using_explicit_connection = true;
			break;
		case 'v':
			verbose = atoi(optarg);
			break;
		case 'D':
			pid = optarg;
			break;
		case 'x':
			extended_connections = strdup(optarg);
			using_extended_connections = true;
			break;
		default:
			usage(stderr);
			return -1;
		}
	}
	if (using_extended_connections && using_explicit_connection)
	{
		// combining explicit and extended (multiple) connection arguments is not supported
		usage(stderr);
		return -1;
	}
	if (using_extended_connections)
	{
		// determine how many hosts there are
		const char *str = extended_connections;
		nr_hosts = 1;
		while (*str)
			if (*str++ == ',')
				++nr_hosts;
	}

	rx = calloc(nr_hosts, sizeof(struct rx_args));
	rx_threads = calloc(nr_hosts, sizeof(pthread_t));
	tx.nr_sessions = nr_hosts;
	tx.sessions = calloc(nr_hosts, sizeof(RtpSession *));

	tx.encoder = opus_encoder_create(rate, channels, OPUS_APPLICATION_AUDIO,
									 &error);
	if (tx.encoder == NULL)
	{
		fprintf(stderr, "opus_encoder_create: %s\n",
				opus_strerror(error));
		return -1;
	}

	tx.bytes_per_frame = kbps * 1024 * frame / rate / 8;
	/* Follow the RFC, payload 0 has 8kHz reference rate */

	tx.ts_per_frame = frame * 8000 / rate;

	ortp_init();
	ortp_scheduler_init();
	ortp_set_log_level_mask(NULL, ORTP_WARNING | ORTP_ERROR);

	r = snd_pcm_open(&tx.snd, device, SND_PCM_STREAM_CAPTURE, 0);
	if (r < 0)
	{
		aerror("snd_pcm_open", r);
		return -1;
	}
	if (set_alsa_hw(tx.snd, rate, channels, buffer * 1000) == -1)
		return -1;
	if (set_alsa_sw(tx.snd) == -1)
		return -1;

	/* REGEX code does not work as expected yet, so that's why it is commented out.
	   fallback for now is to use strtok on each host connection string
	char *host_regex_text = "(\\d+)@(\\d+)#(\\d+\\.\\d+\\.\\d+\\.\\d+):(\\d+)"; // "(ssrc)@(localport)\\#(remoteip):(remoteport)";
	// 12@34#5.6.7.8:9
	regex_t regexCompiled;
	compile_regex(&regexCompiled, host_regex_text);
	*/

	char *rest_host_connection = NULL;
	char *host_connection = NULL;
	char *connection = NULL;

	if (using_extended_connections)
	{
		rest_host_connection = extended_connections;
		host_connection = strtok_r(extended_connections, ",", &rest_host_connection);
	}

	for (i = 0; i < nr_hosts; i++)
	{
		if (using_extended_connections)
		{
			printf("host: '%s'\n", host_connection);

			connection = strdup(host_connection);
			char *rest_host_token_split = connection;

			// this code expects the following format "(ssrc)@(localport)#(remoteip):(remoteport)"
			char *host_token_split = strtok_r(connection, "@#:", &rest_host_token_split);
			ssrc = atoi(host_token_split);
			host_token_split = strtok_r(NULL, "@#:", &rest_host_token_split);
			rx_port = atoi(host_token_split);
			host_token_split = strtok_r(NULL, "@#:", &rest_host_token_split);
			tx_addr = host_token_split;
			host_token_split = strtok_r(NULL, "@#:", &rest_host_token_split);
			tx_port = atoi(host_token_split);

			printf("decoded host connection : ssrc:%d, rx_port:%d, tx_addr:%s, tx_port:%d\n", ssrc, rx_port, tx_addr, tx_port);

			/*
			size_t maxMatches = 1;
			size_t maxGroups = 5;
			regmatch_t groupArray[maxGroups];
			unsigned int m = 0;
			char *cursor = host_connection;
			int result = regexec(&regexCompiled, cursor, maxGroups, groupArray, 0);

			if (result != 0)
			{
				// could not find anything, stop?
				printf("could not find a match in %s, %d\n", host_connection, result);
				return -1;
			}

			unsigned int g = 0;
			unsigned int offset = 0;
			for (g = 0; g < maxGroups; g++)
			{
				if (groupArray[g].rm_so == (size_t)-1)
					break; // No more groups

				if (g == 0)
					offset = groupArray[g].rm_eo;

				char cursorCopy[strlen(cursor) + 1];
				strcpy(cursorCopy, cursor);
				cursorCopy[groupArray[g].rm_eo] = 0;
				printf("Match %u, Group %u: [%2u-%2u]: %s\n",
					m, g, groupArray[g].rm_so, groupArray[g].rm_eo,
					cursorCopy + groupArray[g].rm_so);
			}
			cursor += offset;
			*/
		}

		printf("setting up decoded host connection[%d] : ssrc:%d, rx_port:%d, tx_addr:%s, tx_port:%d\n", i, ssrc, rx_port, tx_addr, tx_port);
		rx[i].decoder = opus_decoder_create(rate, channels, &error);
		if (rx[i].decoder == NULL)
		{
			fprintf(stderr, "opus_decoder_create: %s\n",
					opus_strerror(error));
			return -1;
		}

		rx[i].session = tx.sessions[i] = create_rtp_send_recv(tx_addr, tx_port, "0.0.0.0", rx_port, jitter, ssrc);
		assert(rx[i].session != NULL);

		r = snd_pcm_open(&rx[i].snd, device, SND_PCM_STREAM_PLAYBACK, 0);
		if (r < 0)
		{
			aerror("snd_pcm_open", r);
			return -1;
		}
		if (set_alsa_hw(rx[i].snd, rate, channels, buffer * 1000) == -1)
			return -1;
		if (set_alsa_sw(rx[i].snd) == -1)
			return -1;

		if (using_extended_connections)
		{
			if (connection)
				free(connection);
			host_connection = strtok_r(NULL, ",", &rest_host_connection);
		}
	}

	if (pid)
		go_daemon(pid);

	go_realtime();

	tx.channels = channels;
	tx.frame = frame;
	pthread_create(&tx_thread, NULL, (void *(*)(void *))run_tx, &tx);
	for (i = 0; i < nr_hosts; i++)
	{
		rx[i].channels = channels;
		rx[i].rate = rate;
		pthread_create(&rx_threads[i], NULL, (void *(*)(void *))run_rx, &rx[i]);
	}

	pthread_join(tx_thread, NULL);
	for (i = 0; i < nr_hosts; i++)
	{
		pthread_join(rx_threads[i], NULL);
	}

	ortp_exit();
	ortp_global_stats_display();

	if (snd_pcm_close(tx.snd) < 0)
		abort();

	opus_encoder_destroy(tx.encoder);

	for (i = 0; i < nr_hosts; i++)
	{
		if (snd_pcm_close(rx[i].snd) < 0)
			abort();

		rtp_session_destroy(rx[i].session);

		opus_decoder_destroy(rx[i].decoder);
	}

	if (extended_connections)
		free(extended_connections);

	return r;
}
