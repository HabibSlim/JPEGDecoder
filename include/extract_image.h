#ifndef __EXTRACT_IMAGE_H__
#define __EXTRACT_IMAGE_H__

#include <stdint.h>
#include <stdbool.h>

#include "extract_image.h"
#include "jpeg_reader.h"
#include "bitstream.h"


/* Image avec pixels sur 16 bits signés */
typedef struct
{
    bool      color;        // true si image couleur

    /* Dimensions de l'image */
    size_t    num_blocs;    // Nombre de blocs Y
    size_t    num_blocs_Cb; // Nombre de blocs Cb
    size_t    num_blocs_Cr; // Nombre de blocs Cr
    size_t    bloc_width;   // Largeur en nombre de blocs (après décompression)
    size_t    bloc_height;  // Hauteur en nombre de blocs (après décompression)

    /* Blocs couleur contenus */
    int16_t** y_blocs;      // Blocs Y
    int16_t** cb_blocs;     // Blocs Cb
    int16_t** cr_blocs;     // Blocs Cr
} image16_t;

/* Image avec pixels sur 8 bits non signés */
typedef struct
{
    bool      color;        // true si image couleur

    /* Dimensions de l'image */
    size_t    num_blocs;    // Nombre de blocs Y
    size_t    num_blocs_Cb; // Nombre de blocs Cb
    size_t    num_blocs_Cr; // Nombre de blocs Cr
    size_t    bloc_width;   // Largeur en nombre de blocs (après décompression)
    size_t    bloc_height;  // Hauteur en nombre de blocs (après décompression)

    /* Blocs couleur contenus */
    uint8_t** y_blocs;      // Blocs Y
    uint8_t** cb_blocs;     // Blocs Cb
    uint8_t** cr_blocs;     // Blocs Cr
} image8_t;

extern image8_t* extract_image(struct jpeg_desc *jdesc);

extern uint32_t* remap_mcus(struct jpeg_desc *jdesc, image16_t* jpeg_image, uint8_t comp);

extern void free_image(image8_t* jpeg_image);

#endif