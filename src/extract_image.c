#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "process.h"
#include "export_ppm.h"
#include "extract_bloc.h"
#include "extract_image.h"
#include "jpeg_reader.h"
#include "jpeg_const.h"
#include "bitstream.h"
#include "upsampling.h"


/*
 * Fonction:  ceil_value
 * --------------------
 * renvoie la division value/divider
 * arrondie à l'excès.
 *
 */
static uint16_t ceil_value(uint16_t value, uint16_t divider)
{
    return (1 + ((value - 1) / divider));
}

/*
 * Fonction:  allocate_luminance_16 
 * --------------------
 * alloue l'espace requis pour stocker les blocs de luminance
 * dans une struct image avex pixels sur 16 bits.
 * 
 */
static void allocate_luminance_16(image16_t* new_image)
{    
    new_image->y_blocs = malloc(sizeof(int16_t*)*(new_image->num_blocs));
    for (size_t i=0; i<new_image->num_blocs; i++)
        new_image->y_blocs[i] = calloc(BLOCK_PIXELS, sizeof(int16_t));
}
// sur 8 bits
static void allocate_luminance_8(image8_t* new_image)
{
    new_image->y_blocs = malloc(sizeof(uint8_t*)*(new_image->num_blocs));
}

/*
 * Fonction:  allocate_colors_16 
 * --------------------
 * alloue l'espace requis pour stocker les blocs de luminance
 * dans une struct image avex pixels sur 16 bits.
 * 
 */
static void allocate_colors_16(image16_t* new_image)
{
    new_image->cr_blocs = malloc(sizeof(int16_t*)*(new_image->num_blocs));
    new_image->cb_blocs = malloc(sizeof(int16_t*)*(new_image->num_blocs));
    for (size_t i=0; i<new_image->num_blocs_Cr; i++)
        new_image->cr_blocs[i] = calloc(BLOCK_PIXELS, sizeof(int16_t));
    for (size_t i=0; i<new_image->num_blocs_Cb; i++)
        new_image->cb_blocs[i] = calloc(BLOCK_PIXELS, sizeof(int16_t));
}
// sur 8 bits
static void allocate_colors_8(image8_t* new_image)
{
    new_image->cr_blocs = malloc(sizeof(uint8_t*)*(new_image->num_blocs));
    new_image->cb_blocs = malloc(sizeof(uint8_t*)*(new_image->num_blocs));
}

/*
 * Fonction:  free_blocs 
 * --------------------
 * libère tous les blocs d'un tableau de blocs.
 * 
 *  num_blocs : nombre de blocs dans le tableau
 *  blocs     : tableau de blocs à libérer
 *
 */
static void free_blocs(size_t num_blocs, uint8_t** blocs)
{
    for (size_t i=0; i<num_blocs; i++) {
        free(blocs[i]);
    }
}

/*
 * Fonction:  free_image 
 * --------------------
 * libère les blocs d'une image avec pixels 8 bits.
 * 
 *  jpeg_image : image à libérer
 *
 */
void free_image(image8_t* jpeg_image)
{
    if (jpeg_image->color) {
        /* On libère les blocs de chrominance */
        free_blocs(jpeg_image->num_blocs, jpeg_image->cb_blocs);
        free_blocs(jpeg_image->num_blocs, jpeg_image->cr_blocs);
        /* On libère les tableaux de pointeurs */
        free(jpeg_image->cb_blocs);
        free(jpeg_image->cr_blocs);
    }
    /* On libère les blocs de luminance */
    free_blocs(jpeg_image->num_blocs, jpeg_image->y_blocs);
    /* Libération du tableau de pointeurs */
    free(jpeg_image->y_blocs);
    free(jpeg_image);
}

/*
 * Fonction:  free_zipped_image
 * --------------------
 * libère les blocs d'une image avec pixels 16 bits.
 * 
 *  jpeg_image : image à libérer
 *
 */
static void free_zipped_image(image16_t* jpeg_image)
{
    if (jpeg_image->color) {
        /* On libère les tableaux de pointeurs */
        free(jpeg_image->cb_blocs);
        free(jpeg_image->cr_blocs);
    }
    /* Libération du tableau de pointeurs */
    free(jpeg_image->y_blocs);
    free(jpeg_image);
}

