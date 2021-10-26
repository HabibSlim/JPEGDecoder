#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "jpeg_reader.h"
#include "jpeg_const.h"

/* Tableau de correspondances entre indices dans un bloc en zig-zag et un bloc classique */
const uint8_t EQUIV_ZZ[64] =  {0,  1,  8, 16,  9,  2,  3, 10, 
                              17, 24, 32, 25, 18, 11,  4,  5, 
                              12, 19, 26, 33, 40, 48, 41, 34, 
                              27, 20, 13,  6,  7, 14, 21, 28, 
                              35, 42, 49, 56, 57, 50, 43, 36, 
                              29, 22, 15, 23, 30, 37, 44, 51, 
                              58, 59, 52, 45, 38, 31, 39, 46, 
                              53, 60, 61, 54, 47, 55, 62, 63};


/*
 * Fonction:  quantification_inverse 
 * --------------------
 * applique une quantification inverse sur un tableau
 * en sa valeur entière réelle
 * 
 *  jdesc : descripteur JPEG du fichier ouvert
 *  bloc  : bloc 8x8 lu
 *  index : index de la table de quantization
 *          | 0 : Y - Luminance
 *          | 1 : Cr\Cb - Chrominance
 *
 */
void quantification_inverse(const struct jpeg_desc* jdesc, int16_t* bloc, int index)
{
    /* Multiplication de 16bits * 8bits => 24 bits au maximum */
    int32_t res;
    uint8_t* qtable = get_quantization_table(jdesc, index);

    for (size_t i = 0; i < BLOCK_PIXELS; i++) {
        res = bloc[i]*qtable[i];

        /* Saturation sur 16 bits */
        if (res > SHRT_MAX)
            bloc[i] = SHRT_MAX;
        else if (res < SHRT_MIN)
            bloc[i] = SHRT_MIN;
        else
            bloc[i] = (int16_t)res;
    }
}


/*
 * Fonction: zig_zag  
 * --------------------
 * renvoie le tableau 1D passé en paramètre dans l'ordre zig-zag
 * 
 *  tab  : tableau à réorganiser en zig_zag
 *         | Le tableau tab est libéré à la fin de la fonction
 */
int16_t *zig_zag(int16_t *nvx_tab, int16_t *tab)
{
    /* On place les coefficients à la bonne position dans un tableau annexe */
    for (uint8_t i = 0; i < 64; i++){
        nvx_tab[EQUIV_ZZ[i]] = tab[i];
    }
    /* On recopie le tableau annexe dans le bon tableau */
    memcpy(tab, nvx_tab, BLOCK_PIXELS*sizeof(int16_t));
    return tab;
}
