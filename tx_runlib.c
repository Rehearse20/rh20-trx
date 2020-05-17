#include "tx_runlib.h"
#include "tx_alsalib.h"

extern unsigned int verbose;

int run_tx(snd_pcm_t *snd,
		const unsigned int channels,
		const snd_pcm_uframes_t frame,
		OpusEncoder *encoder,
		const size_t bytes_per_frame,
		const unsigned int ts_per_frame,
		RtpSession *session)
{
	for (;;) {
		int r;

		r = send_one_frame(snd, channels, frame,
				encoder, bytes_per_frame, ts_per_frame,
				session);
		if (r == -1)
			return -1;

		if (verbose > 1)
			fputc('>', stderr);
	}
}