/*
 * Fonction:  init_zip_unzip
 * --------------------
 * initialisation d'une image d'entrée et sortie pour la
 * décompression.
 * 
 *       jdesc : descripteur JPEG
 *         zip : image 16 bits compressée
 *       unzip : image 8 bits décompressée
 */
static void init_zip_unzip(struct jpeg_desc *jdesc, image16_t** zip, image8_t** unzip)
{
    /* Lecture du nombre de composantes de l'image */
    uint8_t h_MCU = get_frame_component_sampling_factor(jdesc, DIR_H, 0),
            v_MCU = get_frame_component_sampling_factor(jdesc, DIR_V, 0);

    uint16_t largeur = get_image_size(jdesc, DIR_H), 
             hauteur = get_image_size(jdesc, DIR_V);

    /* 
       Création des structures d'image nécessaires 
        -> image intermédiaire compressée : zip_image
        -> image décompressée : unzipped_image
    */
    *zip   = malloc(sizeof(image16_t));
    *unzip = malloc(sizeof(image8_t));

    /* Profil couleur de l'image */
    bool isColor = jdesc->nb_comp>1;
    (*zip)->color = isColor; (*unzip)->color = isColor;

    /* Initialisation du nombre de blocs */
    (*zip)->bloc_width  = ceil_value(largeur, BLOCK_SIZE);
    (*zip)->bloc_height = ceil_value(hauteur, BLOCK_SIZE);
    (*zip)->num_blocs   = ceil_value(largeur, h_MCU*BLOCK_SIZE)*h_MCU*ceil_value(hauteur, v_MCU*BLOCK_SIZE)*v_MCU;
    // --
    (*unzip)->bloc_width  = (*zip)->bloc_width;
    (*unzip)->bloc_height = (*zip)->bloc_height;
    (*unzip)->num_blocs   = (*zip)->num_blocs;

    /* Allocation des composantes de luminance */
    allocate_luminance_16(*zip);
    allocate_luminance_8(*unzip);

    if (isColor) {

        /* Calcul du nombre de blocs d'une composante par MCU */
        jdesc->nb_cp_mcu[0] = get_frame_component_sampling_factor(jdesc, DIR_H, 0)*get_frame_component_sampling_factor(jdesc, DIR_V, 0);
        jdesc->nb_cp_mcu[1] = get_frame_component_sampling_factor(jdesc, DIR_H, 1)*get_frame_component_sampling_factor(jdesc, DIR_V, 1);
        jdesc->nb_cp_mcu[2] = get_frame_component_sampling_factor(jdesc, DIR_H, 2)*get_frame_component_sampling_factor(jdesc, DIR_V, 2);
        jdesc->nb_mcus = (*zip)->num_blocs/jdesc->nb_cp_mcu[0];

        /* Nombre total de blocs avant sur-échantillonnage */
        (*zip)->num_blocs_Cb = jdesc->nb_mcus*jdesc->nb_cp_mcu[1];
        (*zip)->num_blocs_Cr = jdesc->nb_mcus*jdesc->nb_cp_mcu[2];
        (*unzip)->num_blocs_Cb = (*zip)->num_blocs_Cb;
        (*unzip)->num_blocs_Cr = (*zip)->num_blocs_Cr;

        /* Allocation des composantes couleur */
        allocate_colors_16(*zip);
        allocate_colors_8(*unzip);

    } else {
        (*zip)->cr_blocs = NULL;         (*zip)->cb_blocs = NULL;
        (*unzip)->cr_blocs = NULL;       (*unzip)->cb_blocs = NULL;

        (*zip)->num_blocs_Cr = 0;        (*zip)->num_blocs_Cb = 0;
        (*unzip)->num_blocs_Cr = 0;      (*unzip)->num_blocs_Cb = 0;
    }
}

/*
 * Fonction:  create_outputname_prog
 * --------------------
 * création d'une chaîne de caractères pour le fichier
 * de sortie.
 * 
 *  count : itération de décodage progressif
 *
 */
static char* create_outputname_prog(size_t count)
{
    /* On alloue une chaîne de caractères pour le nouveau nom du fichier */
    char* buffer = malloc(sizeof(char)*150);
    sprintf (buffer, "%s_%zu", "prog_out", count);

    return buffer;
}

/*
 * Fonction:  export_copy
 * --------------------
 * création d'une chaîne de caractères pour le fichier
 * de sortie.
 * 
 *  jdesc     : descripteur JPEG
 *  zip_image : image 16 bits compressée
 *  count     : itération de décodage progressif
 *
 */
