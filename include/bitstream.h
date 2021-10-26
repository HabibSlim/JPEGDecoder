#ifndef __BITSTREAM_H__
#define __BITSTREAM_H__

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>


struct bitstream {
    FILE*   filehandle;
    uint8_t bits_restants;
    uint8_t nb_bits_restants;
    uint8_t nb_bits_lus;
};

extern struct bitstream *create_bitstream(const char *filename);

extern void close_bitstream(struct bitstream *stream);

extern uint8_t read_bitstream(struct bitstream *stream,
                              uint8_t nb_bits,
                              uint32_t *dest,
                              bool discard_byte_stuffing);

extern void read_byte(struct bitstream* stream, uint8_t* bits_ptr, bool byte_stuffing);

extern void skip_bytes(struct bitstream* stream, uint32_t n_bytes);

extern void flush_stream(struct bitstream* stream);

extern void print_offset(struct bitstream *stream);

extern bool end_of_bitstream(struct bitstream *stream);

#endif