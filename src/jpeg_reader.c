#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "huffman.h"
#include "bitstream.h"
#include "jpeg_const.h"
#include "jpeg_reader.h"


/*
 * Fonction:  read_header_length
 * --------------------
 * lecture de la longueur de l'entête sur deux octets
 *
 *  stream : bitstream du fichier ouvert
 *
 */
static uint16_t read_header_length(struct bitstream* stream)
{
    uint32_t header_length = 0;
    uint8_t  read_bits = read_bitstream(stream, 16, &header_length, false);
    if (read_bits != 16) {
        EXIT_ERROR("jpeg_reader", "Impossible de lire deux octets dans le flux");
    }
    return (uint16_t) header_length-2;
}

/*
 * Fonction:  parse_app0
 * --------------------
 * parsing de la section "APP0" du fichier JPEG ouvert
 *
 *  desc : descripteur JPEG du fichier ouvert
 *
 */
static void parse_app0(struct jpeg_desc *desc)
{
    /* Taille de la section */
    uint16_t hlength = read_header_length(desc->bitstream);
    INFO_MSG("APP0 : %u octets \n", hlength);

    /* Lecture de JFIF */
    uint8_t  input;
    char*    jfif = "JFIF\0";
    for (size_t i=0; i<5; i++) {
        read_byte(desc->bitstream, &input, false);
        if (jfif[i] != input) {
            EXIT_ERROR("jpeg_reader", "APP0 : Attendu : %c, lu : %c", jfif[i], input);
        }
    }
    /* Skip des données ignorées */
    skip_bytes(desc->bitstream, hlength-5);
}

/*
 * Fonction:  parse_dqt
 * --------------------
 * parsing de la section "DQT" du fichier JPEG ouvert
 * Remarques : -> Ne supporte que les précisions de 8 bits
 *             -> Alloue des cases en trop dans le cas de redéfinitions
 *                de table : à corriger
 *
 *  desc : descripteur JPEG du fichier ouvert
 *
 */
static void parse_dqt(struct jpeg_desc *desc)
{
    /* Taille de la section */
    uint16_t hlength = read_header_length(desc->bitstream);
    INFO_MSG("DQT : %u octets \n", hlength);

    /* Lecture des tables de quantification */
    uint32_t precision, indice;
    uint8_t  length_table = 65; // Nombre d'octets nécessaires pour définir une table

    if (hlength % length_table != 0) 
        EXIT_ERROR("jpeg_reader", "DQT : Tables de tailles invalides");

    // Calcul du nombre de tables dans cette section
    uint8_t  nb_tables = hlength/length_table;
    uint8_t  new_size = 0;
    INFO_MSG( "-- Quant. tables : %u\n", nb_tables);
    if (nb_tables > 4)
        EXIT_ERROR("jpeg_reader", "DQT : Trop grand nombre de tables definies (%u au lieu de 4 max).", nb_tables);
    // => Si c'est la première fois qu'on lit une table
    if (desc->ntables_qt_8 == 0) {
        desc->ntables_qt_8 = nb_tables;
        desc->quantization_tables_8 = malloc(sizeof(uint8_t*)*nb_tables);

        // Initialisation de tous les champs à NULL
        for (size_t i=0; i<nb_tables; i++)
            desc->quantization_tables_8[i] = NULL;
    } else {
        /* On fait une réallocation du tableau
           => Conserve les anciens pointeurs vers les tables antérieures
           => Fait de la place pour les [nb_tables] nouvelles tables
           => Ne prend pas en compte la redéfinition de tables */
        // Mise à jour du nombre de tables
        new_size = desc->ntables_qt_8 + nb_tables;

        // Réallocation de l'espace
        uint8_t** new_tables = realloc(desc->quantization_tables_8, sizeof(uint8_t*)*new_size);
        if (new_tables != NULL) {
            desc->quantization_tables_8 = new_tables;

            // Initialisation des champs ajoutés à NULL
            for (size_t i=desc->ntables_qt_8; i<new_size; i++)
                desc->quantization_tables_8[i] = NULL;
            desc->ntables_qt_8 = new_size;
        } else {
            EXIT_ERROR("jpeg_reader", "DQT : Impossible de réallouer quant_tables_8");
        }
    }

    for (size_t num_table=0; num_table<nb_tables; num_table++) {
        // On lit la précision de la table (data == 0 ==> 8 bits, data == 1 ==> 16 bits)
        read_bitstream(desc->bitstream, 4, &precision, true);
        if (precision != 0 && precision != 1) {
            EXIT_ERROR("jpeg_reader", "DQT : Précision différente de 8 ou 16 bits : %x", precision);
        } else if (precision == 1) {
            EXIT_ERROR("jpeg_reader", "DQT : Précision de 16 bits non gérée");
        }
        // Indice de la table de quantification
        read_bitstream(desc->bitstream, 4, &indice, true);
        INFO_MSG( "---- Table %zu, iQ = %u\n", num_table, indice);

        // On lit tous les élements de la table, et on les charge dans la structure
        // Si l'indice est déjà utilisé : on commence par faire un free
        if (desc->quantization_tables_8[indice] != NULL) {
            free(desc->quantization_tables_8[indice]);
        }
        desc->quantization_tables_8[indice] = malloc(sizeof(uint8_t)*BLOCK_PIXELS);
        INFO_MSG( "---- Contenu : ");
        for (size_t j=0; j<BLOCK_PIXELS; j++) {
            read_byte(desc->bitstream, &desc->quantization_tables_8[indice][j], true);
            INFO_MSG( "%hhx ", desc->quantization_tables_8[indice][j]);
        }
        INFO_MSG( "\n");
    }
}

