#include <stdlib.h>
#include <stdio.h>

#include "jpeg_reader.h"
#include "jpeg_const.h"
#include "bitstream.h"
#include "huffman.h"
#include "extract_image.h"


/*
 * Fonction:  magnitude_to_value 
 * --------------------
 * convertit une valeur sous sa forme magnitude+index
 * en sa valeur entière réelle.
 *  
 *  magnitude : magnitude extraite du dernier bloc
 *  indice :    indice dans la magnitude de référence
 *
 * renvoie : la valeur d'index [indice] dans la magnitude [magnitude]
 */
static int16_t magnitude_to_value(uint8_t magnitude, uint16_t indice)
{
    uint16_t ref_indice = (1 << (magnitude-1)) - 1;
    int16_t  min_val = -((1 << magnitude) - 1);

    if (indice <= ref_indice) {
        return min_val + indice;
    } else {
        return indice;
    }
}

/*
 * Fonction:  read_coeff 
 * --------------------
 * lit un coefficient encodé en magnitude\indice dans le flux ouvert.
 *
 *  stream    : flux JPEG ouvert
 *  magnitude : magnitude de l'indice à lire
 *
 */
static int16_t read_coeff(struct bitstream *stream, uint16_t magnitude)
{
    uint32_t data;
    uint16_t indice;

    read_bitstream(stream, magnitude, &data, true); 
    indice = (uint16_t) data;

    return magnitude_to_value(magnitude, indice);
}

/*
 * Fonction:  read_DC 
 * --------------------
 * convertit une valeur sous sa forme magnitude+index
 * en sa valeur entière réelle.
 *  
 *  last_DC  : valeur du dernier coefficient DC lu (calcul différentiel)
 *  stream   : flux JPEG ouvert
 *  table_DC : table de Huffman courante pour les coefficients DC
 *
 * renvoie : le premier coefficient DC encodé huffman\magnitude dans le flux
 */
static int16_t read_DC(int16_t last_DC, struct bitstream *stream, struct huff_table *table_DC)
{
    uint16_t magnitude;

    /* Lecture du coefficient DC */
    // Magnitude :
    magnitude = next_huffman_value(table_DC, stream);

    // On ajoute la valeur du dernire coefficient DC lu
    return (read_coeff(stream, magnitude) + last_DC);
}

/*
 * Fonction:  extract_bloc 
 * --------------------
 * extrait un bloc du bitstream passé en paramètres (mode séquentiel).
 * 
 *  bloc     : pointeur vers le tableau de pixels à initialiser
 *  last_DC  : valeur du dernier coefficient DC lu (calcul différentiel)
 *  stream   : flux courant ouvert
 *  table_DC : table de Huffman courante pour les coefficients DC
 *  table_AC : table de Huffman courante pour les coefficients AC
 *
 */
static void extract_bloc(int16_t* bloc, int16_t last_DC, struct bitstream *stream, struct huff_table *table_DC, struct huff_table *table_AC)
{
    uint32_t data;
    uint16_t magnitude, n_zeros;
    int8_t c_i = 0; // Indice courant

    // On ajoute la valeur du coefficient DC calculée
    // + Valeur du dernier coefficient DC
    bloc[c_i] = read_DC(last_DC, stream, table_DC);
    c_i++;

    /* Lecture des coefficients AC */
    while (c_i < BLOCK_PIXELS) {
        data = next_huffman_value(table_AC, stream);

        data &= 0xFF;
        if (data == EOB) {
            break;
        } else if (data == ZRL) {
            c_i += 16; continue;
            /* 
               Remarque : Le code ZRL : 0xF0, code bien 16 coefficients nuls
               sans l'ajout de cette condition.
                -> F = 15, 0 donne une magnitude de 0 et un indice 0, donc 0
               On avance effectivement de 15 zéros+1 en interprétant le code "normalement",
               mais on choisit de traiter le cas à part.
            */
        }

        /*
          Byte de AC :
           0x13 -> 1 : 1xzéro
                -> 3 : magnitude -> 3 bits d'indice à lire dans magnitude 3
        */

        // Lecture du nombre de zéros :
        n_zeros = (data&0xF0) >> 4;
        // On écrit le nombre n_zeros dans le bloc
        c_i += n_zeros;

        // Lecture de la magnitude :
        magnitude = (uint16_t)(data&0x0F);

        // On ajoute la valeur du coefficient AC calculée
        bloc[c_i] = read_coeff(stream, magnitude);
        c_i++;
    }
}

