#ifndef RX_RUNLIB_H
#define RX_RUNLIB_H

#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>

typedef struct {
	RtpSession *session;
	OpusDecoder *decoder;
	snd_pcm_t *snd;
	unsigned int channels;
	unsigned int rate;
} rx_args;

void *run_rx(rx_args *args);

#endif