/*
 * Fonction:  parse_sof
 * --------------------
 * parsing de la section "SOF0" du fichier JPEG ouvert
 *
 *  desc : descripteur JPEG du fichier ouvert
 *
 */
static void parse_sof(struct jpeg_desc *desc)
{
    /* Taille de la section */
    uint16_t hlength = read_header_length(desc->bitstream);
    INFO_MSG( "SOF%s : %u octets \n", desc->isProgressive ? "0" : "2", hlength);

    /* Lecture de la précision */
    uint8_t precision;
    read_byte(desc->bitstream, &precision, true);
    if (precision != 8) perror("Precision différente de 8, non supportée");

    /* On lit les dimensions de l'image */
    uint32_t hauteur, largeur;
    read_bitstream(desc->bitstream, 16, &hauteur, true);
    read_bitstream(desc->bitstream, 16, &largeur, true);
    desc->hauteur = (uint16_t) hauteur;
    desc->largeur = (uint16_t) largeur;
    INFO_MSG( "-- Dimensions de l'image : %u x %u \n", desc->hauteur, desc->largeur);

    /* Lecture nombre de composantes, dimensions, ... */
    uint8_t nb_comp;
    read_byte(desc->bitstream, &nb_comp, true);
    desc->nb_comp = nb_comp;
    if (desc->nb_comp != 1 && desc->nb_comp != 3) {
        EXIT_ERROR("jpeg_reader", "SOF : Nombre de composantes différent de 1 et 3.");
    }
    INFO_MSG( "-- Nombre de composantes : %u \n", desc->nb_comp);

    if (hlength != 6+3*nb_comp) {
        EXIT_ERROR("jpeg_reader", "SOF : mauvaise taille d'en-tete SOF");
    }

    // Tableaux pour garder les ind_comp réels de YCbCr (tab[Y] == id_comp_Y)
    uint32_t fact_h, fact_v;
    for (size_t i = 0; i < nb_comp; i++) {
        // Identifiant de la composante
        read_byte(desc->bitstream, &(desc->tab_id_YCbCr[i]), true);

        // Facteurs horizontaux et verticaux
        read_bitstream(desc->bitstream, 4, &fact_h, true);
        desc->facts_h[i] = (uint8_t) fact_h;
        read_bitstream(desc->bitstream, 4, &fact_v, true);
        desc->facts_v[i] = (uint8_t) fact_v;
        INFO_MSG("-- Dimensions composante %zu : %ix%i, indice réel : %i\n", i, desc->facts_h[i], desc->facts_v[i], desc->tab_id_YCbCr[i]);
        read_byte(desc->bitstream, &(desc->ids_quant[i]), true);
    }
}