/*
 * Fonction:  extract_blocs_grey
 * --------------------
 * extrait l'ensemble des blocs de luminance
 * issus du fichier JPEG ouvert (mode séquentiel).
 * 
 *  jdesc      : descripteur JPEG du fichier ouvert
 *  grey_image : pointeur vers un tableau
 *               dans lequel les blocs de luminance seront stockés
 * 
 */
void extract_blocs_grey(struct jpeg_desc *jdesc, image16_t* grey_image)
{
    /* Chargement des tables de Huffman */
    struct huff_table *table_DC, *table_AC;
    table_DC = get_huffman_table(jdesc, DC, COMP_Y);
    table_AC = get_huffman_table(jdesc, AC, COMP_Y);

    /* Dernier coefficient DC lu*/
    int16_t last_DC = 0;

    /* On charge tous les blocs de l'image dans le tableau */       
    for (size_t i=0; i<grey_image->num_blocs; i++) {
        /* On charge le dernier coefficient DC, et on le passe en paramètres*/
        extract_bloc(grey_image->y_blocs[i], last_DC, jdesc->bitstream, table_DC, table_AC);
        last_DC = grey_image->y_blocs[i][0];
    }
}

/*
 * Fonction:  extract_blocs_color
 * --------------------
 * extrait l'ensemble des blocs luminance et chrominance
 * issus du fichier JPEG ouvert (mode séquentiel).
 * 
 *  jdesc       : descripteur JPEG du fichier ouvert
 *  color_image : pointeur vers un tableau
 *                dans lequel les blocs de luminance et de chrominance seront stockés
 * 
 */
void extract_blocs_color(struct jpeg_desc *jdesc, image16_t *color_image)
{
    struct huff_table *tables_DC[2], *tables_AC[2];

    /* 
        Chargement des tables de Huffman 
        tables_*C : 0  -> table Y
                    1  -> table Ch\Cb
    */
    tables_DC[0] = get_huffman_table(jdesc, DC, COMP_Y);   tables_AC[0] = get_huffman_table(jdesc, AC, COMP_Y);
    tables_DC[1] = get_huffman_table(jdesc, DC, COMP_Cb);  tables_AC[1] = get_huffman_table(jdesc, AC, COMP_Cb);

    /* Derniers coefficients DC lus */
    int16_t last_DC[3] = {0, 0, 0};

    /* Calcul du nombre de blocs d'une composante par MCU */
    uint8_t nb_blocs_y_mcu  = jdesc->nb_cp_mcu[0],
            nb_blocs_cb_mcu = jdesc->nb_cp_mcu[1],
            nb_blocs_cr_mcu = jdesc->nb_cp_mcu[2];

    /* On lit les composantes selon l'ordre enregistré dans ordre_composants */
    size_t  offt[3] = {0, 0, 0};
    for (size_t i=0; i<jdesc->nb_mcus; i++)
    for (size_t w=0; w<3; w++) {
        switch (jdesc->ordre_composants[w]) {
            case COMP_Y:
                /* Chargement des blocs Y */
                for (size_t j=0; j<nb_blocs_y_mcu; j++) {
                    extract_bloc(color_image->y_blocs[offt[0]], last_DC[0], jdesc->bitstream, tables_DC[0], tables_AC[0]);
                    last_DC[0] = color_image->y_blocs[offt[0]][0];
                    offt[0]++;
                }
                break;
            case COMP_Cb:
                /* Chargement des blocs Cb */
                for (size_t j=0; j<nb_blocs_cb_mcu; j++) {
                    extract_bloc(color_image->cb_blocs[offt[1]], last_DC[1], jdesc->bitstream, tables_DC[1], tables_AC[1]);
                    last_DC[1] = color_image->cb_blocs[offt[1]][0];
                    offt[1]++;
                }
                break;
            default:
                /* Chargement des blocs Cr */
                for (size_t j=0; j<nb_blocs_cr_mcu; j++) {
                    extract_bloc(color_image->cr_blocs[offt[2]], last_DC[2], jdesc->bitstream, tables_DC[1], tables_AC[1]);
                    last_DC[2] = color_image->cr_blocs[offt[2]][0];
                    offt[2]++;
                }
                break;
        }
    }
}

