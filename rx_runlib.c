#include "rx_runlib.h"
#include "rx_alsalib.h"

extern unsigned int verbose;

int run_rx(rx_args *rx)
{
	int ts = 0;

	for (;;) {
		int r, have_more;
		char buf[32768];
		void *packet;

		r = rtp_session_recv_with_ts(rx->session, (uint8_t*)buf,
				sizeof(buf), ts, &have_more);
		assert(r >= 0);
		assert(have_more == 0);
		if (r == 0) {
			packet = NULL;
			if (verbose > 1)
				fputc('#', stderr);
		} else {
			packet = buf;
			if (verbose > 1)
				fputc('.', stderr);
		}

		r = play_one_frame(packet, r, rx->decoder, rx->snd, rx->channels);
		if (r == -1)
			return -1;

		/* Follow the RFC, payload 0 has 8kHz reference rate */

		ts += r * 8000 / rx->rate;
	}
}
