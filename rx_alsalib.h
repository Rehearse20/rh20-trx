#ifndef RX_ALSALIB_H
#define RX_ALSALIB_H

#include <alsa/asoundlib.h>
#include <portaudio.h>
#include <opus/opus.h>

int play_one_frame(void *packet,
									 size_t len,
									 OpusDecoder *decoder,
									 PaStream *stream,
									 const unsigned int channels);

#endif