//===============================================================================================
// Progressive JPEG : Méthodes générales

/*
 * Fonction:  read_EOB_value 
 * --------------------
 * lecture de la valeur d'un marqueur EOB (mode progressif).
 * 
 *    stream : flux courant ouvert
 *   n_zeros : bits de poids fort [SSSS]
 * 
 * renvoie le nombre de blocs EOB (zéro) à passer
 */
static uint32_t read_EOB_value(struct bitstream* stream, uint16_t n_zeros)
{
    uint32_t data;
    read_bitstream(stream, n_zeros, &data, true);
    return (data + (1 << n_zeros) - 1);
}

/*
 * Fonction:  correct_coeff 
 * --------------------
 * corrige un coefficient NZH en lisant le bit de correction
 * suivant dans le flux (mode progressif).
 * 
 *     jdesc : descripteur JPEG du fichier ouvert
 *   bloc_ci : NZH à corriger
 * 
 */
static void correct_coeff(struct jpeg_desc* jdesc, int16_t* bloc_ci)
{
    // Lecture du bit de correction
    uint32_t bit;
    read_bitstream(jdesc->bitstream, 1, &bit, true);

    // Correction du coefficient
    if (bit) *bloc_ci |= 1 << jdesc->prog_al;
}

/*
 * Fonction:  extract_first_DC_bloc 
 * --------------------
 * convertit une valeur sous sa forme magnitude+index
 * en sa valeur entière réelle (mode progressif).
 *  
 *     jdesc : descripteur JPEG du fichier ouvert
 *      bloc : bloc à initialiser
 *   last_DC : valeur du dernier coefficient DC lu (calcul différentiel)
 *  table_DC : table de Huffman courante pour les coefficients DC
 *
 * renvoie : le coefficient DC lu, avant application du bitshift (scaling)
 */
static int16_t extract_first_DC_bloc(struct jpeg_desc *jdesc, int16_t* bloc, int16_t last_DC, struct huff_table *table_DC)
{   
    int16_t new_DC;
    new_DC = read_DC(last_DC, jdesc->bitstream, table_DC);
    *bloc  = new_DC << jdesc->prog_al;
    return new_DC;
}

/*
 * Fonction:  extract_next_DC_bloc
 * --------------------
 * extrait un bloc (coefficient) DC déjà initialisé 
 * dans les scans précédents (mode progressif).
 * 
 *  jdesc : descripteur JPEG du fichier ouvert
 *   bloc : bloc à initialiser
 * 
 */
static void extract_next_DC_bloc(struct jpeg_desc *jdesc, int16_t* bloc)
{   
    uint32_t bit;

    /* On lit un bit de précision supplémentaire du coefficient DC */
    read_bitstream(jdesc->bitstream, 1, &bit, true);
    *bloc |= bit << jdesc->prog_al;
}

/*
 * Fonction:  extract_first_AC_bloc 
 * --------------------
 * extrait une première définition d'un bloc AC dans le flux (mode progressif).
 * 
 *     jdesc : descripteur JPEG du fichier ouvert
 *      bloc : bloc à initialiser
 *  table_AC : table de Huffman pour les coefficients
 * 
 * renvoie le nombre de blocs EOB (zéro) à passer
 */
