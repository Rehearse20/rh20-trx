-include .config

INSTALL ?= install

# Installation paths

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

CFLAGS += -MMD -Wall

LDLIBS_ASOUND ?= -lasound
LDLIBS_OPUS ?= -lopus
LDLIBS_ORTP ?= -lortp

LDLIBS += $(LDLIBS_ASOUND) $(LDLIBS_OPUS) $(LDLIBS_ORTP)

.PHONY:		all install dist clean

all:		rx tx trx

rx:		rx.o device.o sched.o rx_alsalib.o rx_rtplib.o

tx:		tx.o device.o sched.o tx_alsalib.o tx_rtplib.o tx_runlib.o

trx:		trx.o device.o sched.o rx_alsalib.o rx_rtplib.o tx_alsalib.o tx_runlib.o trx_rtplib.o

install:	rx tx
		$(INSTALL) -d $(DESTDIR)$(BINDIR)
		$(INSTALL) rx tx $(DESTDIR)$(BINDIR)

dist:
		mkdir -p dist
		V=$$(git describe) && \
		git archive --prefix="trx-$$V/" HEAD | \
			gzip > "dist/trx-$$V.tar.gz"

clean:
		rm -f *.o *.d tx rx trx

-include *.d
