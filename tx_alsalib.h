#ifndef TX_ALSALIB_H
#define TX_ALSALIB_H

#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <ortp/ortp.h>

int send_one_frame(snd_pcm_t *snd,
		const unsigned int channels,
		const snd_pcm_uframes_t samples,
		OpusEncoder *encoder,
		const size_t bytes_per_frame,
		const unsigned int ts_per_frame,
		const int nr_sessions,
		RtpSession **sessions);

#endif
