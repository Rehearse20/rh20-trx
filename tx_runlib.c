#include "tx_runlib.h"
#include "tx_alsalib.h"

extern unsigned int verbose;

void *run_tx(struct tx_args *tx)
{
	for (;;)
	{
		int r;

		r = send_one_frame(tx->stream, tx->channels, tx->frame,
											 tx->encoder, tx->bytes_per_frame, tx->ts_per_frame,
											 tx->nr_sessions, tx->sessions);
		if (r == -1)
			return (void *)-1;

		if (verbose > 1)
			fputc('>', stderr);
	}
}
