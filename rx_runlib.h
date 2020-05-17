#ifndef RX_RUNLIB_H
#define RX_RUNLIB_H

#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>

int run_rx(RtpSession *session,
		OpusDecoder *decoder,
		snd_pcm_t *snd,
		const unsigned int channels,
		const unsigned int rate);

#endif