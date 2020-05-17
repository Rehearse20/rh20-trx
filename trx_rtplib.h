#ifndef TRX_RTPLIB_H
#define TRX_RTPLIB_H

#include <ortp/ortp.h>

RtpSession* create_rtp_send_recv(
		const char *tx_addr_desc, const int tx_port,
		const char *rx_addr_desc, const int rx_port,
		unsigned int jitter);

#endif