static uint32_t extract_first_AC_bloc(struct jpeg_desc *jdesc, int16_t* bloc, struct huff_table *table_AC)
{
    uint32_t data;
    uint16_t magnitude, n_zeros;
    uint8_t  c_i;

    /* Lecture des coefficients AC dans la bande Ss.Se */
    c_i = jdesc->prog_ss;
    while (c_i <= jdesc->prog_se) {
        data = next_huffman_value(table_AC, jdesc->bitstream) & 0xFF;

        if (data == ZRL) {
            c_i += 16;
            continue;
        }

        // Lecture du nombre de zéros :
        n_zeros = data >> 4;

        // Lecture de la magnitude :
        magnitude = (uint16_t)(data&0x0F);

        // Bits de poids faible à 0 => EOBn
        if (magnitude == 0) {
            // On renvoie le nombre de blocs à passer
            return read_EOB_value(jdesc->bitstream, n_zeros);

            // -> Endband déjà à 0 car calloc : rien à faire
        } else {
            // On écrit le nombre n_zeros dans le bloc
            c_i += n_zeros;

            // On ajoute la valeur du coefficient AC calculée
            bloc[c_i] = read_coeff(jdesc->bitstream, magnitude) << jdesc->prog_al;
        }
        c_i++;
    }

    /* Si la bande de fréquence du bloc suivant est non nulle */
    return 0;
}

/*
 * Fonction:  extract_next_AC_bloc 
 * --------------------
 * extrait un bloc AC dans le flux (mode progressif).
 * 
 *     jdesc : descripteur JPEG du fichier ouvert
 *      bloc : bloc à initialiser
 *  table_AC : table de Huffman pour les coefficients
 * 
 * renvoie le nombre de blocs EOB (zéro) à passer
 */
static uint32_t extract_next_AC_bloc(struct jpeg_desc *jdesc, int16_t* bloc, struct huff_table *table_AC)
{
    uint32_t data, skip_num;
    uint16_t magnitude, value;
    uint8_t  c_i = jdesc->prog_ss;;
    int16_t  n_zeros;

    /* Lecture des coefficients AC dans la bande Ss.Se */
    while (c_i <= jdesc->prog_se) {
        data = next_huffman_value(table_AC, jdesc->bitstream) & 0xFF;

        // Nombre de zéros :
        n_zeros = data >> 4;
        // Magnitude :
        magnitude = (uint16_t)(data&0x0F);

        /*  Si la magnitude vaut 1, on est dans le cas
            ZH : Zero History.
            n_zeros vaut le nombre de coefficients valant 0
            entre c_i (dernier coefficient codé) ou Ss et
            le coefficient à coder, sans compter d'autres NZH
            dans la séquence.

            | Se | NZH ~ 0 | 0 | 0 | NZH | NZH | new_a_coder |
                       ^                              ^
                   c_i ~ Ss                  c_i + n_zeros + n_NZH
            -> ici n_zeros = 3

            bloc[c_i + n_zeros + n_NZH] est lu classiquement :
                        (magnitude + indice) << Al
            On applique ensuite tous les bits de correction sur les
            NZH parcourus en descendant dans la séquence.

            Dans la séquence, on a donc :
                   0 : new_a_coder : magnitude, indice
                1..n : bits de correction pour les NZH
                       entre c_i et c_i + n_zeros + n_NZH

            bit_correction = | 0 si pas de correction
                             | 1 magnitude décodée du coefficient
        */
        if (magnitude == 1) {
            /* On lit la valeur de bloc[c_i] */
            value = read_coeff(jdesc->bitstream, magnitude) << jdesc->prog_al;

            /* On applique les corrections des NZH trouvés */
            for (; n_zeros>0 || bloc[c_i]!=0; c_i++) {
                // -> NZH trouvé : correction du coefficient
                if (bloc[c_i] != 0) correct_coeff(jdesc, &bloc[c_i]);
                // -> 0 à passer
                else n_zeros-=1;
            }
            bloc[c_i] = value; c_i++;

        /* EOBn ou ZRL
           Dans les deux cas, on corrige les coefficients suivants
           comme pour le cas ZH : mais on n'ignore 
        */
        } else if (magnitude == 0) {
            // -> EOBn : position
            if (data < ZRL) {
                // Lecture du nombre de blocs à passer
                skip_num = read_EOB_value(jdesc->bitstream, n_zeros);

                /* On corrige les derniers NZH de la bande */
                for (; c_i<=jdesc->prog_se; c_i++)
                    if (bloc[c_i] != 0) correct_coeff(jdesc, &bloc[c_i]);

                return skip_num;
            // -> ZRL
            } else if (data == ZRL) {
                /* On corrige les NZH suivants dans la bande en passant 16 zéros */
                for (; n_zeros>=0; c_i++) {
                    if (bloc[c_i] != 0) correct_coeff(jdesc, &bloc[c_i]);
                    else {
                        n_zeros -= 1;
                    }
                }
            }
        }
    }

    /* Si la bande de fréquence du bloc suivant est non nulle */
    return 0;
}