static void export_copy(struct jpeg_desc *jdesc, image16_t* zip_image, size_t count)
{
    /* Création d'une copie complète de zip_image */
    image16_t* zip_copy = malloc(sizeof(image16_t));
    image8_t* unzipped_image = malloc(sizeof(image8_t));

    /* Copie des champs primaires */
    zip_copy->color        = zip_image->color;
    zip_copy->num_blocs    = zip_image->num_blocs;
    zip_copy->num_blocs_Cb = zip_image->num_blocs_Cb;
    zip_copy->num_blocs_Cr = zip_image->num_blocs_Cr;
    zip_copy->bloc_width   = zip_image->bloc_width;
    zip_copy->bloc_height  = zip_image->bloc_height;

    /* Initialisation de la sortie décompressée */
    unzipped_image->color        = zip_image->color;
    unzipped_image->num_blocs    = zip_image->num_blocs;
    unzipped_image->num_blocs_Cb = zip_image->num_blocs_Cb;
    unzipped_image->num_blocs_Cr = zip_image->num_blocs_Cr;
    unzipped_image->bloc_width   = zip_image->bloc_width;
    unzipped_image->bloc_height  = zip_image->bloc_height;

    /* Copie des blocs */
    allocate_luminance_16(zip_copy);
    allocate_luminance_8(unzipped_image);
    for (size_t i=0; i<zip_image->num_blocs; i++) {
        memcpy(zip_copy->y_blocs[i], zip_image->y_blocs[i], sizeof(int16_t)*BLOCK_PIXELS);
    }
    //
    if (zip_copy->color) {
        allocate_colors_16(zip_copy);
        allocate_colors_8(unzipped_image);
        for (size_t i=0; i<zip_image->num_blocs_Cb; i++) {
            memcpy(zip_copy->cb_blocs[i], zip_image->cb_blocs[i], sizeof(int16_t)*BLOCK_PIXELS);
        }
        for (size_t i=0; i<zip_image->num_blocs_Cr; i++) {
            memcpy(zip_copy->cr_blocs[i], zip_image->cr_blocs[i], sizeof(int16_t)*BLOCK_PIXELS);
        }
    }

    /* Décompression des blocs */    
    if (P_MULTITHREAD)
        unzip_parallel(jdesc, zip_copy, unzipped_image);
    else
        unzip_image(jdesc, zip_copy, unzipped_image);

    /* Upsampling de l'image */
    if (zip_copy->color) upsamples(unzipped_image, jdesc);

    /* Exportation PPM de l'image intermédiaire */
    char* outputname = create_outputname_prog(count);
    if (zip_copy->color) {
        export_ppm(unzipped_image, jdesc, outputname);
    } else {
        export_pgm(unzipped_image, jdesc, outputname);
    }
    free(outputname);

    /* Libération de la copie */
    free_zipped_image(zip_copy);

    /* Libération de la version décompressée */
    free_image(unzipped_image);
}

/*
 * Fonction:  remap_mcus
 * --------------------
 * renvoie un tableau associant l'index du bloc
 * dans une grille 2D "lambda" à l'index du bloc
 * réel paramétré par les dimensions des MCUs.
 *
 *  jdesc      : descripteur JPEG de l'image ouverte
 *  jpeg_image : image JPEG couleur à exporter
 *  comp       : composante sélectionnée à réordonner
 * 
 */
