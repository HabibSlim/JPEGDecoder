#ifndef __JPEG_CONST_H__
#define __JPEG_CONST_H__

#include <stdbool.h>

/* Propriétés des blocs */
#define BLOCK_SIZE   8
#define BLOCK_PIXELS 64

/* Marqueurs RLE */
#define EOB     0x00
#define ZRL     0xf0

/* Début\Fin du flux */
#define SOI     0xd8
#define EOI     0xd9

/* Marqueur d'application */
#define APP0    0xe0
#define APP2    0xe2

/* Start Of Frame */
#define SOF0    0xc0 // DCT Baseline
#define SOF1    0xc1
#define SOF2    0xc2 // DCT Progressive
#define SOF3    0xc3
#define SOF5    0xc5
#define SOF6    0xc6
#define SOF7    0xc7
#define SOF9    0xc9
#define SOF10   0xca
#define SOF11   0xcb
#define SOF13   0xcd
#define SOF14   0xce
#define SOF15   0xcf

/* Déclaration des tables */
#define DHT     0xc4 // Huffman
#define DQT     0xdb // Quantization

/* Start Of Scan */
#define SOS     0xda

/* Autres marqueurs */
#define JPG     0xc8
#define DAC     0xcc
#define RST0    0xd0
#define RST1    0xd1
#define RST2    0xd2
#define RST3    0xd3
#define RST4    0xd4
#define RST5    0xd5
#define RST6    0xd6
#define RST7    0xd7
#define TEM     0x01
#define DNL     0xdc
#define DRI     0xdd
#define DHP     0xde
#define EXP     0xdf
#define COM     0xfe

/* Gestion des erreurs */
#define EXIT_ERROR(callee, ...) {       \
    fprintf(stderr, "[%s.c] ", callee); \
    fprintf(stderr, ##__VA_ARGS__);     \
    fprintf(stderr, "\n");              \
    exit(-1);                           \
}

/* Flags des paramètres d'appel */
extern bool P_VERBOSE, P_BLABLA, P_PROG_STEP, P_MULTITHREAD;

/* Sortie "verbose" */
#define INFO_MSG(format, ...) do {              \
    if (P_VERBOSE)                                \
        fprintf(stderr, format, ##__VA_ARGS__); \
} while(0)                                      \

#endif