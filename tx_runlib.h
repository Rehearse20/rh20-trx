#ifndef TX_RUNLIB_H
#define TX_RUNLIB_H

#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>

struct tx_args
{
	snd_pcm_t *snd;
	unsigned int channels;
	snd_pcm_uframes_t frame;
	OpusEncoder *encoder;
	size_t bytes_per_frame;
	unsigned int ts_per_frame;
	int nr_sessions;
	RtpSession **sessions;
};

void *run_tx(struct tx_args *args);

#endif
