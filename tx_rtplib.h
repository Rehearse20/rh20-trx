#ifndef TX_RTPLIB_H
#define TX_RTPLIB_H

#include <ortp/ortp.h>

RtpSession* create_rtp_send(const char *addr_desc, const int port);

#endif
