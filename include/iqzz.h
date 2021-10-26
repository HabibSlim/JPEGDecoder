#ifndef __IQZZ_H__
#define __IQZZ_H__

#include <stdint.h>

extern void quantification_inverse(const struct jpeg_desc* jdesc, int16_t* bloc, int index);

extern int16_t* zig_zag(int16_t *nvx_tab, int16_t *tab);

#endif