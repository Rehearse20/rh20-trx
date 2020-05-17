#ifndef TX_RUNLIB_H
#define TX_RUNLIB_H

#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>

typedef struct {
	snd_pcm_t *snd;
	unsigned int channels;
	snd_pcm_uframes_t frame;
	OpusEncoder *encoder;
	size_t bytes_per_frame;
	unsigned int ts_per_frame;
	RtpSession *session;
} tx_args;

void *run_tx(tx_args *args);

#endif