uint32_t* remap_mcus(struct jpeg_desc *jdesc, image16_t* jpeg_image, uint8_t comp)
{
    uint8_t h_MCU = get_frame_component_sampling_factor(jdesc, DIR_H, comp),
            v_MCU = get_frame_component_sampling_factor(jdesc, DIR_V, comp);

    size_t  blocs_largeur = jpeg_image->bloc_width,  // Largeur en nombre de blocs
            blocs_hauteur = jpeg_image->bloc_height, // Hauteur en nombre de blocs
            bloc_offset = 0,                         // Indice du bloc en tête de la ligne courante
            index = 0,                               // Index global dans le tableau à renvoyer
            curw_blocs,                              // Nombre courant de blocs écrits en largeur
            curw_mcu,                                // Nombre courant de blocs écrits en largeur dans le MCU
            curh_blocs = 0,                          // Nombre total de blocs écrits en hauteur
            curh_mcu = 0,                            // Nombre courant de blocs écrits en hauteur dans le MCU
            cur_index = 0,                           // Index du bloc en tête du MCU courant
            line_offset = h_MCU*v_MCU,               // Saut d'une ligne à l'autre
            col_offset = blocs_largeur*v_MCU;        // Saut d'une colonne à l'autre

    /* Tableau associé reliant ancien index à nouvel index */
    uint32_t *index_map = malloc(sizeof(uint32_t)*jpeg_image->num_blocs);

    while (curh_blocs < blocs_hauteur) {
        curw_blocs = 0;
        curw_mcu = 0;
        while (curw_blocs < blocs_largeur) {
            if (curw_mcu == h_MCU) {
                // On saute au MCU suivant en ligne
                cur_index += line_offset;
                curw_mcu = 0;
            }
            // Calcul de l'index du bloc courant
            index_map[index] = cur_index+curw_mcu;

            curw_blocs++;
            curw_mcu++;
            index++;
        }
        curh_blocs++; 
        curh_mcu++;

        // On saute au MCU suivant en colonne
        if (curh_mcu < v_MCU) {
            cur_index = bloc_offset+h_MCU;
        } else {
            bloc_offset += col_offset;
            cur_index += h_MCU;
            curh_mcu = 0;
        }
    }

    return index_map;
}

/*
 * Fonction:  extract_image 
 * --------------------
 * extrait l'ensemble des blocs d'une image, couleur ou non,
 * et décompresse tous ses blocs.
 * 
 *  jdesc : descripteur JPEG du fichier ouvert
 *
 * renvoie : image décompressée avec pixels sur 8 bits
 */
image8_t* extract_image(struct jpeg_desc *jdesc)
{
    image16_t* zip_image = NULL;
    image8_t* unzipped_image = NULL;

    /* Initialisation des variables source\destination */
    init_zip_unzip(jdesc, &zip_image, &unzipped_image);

    /* Remapping des MCUs */
    if (zip_image->color) for (size_t i=COMP_Y; i<COMP_NB; i++)
        jdesc->mcu_maps[i] = remap_mcus(jdesc, zip_image, i);

    /* On extrait tous les blocs : luminance et chrominances */
    size_t count = 0;
    if (jdesc->isProgressive) {
        /* Cas progressif ->
            On extrait chaque frame, en reparsant les sections SOS et DHT */
        do {
            print_offset(jdesc->bitstream);
            if (jdesc->prog_ss == 0) {
                if (jdesc->prog_ah == 0) {
                    INFO_MSG("-- First DC\n");
                    if (zip_image->color)
                        extract_first_DC_blocs_color(jdesc, zip_image);
                    else
                        extract_first_DC_blocs_grey(jdesc, zip_image);
                } else {
                    INFO_MSG("-- Next DC\n");
                    if (zip_image->color)
                        extract_next_DC_blocs_color(jdesc, zip_image);
                    else
                        extract_next_DC_blocs_grey(jdesc, zip_image);
                }
            } else if (jdesc->prog_ah == 0) {
                INFO_MSG("-- First AC\n");
                if (zip_image->color)
                    extract_first_AC_blocs_color(jdesc, zip_image);
                else
                    extract_first_AC_blocs_grey(jdesc, zip_image);
            } else {
                INFO_MSG("-- Next AC\n");
                if (zip_image->color)
                    extract_next_AC_blocs_color(jdesc, zip_image);
                else
                    extract_next_AC_blocs_grey(jdesc, zip_image);
            }

            /* Extraction d'une copie intermédiaire de l'image */
            if (P_PROG_STEP) export_copy(jdesc, zip_image, count);
            count++;
            // if (count == 5) break;
        } while (next_progressive_scan(jdesc));
    } else {
        /* Cas baseline */
        if (zip_image->color)
            extract_blocs_color(jdesc, zip_image);
        else
            extract_blocs_grey(jdesc, zip_image);
    }

    if (P_BLABLA) jpeg_blabla(jdesc, zip_image);

    /* Décompression des blocs */    
    if (P_MULTITHREAD)
        unzip_parallel(jdesc, zip_image, unzipped_image);
    else
        unzip_image(jdesc, zip_image, unzipped_image);

    /* Upsampling de l'image */
    if (zip_image->color) upsamples(unzipped_image, jdesc);

    /* Libération de l'image compressée intermédiaire */
    free_zipped_image(zip_image);

    return unzipped_image;
}
