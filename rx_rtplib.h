#ifndef RX_RTPLIB_H
#define RX_RTPLIB_H

#include <ortp/ortp.h>

RtpSession* create_rtp_recv(const char *addr_desc, const int port,
		unsigned int jitter);

#endif