/*
 * Fonction:  parse_dht
 * --------------------
 * parsing de la section "DHT" du fichier JPEG ouvert
 *
 *  desc : descripteur JPEG du fichier ouvert
 *
 */
static void parse_dht(struct jpeg_desc *desc)
{
    /* Taille de la section */
    uint16_t hlength = read_header_length(desc->bitstream);
    INFO_MSG("DHT : %u octets \n", hlength);

    /* Lecture des tables de Huffman */
    uint32_t data;
    uint16_t nb_bytes_read, total_bytes_read = 0;
    uint8_t  AC_DC, indice;
    while (total_bytes_read < hlength) {
        read_bitstream(desc->bitstream, 3, &data, true);
        if (data != 0) {
            EXIT_ERROR("jpeg_reader", "DHT : 3 bits précédent la table Huffman différents de zéro");
        }
        // Type de la table : 0->DC, 1->AC
        read_bitstream(desc->bitstream, 1, &data, true);
        AC_DC = (uint8_t) data;

        // Indice de la table
        read_bitstream(desc->bitstream, 4, &data, true);
        indice = (uint8_t) data;

        INFO_MSG( "-- Table de huffman type %s, index %i \n", AC_DC ? "AC" : "DC", indice);
        if (AC_DC == 0) {
            if (desc->tables_DC[indice] == NULL)
                desc->ntables_DC++;
            else
                free_huffman_table(desc->tables_DC[indice]);
            desc->tables_DC[indice] = load_huffman_table(desc->bitstream, &nb_bytes_read);
        } else {
            if (desc->tables_AC[indice] == NULL)
                desc->ntables_AC++;
            else
                free_huffman_table(desc->tables_AC[indice]);
            desc->tables_AC[indice] = load_huffman_table(desc->bitstream, &nb_bytes_read);
        }

        // Mise à jour du nombre total d'octets lus
        total_bytes_read += nb_bytes_read + 1;
    }
}

/*
 * Fonction:  get_indice_comp
 * --------------------
 * Prend en paramètre un indice de composante (YCbCr)
 * entre 0 et 255 et renvoie l'indice entre 0 et 3 (i.e Y0 Cb1 Cr2)
 *
 *  desc    : descripteur JPEG du fichier ouvert
 *  id_comp : indice de composante à convertir
 *
 */
static uint8_t get_indice_comp(struct jpeg_desc *desc, uint8_t id_comp) {
    for (size_t i=COMP_Y; i<COMP_NB; i++) {
        if (id_comp == desc->tab_id_YCbCr[i])
            return i;
    }
    EXIT_ERROR("jpeg_reader", "get_indice_comp : ID de composant incorrect");
}

/*
 * Fonction:  parse_sos
 * --------------------
 * parsing de la section "SOS" du fichier JPEG ouvert
 *
 *  desc : descripteur JPEG du fichier ouvert
 *
 */
