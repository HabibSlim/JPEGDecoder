#ifndef __EXTRACT_BLOC_H__
#define __EXTRACT_BLOC_H__

#include <stdint.h>

#include "huffman.h"
#include "extract_image.h"


/* Mode séquentiel */
extern void extract_blocs_grey(struct jpeg_desc *jdesc,
                               image16_t* grey_image);

extern void extract_blocs_color(struct jpeg_desc *jdesc, 
                                image16_t* grey_image);

/* Mode progressif */
// Grayscale
extern void extract_first_DC_blocs_grey(struct jpeg_desc *jdesc,
                                        image16_t* prog_image);

extern void extract_next_DC_blocs_grey(struct jpeg_desc *jdesc,
                                       image16_t* prog_image);

extern void extract_first_AC_blocs_grey(struct jpeg_desc *jdesc,
                                        image16_t* prog_image);

extern void extract_next_AC_blocs_grey(struct jpeg_desc *jdesc,
                                       image16_t* prog_image);

// Color
extern void extract_first_DC_blocs_color(struct jpeg_desc *jdesc,
                                         image16_t* prog_image);

extern void extract_next_DC_blocs_color(struct jpeg_desc *jdesc,
                                        image16_t* prog_image);

extern void extract_first_AC_blocs_color(struct jpeg_desc *jdesc,
                                         image16_t* prog_image);

extern void extract_next_AC_blocs_color(struct jpeg_desc *jdesc,
                                        image16_t* prog_image);

#endif