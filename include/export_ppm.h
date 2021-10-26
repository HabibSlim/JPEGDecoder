#ifndef __EXPORTPPM_H__
#define __EXPORTPPM_H__

#include <stdint.h>

#include "extract_image.h"
#include "jpeg_reader.h"


extern void export_pgm(image8_t* jpeg_image, struct jpeg_desc *jdesc, const char* filename);

extern void export_ppm(image8_t* jpeg_image, struct jpeg_desc *jdesc, const char* filename);

extern void export_img(image8_t* jpeg_image, struct jpeg_desc *jdesc, const char* filename);

#endif