static void parse_sos(struct jpeg_desc *desc)
{
    uint16_t hlength = read_header_length(desc->bitstream);
    INFO_MSG("SOS : %u octets \n", hlength);

    /* Lecture nombre de composantes */
    uint8_t nb_comp;
    read_byte(desc->bitstream, &nb_comp, true);
    if (nb_comp != desc->nb_comp && !desc->isProgressive) {
        EXIT_ERROR("jpeg_reader", "SOS : Nombre de composantes incohérent avec le nombre annoncé en section SOFx");
    }
    desc->scan_nb_comp = nb_comp;

    if (hlength != 2*nb_comp + 4)
        EXIT_ERROR("jpeg_reader", "SOS : Mauvaise taille de section.");

    /* Lecture indices table Huffman */
    uint8_t id_comp, id_reel;
    uint32_t id_huff_DC, id_huff_AC, data;
    for (size_t i=0; i<nb_comp; i++) {
        INFO_MSG( "-- Scan composante %zu \n", i);
        read_byte(desc->bitstream, &id_comp, true);
        // id_reel = id component (0Y 1Cb 2Cr)
        id_reel = get_indice_comp(desc, id_comp);
        /* On lit l'ordre d'apparition des composants dans le flux */
        desc->ordre_composants[i] = id_reel;
        INFO_MSG( "---- associé au composant %i (%i)\n", id_comp, id_reel);
        read_bitstream(desc->bitstream, 4, &id_huff_DC, true);
        desc->ids_huff_DC[id_reel] = (uint8_t) id_huff_DC;
        INFO_MSG( "---- associé à la HT DC d'index %hhu\n", (uint8_t) id_huff_DC);
        read_bitstream(desc->bitstream, 4, &id_huff_AC, true);
        desc->ids_huff_AC[id_reel] = (uint8_t) id_huff_AC;
        INFO_MSG( "---- associé à la HT AC d'index %hhu\n", (uint8_t) id_huff_AC);
    }
    INFO_MSG( "---- ordre d'apparition des composantes : ");
    for (size_t i=0; i<nb_comp; i++) INFO_MSG( "%u ", desc->ordre_composants[i]);
    INFO_MSG( "\n");
 
    /* Lecture d'indices pour le JPEG progressif */
    if (desc->isProgressive) {
        read_byte(desc->bitstream, &desc->prog_ss, true);
        read_byte(desc->bitstream, &desc->prog_se, true);
        read_bitstream(desc->bitstream, 4, &data, true);
        desc->prog_ah = (uint8_t) data;
        read_bitstream(desc->bitstream, 4, &data, true);
        desc->prog_al = (uint8_t) data;
        INFO_MSG( "-- Progressive parameters \n");
        INFO_MSG( "--- Ss_Se : %hhu .. %hhu | Ah_Al = %x_%x\n", desc->prog_ss, desc->prog_se, desc->prog_ah, desc->prog_al);
    } else {
        skip_bytes(desc->bitstream, 3);
    }
}

/*
 * Fonction:  parse_com
 * --------------------
 * parsing de la section "COM" du fichier JPEG ouvert
 *
 *  desc : descripteur JPEG du fichier ouvert
 *
 */
static void parse_com(struct jpeg_desc *desc)
{
    uint16_t hlength = read_header_length(desc->bitstream);
    INFO_MSG("COM : %u octets \n", hlength);
    /* On passe toute la section : inutile */
    skip_bytes(desc->bitstream, hlength);
}

/*
 * Fonction:  create_jpeg_desc
 * --------------------
 * allocation des champs d'un descripteur JPEG
 *
 *  stream   : bitstream du fichier ouvert
 *  filename : nom du fichier JPEG
 *
 */
static struct jpeg_desc* create_jpeg_desc(struct bitstream* stream, const char *filename)
{
    struct jpeg_desc *desc = malloc(sizeof(struct jpeg_desc));

    /* Bitstream et copie du nom du fichier */
    desc->bitstream = stream;
    strcpy(desc->filename, filename);

    /* Pointeurs sur les tables de quantization */
    desc->quantization_tables_8  = NULL;
    desc->quantization_tables_16 = NULL;

    /* Nombre de composantes */
    desc->nb_comp = 0;
    desc->scan_nb_comp = 0;

    /* Nombres de tables de huffman et quantization*/
    desc->ntables_qt_8  = 0;
    desc->ntables_qt_16 = 0;
    desc->ntables_AC = 0;
    desc->ntables_DC = 0;

    /* Champs relatifs au mode progressif */
    desc->prog_ss = 0;
    desc->prog_se = 0;
    desc->prog_ah = 0;
    desc->prog_al = 0;

    /* Initialisation des tableaux de pointeurs à NULL */
    for (size_t i=0; i<2; i++) {
        desc->tables_AC[i] = NULL;
        desc->tables_DC[i] = NULL;
    }

