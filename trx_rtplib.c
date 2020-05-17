#include <assert.h>

#include "trx_rtplib.h"

extern unsigned int verbose;

static void timestamp_jump(RtpSession *session, void *a, void *b, void *c)
{
	if (verbose > 1)
		fputc('|', stderr);
	rtp_session_resync(session);
}

RtpSession* create_rtp_send_recv(const char *tx_addr_desc, const int tx_port,
		const char *rx_addr_desc, const int rx_port, unsigned int jitter)
{
	RtpSession *session;

	session = rtp_session_new(RTP_SESSION_SENDRECV);
	assert(session != NULL);

	rtp_session_set_scheduling_mode(session, FALSE);
	rtp_session_set_blocking_mode(session, FALSE);
	rtp_session_set_connected_mode(session, FALSE);

	if (rtp_session_set_payload_type(session, 0) != 0)
		abort();

	/* tx */
	if (rtp_session_set_remote_addr(session, tx_addr_desc, tx_port) != 0)
		abort();
	if (rtp_session_set_multicast_ttl(session, 16) != 0)
		abort();
	if (rtp_session_set_dscp(session, 40) != 0)
		abort();

	/* rx */
	if (rtp_session_set_local_addr(session, rx_addr_desc, rx_port, -1) != 0)
		abort();
	rtp_session_enable_adaptive_jitter_compensation(session, TRUE);
	rtp_session_set_jitter_compensation(session, jitter); /* ms */
	rtp_session_set_time_jump_limit(session, jitter * 16); /* ms */
	if (rtp_session_signal_connect(session, "timestamp_jump",
					timestamp_jump, 0) != 0)
	{
		abort();
	}

	return session;
}