//===============================================================================================
// Grayscale Progressive JPEG

/*
 * Fonction:  extract_first_DC_blocs_grey
 * --------------------
 * extrait les premiers blocs DC d'un scan
 * progressif monochrome.
 * 
 *       jdesc : descripteur JPEG du fichier ouvert
 *  prog_image : image progressive en nuance de gris
 *               à initialiser
 *  
 */
void extract_first_DC_blocs_grey(struct jpeg_desc *jdesc, image16_t* prog_image)
{
    /* Chargement des tables de Huffman */
    struct huff_table *table_DC;
    table_DC = get_huffman_table(jdesc, DC, COMP_Y);

    /* Dernier coefficient DC lu*/
    int16_t last_DC = 0;

    /* On charge tous les blocs de l'image dans le tableau */       
    for (size_t i=0; i<prog_image->num_blocs; i++) {
        /* On charge le dernier coefficient DC, et on le passe en paramètres*/
        last_DC = extract_first_DC_bloc(jdesc, &prog_image->y_blocs[i][0], last_DC, table_DC);
        // -> Les coefficients AC valent 0 temporairement
    }
}

/*
 * Fonction:  extract_next_DC_blocs_grey
 * --------------------
 * extrait les blocs DC suivants d'un scan
 * progressif monochrome.
 * 
 *       jdesc : descripteur JPEG du fichier ouvert
 *  prog_image : pointeur vers un tableau
 *               dans lequel les blocs de luminance seront stockés
 */
void extract_next_DC_blocs_grey(struct jpeg_desc *jdesc, image16_t* prog_image)
{   
    /* On charge tous les blocs de l'image dans le tableau */       
    for (size_t i=0; i<prog_image->num_blocs; i++) {
        extract_next_DC_bloc(jdesc, &prog_image->y_blocs[i][0]);
    }
}

/*
 * Fonction:  extract_first_AC_blocs_grey 
 * --------------------
 * extrait les premiers blocs AC d'une bande donnée dans le flux (mode progressif).
 * 
 *       jdesc : descripteur JPEG du fichier ouvert
 *  prog_image : structure image à initialiser
 * 
 */
void extract_first_AC_blocs_grey(struct jpeg_desc *jdesc, image16_t* prog_image)
{
    /* Chargement des tables de Huffman */
    struct huff_table *table_AC;
    table_AC = get_huffman_table(jdesc, AC, COMP_Y);

    /* On charge tous les coefficients AC dans la bande */
    uint32_t skip_num = 0;
    INFO_MSG("* starting at : "); print_offset(jdesc->bitstream);
    for (size_t i=0; i<prog_image->num_blocs; i++) {
        skip_num = extract_first_AC_bloc(jdesc, prog_image->y_blocs[i], table_AC);
        i += skip_num; // On passe les blocs EOB
    }
    INFO_MSG("* ending at : "); print_offset(jdesc->bitstream);
}

/*
 * Fonction:  extract_next_AC_blocs_grey 
 * --------------------
 * réapproxime les blocs AC d'une bande déjà apparue dans le flux (mode progressif).
 * 
 *  jdesc      : descripteur JPEG du fichier ouvert
 *  prog_image : structure image à initialiser
 * 
 */
void extract_next_AC_blocs_grey(struct jpeg_desc *jdesc, image16_t* prog_image)
{
    /* Chargement des tables de Huffman */
    struct huff_table *table_AC;
    table_AC = get_huffman_table(jdesc, AC, COMP_Y);

    uint32_t skip_num = 0;
    for (size_t i=0; i<prog_image->num_blocs; i++) {
        if (skip_num == 0) {
            skip_num = extract_next_AC_bloc(jdesc, prog_image->y_blocs[i], table_AC);
        /* Dans le cas où des blocs EOB sont à passer */
        } else {
            for (size_t c_i = jdesc->prog_ss; c_i <= jdesc->prog_se; c_i++) {
                if (prog_image->y_blocs[i][c_i] != 0) correct_coeff(jdesc, &prog_image->y_blocs[i][c_i]);
            }
            skip_num--;
        }
    }
}