    /* Tableaux de remapping des MCUs */
    for (size_t i=0; i<3; i++) {
        desc->mcu_maps[i] = NULL;
    }

    return desc;
}

/*
 * Fonction:  read_jpeg
 * --------------------
 * fonction principale : parsing d'un fichier JPEG
 * modification du descripteur jpeg_desc en conséquence
 *
 *  filename : nom du fichier JPEG à décoder
 *
 */
struct jpeg_desc *read_jpeg(const char *filename)
{
    /* Création du bitstream */
    struct bitstream* stream = create_bitstream(filename);
    if (stream == NULL) {
        EXIT_ERROR("jpeg_reader", "read_jpeg : Impossible de lire le fichier source : %s", filename);
    }

    /* Création de la structure */
    struct jpeg_desc *desc = create_jpeg_desc(stream, filename);

    /* Parsing de l'entête */
    uint8_t byte;
    while (true) {
        read_byte(stream, &byte, false);
        if (byte != 0xff) {
            // Erreur : on devrait toujours avoir un marqueur ici
            EXIT_ERROR("jpeg_reader", "read_jpeg : 0xff attendu - lu : 0x%hhx", byte);
        }
        // Lecture de l'identifiant de marqueur
        read_byte(stream, &byte, false);
        switch(byte) {
            /* Début de l'image */
            case SOI:
                INFO_MSG("SOI found \n");
                continue;
            case COM:
                parse_com(desc);
                break;
            case APP0:
                parse_app0(desc);
                break;
            case DQT:
                parse_dqt(desc);
                break;
            case SOF0:
                desc->isProgressive = false;
                parse_sof(desc);
                break;
            case SOF2:
                desc->isProgressive = true;
                parse_sof(desc);
                break;
            case DHT:
                parse_dht(desc);
                break;
            case SOS:
                parse_sos(desc);
                goto end_parse;
            /* Fin de fichier : impossible */
            case EOI:
                EXIT_ERROR("jpeg_reader", "Fin de fichier trouvée en phase de lecture du header");
            default:
                EXIT_ERROR("jpeg_reader", "Type de header non pris en charge : 0xff%hhx", byte);
        }
    }

    // On ne ferme pas le fichier ici, fermé dans close_jpeg appelé par la fonction principale 
    end_parse:
    return desc;
}

/*
 * Fonction:  next_progressive_scan
 * --------------------
 * lecture des entêtes suivantes dans le flux
 * jusqu'au premier scan (Mode progressif)
 * 
 *  jdesc : descripteur JPEG du fichier ouvert
 *
 */
bool next_progressive_scan(struct jpeg_desc *jdesc)
{
    INFO_MSG( "-> Finding next scan...\n");
    flush_stream(jdesc->bitstream);
    print_offset(jdesc->bitstream);

    uint8_t byte;
    while (true) {    
        read_byte(jdesc->bitstream, &byte, false);
        if (byte != 0xff) {
            // Erreur : on devrait toujours avoir un marqueur ici
            EXIT_ERROR("jpeg_reader", "read_jpeg : 0xff attendu - lu : 0x%hhx", byte);
        }

        /* Lecture de l'identifiant de marqueur */
        read_byte(jdesc->bitstream, &byte, false);
        switch(byte) {
            // Reéfinition des tables de Huffman
            case DHT:
                parse_dht(jdesc);
                break;
            // Début du scan
            case SOS:
                parse_sos(jdesc);
                return true;
            // Fin de fichier
            case EOI:
                return false;
            default:
                // Marqueur inattendu
                EXIT_ERROR("jpeg_reader", "read_jpeg : Marqueur inattendu : 0xff%hhx", byte);
        }
    }
    return false;
}

/*
 * Fonction:  close_jpeg
 * --------------------
 * fermeture d'un descripteur JPEG ouvert.
 * désallocation des champs, fermeture du bitstream.
 * 
 *  jdesc : descripteur JPEG du fichier ouvert
 * 
 */
