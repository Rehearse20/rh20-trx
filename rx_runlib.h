#ifndef RX_RUNLIB_H
#define RX_RUNLIB_H

#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>

struct rx_args {
	RtpSession *session;
	OpusDecoder *decoder;
	snd_pcm_t *snd;
	unsigned int channels;
	unsigned int rate;
};

void *run_rx(struct rx_args *args);

#endif