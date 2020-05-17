#ifndef TX_RUNLIB_H
#define TX_RUNLIB_H

#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>

RtpSession* create_rtp_send(const char *addr_desc, const int port);

int run_tx(snd_pcm_t *snd,
		const unsigned int channels,
		const snd_pcm_uframes_t frame,
		OpusEncoder *encoder,
		const size_t bytes_per_frame,
		const unsigned int ts_per_frame,
		RtpSession *session);

#endif