void close_jpeg(struct jpeg_desc *jdesc) {
    /* Fermeture du bitstream */
    close_bitstream(jdesc->bitstream);

    /* Libération des tables de Huffman */
    for (size_t i=0; i<jdesc->ntables_AC; i++) free_huffman_table(jdesc->tables_AC[i]);
    for (size_t i=0; i<jdesc->ntables_DC; i++) free_huffman_table(jdesc->tables_DC[i]);

    /* Libération des tables de quantification */
    if (jdesc->ntables_qt_8>0) {
        for (size_t i=0; i<jdesc->ntables_qt_8;  i++) free(jdesc->quantization_tables_8[i]);
        free(jdesc->quantization_tables_8);
    }
    if (jdesc->ntables_qt_16>0) {
        for (size_t i=0; i<jdesc->ntables_qt_16; i++) free(jdesc->quantization_tables_16[i]);
        free(jdesc->quantization_tables_16);
    }

    /* Libération des mappings MCUs */
    for (size_t i=COMP_Y; i<COMP_NB; i++) if (jdesc->mcu_maps[i] != NULL)
        free(jdesc->mcu_maps[i]);

    free(jdesc);
}

/*
 * Getters : nom du fichier, bitstream
 */
const char* get_filename(const struct jpeg_desc *jdesc) {
    return jdesc->filename;
}
struct bitstream *get_bitstream(const struct jpeg_desc *jdesc)
{
    return jdesc->bitstream;
}

/*
 * Getters : données du champ DQT
 */
uint8_t get_nb_quantization_tables(const struct jpeg_desc *jdesc)
{
    return jdesc->ntables_qt_8;
}
uint8_t *get_quantization_table(const struct jpeg_desc *jdesc, uint8_t index)
{
    return jdesc->quantization_tables_8[index];
}

/*
 * Getters : données du champ DHT
 */
uint8_t get_nb_huffman_tables(const struct jpeg_desc *jdesc, enum acdc acdc)
{
    if (acdc == AC) {
        return jdesc->ntables_AC;
    } else {
        return jdesc->ntables_DC;
    }
}
struct huff_table *get_huffman_table(const struct jpeg_desc *jdesc, enum acdc acdc, uint8_t index)
{
    uint8_t n_index;
    if (acdc == AC) {
        n_index = jdesc->ids_huff_AC[index];
        return jdesc->tables_AC[n_index];
    } else {
        n_index = jdesc->ids_huff_DC[index];
        return jdesc->tables_DC[n_index];
    }
}

/*
 * Getters : données du champ SOF0
 */
uint16_t get_image_size(struct jpeg_desc *jdesc, enum direction dir) {
    return (dir == DIR_H) ? jdesc->largeur : jdesc->hauteur;
}
uint8_t get_nb_components(const struct jpeg_desc *jdesc) {
    return jdesc->nb_comp;
}
uint8_t get_frame_component_id(const struct jpeg_desc *jdesc, uint8_t frame_comp_index) {
    return jdesc->tab_id_YCbCr[frame_comp_index];
}
uint8_t get_frame_component_sampling_factor(const struct jpeg_desc *jdesc, enum direction dir,  uint8_t frame_comp_index) {
    return (dir == DIR_H) ? jdesc->facts_h[frame_comp_index] : jdesc->facts_v[frame_comp_index];
}
uint8_t get_frame_component_quant_index(const struct jpeg_desc *jdesc, uint8_t frame_comp_index) {
    return jdesc->ids_quant[frame_comp_index];
}

/*
 * Getters : données du champ SOS
 */
uint8_t get_scan_component_id(const struct jpeg_desc *jdesc, uint8_t scan_comp_index) {
    return jdesc->tab_id_YCbCr[scan_comp_index];
}
uint8_t get_scan_component_huffman_index(const struct jpeg_desc *jdesc, enum acdc type, uint8_t scan_comp_index) {
    return (type == AC) ? jdesc->ids_huff_AC[scan_comp_index] : jdesc->ids_huff_DC[scan_comp_index];
}
