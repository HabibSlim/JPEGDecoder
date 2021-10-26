#ifndef __JPEG_DESC_H__
#define __JPEG_DESC_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "bitstream.h"


struct jpeg_desc
{
    char        filename[100];
    struct      bitstream* bitstream;

    /* Ordre d'apparition des composants */
    uint8_t     ordre_composants[3];

    /* Tables de quantification */
    // => Sur 8 bits
    uint8_t     **quantization_tables_8;
    // => Nombre de tables sur 8 bits
    uint8_t     ntables_qt_8;
    // => Sur 16 bits
    uint16_t     **quantization_tables_16;
    // => Nombre de tables sur 16 bits
    uint8_t     ntables_qt_16;

    /* Tables de Huffman */
    // => Tableaux de tables
    struct huff_table *tables_AC[2], *tables_DC[2];
    // => Nombre de tables stockées par coefficients
    uint8_t     ntables_AC, ntables_DC;

    /* Largeur\Hauteur de l'image en pixels */
    uint16_t    largeur, hauteur;
    /* Nombre de composantes */
    uint8_t     nb_comp, scan_nb_comp;
    /* Facteurs d'échantillonnage de toutes les composantes, -1 si image en gris */
    uint8_t     facts_h[3], facts_v[3];
    /* Tableaux des ID des composantes (rangés dans l'ordre classique Y - Cb - Cr), tab[1] & tab[2] == -1 si image en gris */
    uint8_t     tab_id_YCbCr[3];
    /* id de la table de quantification associée à chaque composante (tjrs dans l'ordre Y-Cb-Cr), tab[1] & tab[2] == -1 si image en gris */
    uint8_t     ids_quant[3];

    /* Tableau de correspondance entre component et l'id de la table de Huffman AC associée */
    uint8_t     ids_huff_AC[3];
    /* Tableau de correspondance entre component et l'id de la table de Huffman DC associée */
    uint8_t     ids_huff_DC[3];

    /* SOS : Partie pour progressive */
    bool        isProgressive;
    // Sequence Start, Sequence End (Sélection spectrale)
    uint8_t     prog_ss;
    uint8_t     prog_se;
    // Approximation successive : Poids fort\Poids faible
    uint8_t     prog_ah;
    uint8_t     prog_al;

    /* Nombre de blocs/MCUs */
    uint8_t     nb_cp_mcu[3];
    uint32_t    nb_mcus;

    /* Tableaux de remapping des MCUs */
    uint32_t*   mcu_maps[3];
};


enum component {
    COMP_Y,
    COMP_Cb,
    COMP_Cr,
    /* sentinelle */
    COMP_NB
};

enum direction {
    DIR_H,
    DIR_V,
    /* sentinelle */
    DIR_NB
};

enum acdc {
    DC = 0,
    AC = 1,
    /* sentinelle */
    ACDC_NB
};


extern struct jpeg_desc *read_jpeg(const char *filename);

extern void close_jpeg(struct jpeg_desc *jpeg);

extern const char *get_filename(const struct jpeg_desc *jpeg);

extern bool next_progressive_scan(struct jpeg_desc *jpeg);

extern struct bitstream *get_bitstream(const struct jpeg_desc *jpeg);

extern uint8_t get_nb_quantization_tables(const struct jpeg_desc *jpeg);

extern uint8_t *get_quantization_table(const struct jpeg_desc *jpeg,
                                       uint8_t index);

extern uint8_t get_nb_huffman_tables(const struct jpeg_desc *jpeg,
                                     enum acdc acdc);

extern struct huff_table *get_huffman_table(const struct jpeg_desc *jpeg,
                                            enum acdc acdc, uint8_t index);

extern uint16_t get_image_size(struct jpeg_desc *jpeg, enum direction dir);

extern uint8_t get_nb_components(const struct jpeg_desc *jpeg);

extern uint8_t get_frame_component_id(const struct jpeg_desc *jpeg,
                                      uint8_t frame_comp_index);

extern uint8_t get_frame_component_sampling_factor(const struct jpeg_desc *jpeg,
                                                   enum direction dir,
                                                   uint8_t frame_comp_index);

extern uint8_t get_frame_component_quant_index(const struct jpeg_desc *jpeg,
                                               uint8_t frame_comp_index);

extern uint8_t get_scan_component_id(const struct jpeg_desc *jpeg,
                                     uint8_t scan_comp_index);

extern uint8_t get_scan_component_huffman_index(const struct jpeg_desc *jpeg,
                                                enum acdc,
                                                uint8_t scan_comp_index);

#endif