//===============================================================================================
// Color Progressive JPEG

/*
 * Fonction:  extract_first_AC_blocs_color 
 * --------------------
 * extrait les premiers blocs AC d'une bande donnée dans le flux (mode progressif).
 * 
 *       jdesc : descripteur JPEG du fichier ouvert
 *  prog_image : structure image à initialiser
 * 
 */
void extract_first_AC_blocs_color(struct jpeg_desc *jdesc, image16_t* prog_image)
{
    /* Les coefficients ACs ne peuvent pas être entrelacés */
    uint8_t current_cp = jdesc->ordre_composants[0];
    int16_t** comp = (current_cp == COMP_Y) ? prog_image->y_blocs : ((current_cp == COMP_Cb) ? prog_image->cb_blocs : prog_image->cr_blocs);    

    /* Nombre de blocs de la composante */
    size_t num_blocs_cp = (current_cp == COMP_Y) ? prog_image->num_blocs : ((current_cp == COMP_Cb) ? prog_image->num_blocs_Cb : prog_image->num_blocs_Cr);

    /* Mapping des MCUs*/
    uint32_t* mcu_map = jdesc->mcu_maps[0];

    /* Chargement des tables de Huffman */
    struct huff_table *table_AC;
    table_AC = get_huffman_table(jdesc, AC, current_cp);

    uint32_t skip_num = 0;
    for (size_t i=0; i<num_blocs_cp; i++) {
        skip_num = extract_first_AC_bloc(jdesc, comp[mcu_map[i]], table_AC);
        i += skip_num; // On passe les blocs EOB
        INFO_MSG(" %zu | ", i);
    }
    INFO_MSG("\n");
}

/*
 * Fonction:  extract_next_AC_blocs_color 
 * --------------------
 * réapproxime les blocs AC d'une bande déjà apparue dans le flux (mode progressif).
 * 
 *       jdesc : descripteur JPEG du fichier ouvert
 *  prog_image : structure image à initialiser
 * 
 */
void extract_next_AC_blocs_color(struct jpeg_desc *jdesc, image16_t* prog_image)
{
    /* Les coefficients ACs ne peuvent pas être entrelacés
       -> on charge la composante courante */
    uint8_t current_cp = jdesc->ordre_composants[0];
    int16_t** comp = (current_cp == COMP_Y) ? prog_image->y_blocs : ((current_cp == COMP_Cb) ? prog_image->cb_blocs : prog_image->cr_blocs);

    /* Nombre de blocs de la composante */
    size_t num_blocs_cp = (current_cp == COMP_Y) ? prog_image->num_blocs : ((current_cp == COMP_Cb) ? prog_image->num_blocs_Cb : prog_image->num_blocs_Cr);

    /* Mapping des MCUs*/
    uint32_t* mcu_map = jdesc->mcu_maps[0];

    /* Chargement des tables de Huffman */
    struct huff_table *table_AC;
    table_AC = get_huffman_table(jdesc, AC, current_cp);

    uint32_t skip_num = 0;
    for (size_t i=0; i<num_blocs_cp; i++) {
        if (skip_num == 0) {
            skip_num = extract_next_AC_bloc(jdesc, comp[mcu_map[i]], table_AC);
        /* Dans le cas où des blocs EOB sont à passer */
        } else {
            for (size_t c_i=jdesc->prog_ss; c_i<=jdesc->prog_se; c_i++) {
                if (comp[mcu_map[i]][c_i] != 0) correct_coeff(jdesc, &comp[mcu_map[i]][c_i]);
            }
            skip_num--;
        }
        INFO_MSG(" %zu | ", i);
    }
    INFO_MSG("\n");
}

/*
 * Fonction:  extract_next_DC_blocs_color
 * --------------------
 * extrait les blocs DC suivants d'un scan
 * progressif couleur.
 * 
 *       jdesc : descripteur JPEG du fichier ouvert
 *  prog_image : pointeur vers un tableau
 *               dans lequel les blocs de luminance seront stockés
 *
 * les composantes DC peuvent être entrelacées
 */
