#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "extract_image.h"
#include "jpeg_const.h"
#include "jpeg_reader.h"


/*
 * Fonction:  ycbcr_to_rgb
 * --------------------
 * transforme le pixel YCbCr en RGB.
 *
 *  Y, Cb, Cr : composantes du pixel à convertir 
 *  pixel     : image JPEG grayscale à exporter
 *
 */
static void ycbcr_to_rgb(uint8_t Y, uint8_t Cb, uint8_t Cr, uint8_t pixel[3]) {
    /* On transforme les coefficients de manière a travailler uniquement sur des entiers */
    int32_t new_values[3];
    new_values[0] = (((int32_t) Y << 17) + 183763 * (Cr - 128)) >> 17;
    new_values[1] = (((int32_t) Y << 17) - 45107 * (Cb - 128) - 93604 * (Cr - 128)) >> 17;
    new_values[2] = (((int32_t) Y << 17) + 232260 *(Cb - 128)) >> 17;

    for (size_t i = 0; i < 3; i++) {
        if (new_values[i] < 0)
            new_values[i] = 0;
        else if (new_values[i] > 255)
            new_values[i] = 255;
        pixel[i] = (uint8_t) new_values[i];
    }
}

/*
 * Fonction:  export_pgm
 * --------------------
 * exporte des blocs de luminance en un format PGM standard.
 *
 *  jpeg_image : image JPEG grayscale à exporter
 *  jdesc      : descripteur de l'image JPEG
 *  filename   : nom du fichier de sortie
 *
 */
void export_pgm(image8_t* jpeg_image, struct jpeg_desc *jdesc, const char* filename)
{
    FILE* output_ppm;
    output_ppm = fopen(filename, "w");

    /* Lecture des dimensions de l'image */
    uint16_t largeur = get_image_size(jdesc, DIR_H),
             hauteur = get_image_size(jdesc, DIR_V);

    /* Ecriture de l'en-tête en ASCII */
    fprintf(output_ppm, "P5\n");
    fprintf(output_ppm, "%u %u\n", largeur, hauteur);
    fprintf(output_ppm, "%u\n", 255); // maximum value

    /* Ecriture des données du fichier en binaire */
    size_t bloc_offset = 0,  // Décalage des index de blocs dans l'itération
           cur_offset = 0,   // Décalage permanent des index de blocs
           cur_width = 0,    // Largeur courante dans l'itération
           cur_height = 0,   // Hauteur courante dans l'itération
           total_height = 0, // Hauteur écrite totale
           restant = 0,      // Pixels restant à écrire sur la ligne
           bloc_index, pixel_index;

    while (total_height<hauteur) {    
        while (cur_width<largeur) {
            /* Index du bloc à écrire */
            bloc_index = bloc_offset+cur_offset;
            /* Index de la ligne de pixels dans le bloc */
            pixel_index = BLOCK_SIZE*cur_height;
            /* Calcul du nombre de pixels restant à écrire */
            restant = (largeur-cur_width>BLOCK_SIZE) ? BLOCK_SIZE : (largeur-cur_width);

            /* On écrit BLOCK_SIZE données depuis le bloc*/
            fwrite(&jpeg_image->y_blocs[bloc_index][pixel_index], sizeof(uint8_t), restant, output_ppm);
            cur_width += BLOCK_SIZE;
            bloc_offset++;
        }
        /* Si on a écrit une hauteur de bloc : on passe au bloc suivant */
        if (cur_height == BLOCK_SIZE-1) {
            cur_offset += bloc_offset;
            cur_height = 0;
        } else {
            cur_height++;
        }
        bloc_offset = 0;
        cur_width = 0;
        total_height++;
    }

    fclose(output_ppm);
}

/*
 * Fonction:  export_ppm
 * --------------------
 * exporte des blocs couleurs en un format PPM standard
 *
 *  jpeg_image : image JPEG grayscale à exporter
 *  jdesc      : descripteur de l'image JPEG
 *  filename   : nom du fichier de sortie
 *
 */
void export_ppm(image8_t* jpeg_image, struct jpeg_desc *jdesc, const char* filename)
{
    FILE* output_ppm;
    output_ppm = fopen(filename, "w");

    /* Lecture des dimensions de l'image en pixel */
    uint16_t largeur = get_image_size(jdesc, DIR_H),
             hauteur = get_image_size(jdesc, DIR_V);
    uint8_t pixel[3];

    /* Remapping des MCUs (sous-échantillonnage) */
    uint32_t* mcu_map = jdesc->mcu_maps[0];

    /* Ecriture de l'en-tête en ASCII */
    fprintf(output_ppm, "P6\n");
    fprintf(output_ppm, "%u %u\n", largeur, hauteur);
    fprintf(output_ppm, "%u\n", 255); // maximum value

    /* Ecriture des données du fichier en binaire */
    size_t bloc_offset = 0,  // Décalage des index de blocs dans l'itération
           cur_offset = 0,   // Décalage permanent des index de blocs
           cur_width = 0,    // Largeur courante dans l'itération
           cur_height = 0,   // Hauteur courante dans l'itération
           total_height = 0, // Hauteur écrite totale
           restant = 0,      // Pixels restant à écrire sur la ligne
           bloc_index, pixel_index;

    while (total_height<hauteur) {
        while (cur_width<largeur) {
            // Index du bloc à écrire
            bloc_index = bloc_offset+cur_offset;
            // -> Remapping de l'index
            bloc_index = mcu_map[bloc_index];
            // Index de la ligne de pixels dans le bloc
            pixel_index = BLOCK_SIZE*cur_height;
            // Calcul du nombre de pixels restant à écrire
            restant = (largeur-cur_width>BLOCK_SIZE) ? BLOCK_SIZE : (largeur-cur_width);

            for (size_t to_write=0; to_write<restant; to_write++) {
                // Conversion YCbCr -> RGB 
                ycbcr_to_rgb(jpeg_image->y_blocs[bloc_index][pixel_index+to_write],
                             jpeg_image->cb_blocs[bloc_index][pixel_index+to_write],
                             jpeg_image->cr_blocs[bloc_index][pixel_index+to_write],
                             pixel);

                for (uint8_t cmp = 0; cmp < 3; cmp++) {
                    // On écrit un pixel de chaque composante
                    fwrite(&pixel[cmp], sizeof(uint8_t), 1, output_ppm);
                }
            }
            cur_width += BLOCK_SIZE;
            bloc_offset++;
        }
        /* Si on a écrit une hauteur de bloc : on passe au bloc suivant */
        if (cur_height == BLOCK_SIZE-1) {
            cur_offset += bloc_offset;
            cur_height = 0;
        } else {
            cur_height++;
        }
        bloc_offset = 0;
        cur_width = 0;
        total_height++;
    }

    fclose(output_ppm);
}

/*
 * Fonction:  export_img
 * --------------------
 * exporte une image dans un format PPM approprié
 *
 *  jpeg_image : image JPEG à exporter, couleur ou grayscale
 *  jdesc      : descripteur de l'image JPEG
 *  filename   : nom du fichier de sortie
 *
 */
void export_img(image8_t* jpeg_image, struct jpeg_desc *jdesc, const char* filename)
{
    if (jpeg_image->color) {
        export_ppm(jpeg_image, jdesc, filename);
    } else {
        export_pgm(jpeg_image, jdesc, filename);
    }
}