void extract_next_DC_blocs_color(struct jpeg_desc *jdesc, image16_t* prog_image)
{   
    /* Calcul du nombre de blocs d'une composante par MCU */
    uint8_t nb_blocs_y_mcu  = jdesc->nb_cp_mcu[0],
            nb_blocs_cb_mcu = jdesc->nb_cp_mcu[1],
            nb_blocs_cr_mcu = jdesc->nb_cp_mcu[2];
    size_t  nb_mcus = jdesc->nb_mcus;

    /* On ne passe que sur les composantes de l'en-tête scannée */
    size_t  offt[3] = {0, 0, 0};
    for (size_t i=0; i<nb_mcus; i++)
    for (size_t w=0; w<jdesc->scan_nb_comp; w++) {
        switch (jdesc->ordre_composants[w]) {
            case COMP_Y:
                /* Chargement des blocs Y */
                for (size_t j=0; j<nb_blocs_y_mcu; j++) {
                    extract_next_DC_bloc(jdesc, &prog_image->y_blocs[offt[0]][0]);
                    offt[0]++;
                }
                break;
            case COMP_Cb:
                /* Chargement des blocs Cb */
                for (size_t j=0; j<nb_blocs_cb_mcu; j++) {
                    extract_next_DC_bloc(jdesc, &prog_image->cb_blocs[offt[1]][0]);
                    offt[1]++;
                }
                break;
            default:
                /* Chargement des blocs Cr */
                for (size_t j=0; j<nb_blocs_cr_mcu; j++) {
                    extract_next_DC_bloc(jdesc, &prog_image->cr_blocs[offt[2]][0]);
                    offt[2]++;
                }
                break;
        }            
    }
}

/*
 * Fonction:  extract_first_DC_blocs_color
 * --------------------
 * extrait les premiers blocs DC d'un scan
 * progressif couleur.
 * 
 *       jdesc : descripteur JPEG du fichier ouvert
 *  prog_image : image progressive en nuance de gris
 *               à initialiser
 * 
 * les composantes DC peuvent être entrelacées
 */
void extract_first_DC_blocs_color(struct jpeg_desc *jdesc, image16_t* prog_image)
{
    /* Chargement des tables de Huffman */
    struct huff_table *tables_DC[2];

    tables_DC[0] = get_huffman_table(jdesc, DC, COMP_Y);
    tables_DC[1] = get_huffman_table(jdesc, DC, COMP_Cb);

    int16_t last_DC[3] = {0, 0, 0};

    /* Calcul du nombre de blocs d'une composante par MCU */
    uint8_t nb_blocs_y_mcu  = jdesc->nb_cp_mcu[0],
            nb_blocs_cb_mcu = jdesc->nb_cp_mcu[1],
            nb_blocs_cr_mcu = jdesc->nb_cp_mcu[2];
    size_t  nb_mcus = jdesc->nb_mcus;

    /* On ne passe que sur les composantes de l'en-tête scannée */
    size_t  offt[3] = {0, 0, 0};
    for (size_t i=0; i<nb_mcus; i++)
    for (size_t w=0; w<jdesc->scan_nb_comp; w++) {
        switch (jdesc->ordre_composants[w]) {
            case COMP_Y:
                /* Chargement des blocs Y */
                for (size_t j=0; j<nb_blocs_y_mcu; j++) {
                    last_DC[w] = extract_first_DC_bloc(jdesc, &prog_image->y_blocs[offt[0]][0], last_DC[w], tables_DC[0]);
                    offt[0]++;
                }
                break;
            case COMP_Cb:
                /* Chargement des blocs Cb */
                for (size_t j=0; j<nb_blocs_cb_mcu; j++) {
                    last_DC[w] = extract_first_DC_bloc(jdesc, &prog_image->cb_blocs[offt[1]][0], last_DC[w], tables_DC[1]);
                    offt[1]++;
                }
                break;
            default:
                /* Chargement des blocs Cr */
                for (size_t j=0; j<nb_blocs_cr_mcu; j++) {
                    last_DC[w] = extract_first_DC_bloc(jdesc, &prog_image->cr_blocs[offt[2]][0], last_DC[w], tables_DC[1]);
                    offt[2]++;
                }
                break;
        }
    }
}
