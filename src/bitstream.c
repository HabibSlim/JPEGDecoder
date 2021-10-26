#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "bitstream.h"
#include "jpeg_const.h"


/*
 * Fonction:  create_bitstream
 * --------------------
 * allocation de la structure et ouverture
 * du fichier.
 *
 *  filename : nom du fichier JPEG à lire
 * 
 */
struct bitstream* create_bitstream(const char *filename)
{
    /* Allocation d'une structure bitstream */
    struct bitstream* new_stream = malloc(sizeof(struct bitstream));

    /* Ouverture du fichier d'entrée */
    new_stream->filehandle = fopen(filename, "r");
    if (new_stream->filehandle == NULL) {
        EXIT_ERROR("bitstream", "Impossible de créer un flux à partir du fichier donné : {%s}.", filename);
    }

    /* Initialisation de la structure */
    flush_stream(new_stream);

    return new_stream;
}

/*
 * Fonction:  close_bitstream
 * --------------------
 * libère les structures allouées, ferme le fichier
 * ouvert.
 *
 *  stream : bitstream du fichier ouvert
 * 
 */
void close_bitstream(struct bitstream *stream)
{
    /* Fermeture du fichier */
    fclose(stream->filehandle);
    /* Libération de la structure */
    free(stream);
}

/*
 * Fonction:  ajout_a_dest
 * --------------------
 * ajouter les [nb_bits] de poids fort de [a_ajouter]
 * au buffer destination [dest], met à jour [a_ajouter]
 * en conséquence.
 *
 *  dest      : buffer destination dans lequel ajouter les bits
 *  a_ajouter : octet contenant les bits à ajouter au buffer
 *  nb_bits   : nombre de bits à ajouter au tampon
 * 
 */
static void ajout_a_dest(uint32_t *dest, uint8_t *a_ajouter, uint8_t nb_bits)
{
    uint32_t selection = (uint32_t) (*a_ajouter >> (8 - nb_bits));
    *dest = (*dest << nb_bits) | (selection);
    *a_ajouter = *a_ajouter << nb_bits;
}

/*
 * Fonction:  safe_read
 * --------------------
 * lecture vérifiée d'un octet depuis le fichier d'entrée.
 *
 *  tmp        : tampon de destination
 *  filehandle : fichier ouvert
 * 
 */
static void safe_read(uint8_t *tmp, FILE* filehandle) 
{
    if (fread(tmp, 1, 1, filehandle) != 1) {
        EXIT_ERROR("bitstream", "Fin inattendue du fichier : mauvais marqueur de fin de fichier.");
    }
}

/*
 * Fonction:  read_bitstream_rec
 * --------------------
 * wrapper récursif de read_bitstream.
 * 
 * Remarque : bits_restants a toujours les bits
 * restants positionnés sur les poids forts.
 */
static uint8_t read_bitstream_rec(struct bitstream *stream, uint8_t nb_bits, uint32_t *dest, bool discard_byte_stuffing)
{
    uint8_t tmp, test;

    if (nb_bits > 32) {
        EXIT_ERROR("bitstream", "Impossible de lire plus de 32 bits depuis bitstream.");
    } else if (nb_bits == 0) {
        return stream->nb_bits_lus;
    } else {
        if (stream->nb_bits_restants == 0) {
            safe_read(&tmp, stream->filehandle);
            if (tmp == 0xFF) {
                if (discard_byte_stuffing) {
                    safe_read(&test, stream->filehandle);
                    if (test != 0x00) {
                        uint8_t a_lire = (nb_bits >= 8) ? 8 : nb_bits;
                        stream->bits_restants = tmp;
                        ajout_a_dest(dest, &stream->bits_restants, a_lire); // nb_bits peut être supérieur à 8 !
                        stream->nb_bits_restants = 8 - a_lire;
                        stream->nb_bits_lus += a_lire;
                        fseek(stream->filehandle, -1, SEEK_CUR); // on revient après 0xff

                        return read_bitstream_rec(stream, nb_bits - a_lire, dest, discard_byte_stuffing);
                    } else {
                        stream->bits_restants = 0xFF;
                        stream->nb_bits_restants = 8;

                        return read_bitstream_rec(stream, nb_bits, dest, discard_byte_stuffing);
                    }
                } else {
                    stream->bits_restants = 0xFF;
                    stream->nb_bits_restants = 8;

                    return read_bitstream_rec(stream, nb_bits, dest, discard_byte_stuffing);
                }
            } else if (nb_bits >= 8) {
                ajout_a_dest(dest, &tmp, 8);
                stream->nb_bits_lus += 8;

                return read_bitstream_rec(stream, nb_bits - 8, dest, discard_byte_stuffing);
            } else  {
                ajout_a_dest(dest, &tmp, nb_bits);
                stream->nb_bits_lus += nb_bits;
                stream->nb_bits_restants = 8 - nb_bits;
                stream->bits_restants = tmp; // tmp deja mis a jour avec ajout_a_dest
    
                return stream->nb_bits_lus;
            }
        } else if (nb_bits <= stream->nb_bits_restants) {
            /* On a plus de bits restants que l'on a besoin d'en lire */
            ajout_a_dest(dest, &stream->bits_restants, nb_bits);
            stream->nb_bits_lus += nb_bits;
            stream->nb_bits_restants -= nb_bits;

            return stream->nb_bits_lus;
        } else {
            ajout_a_dest(dest, &stream->bits_restants, stream->nb_bits_restants);
            nb_bits -= stream->nb_bits_restants;
            stream->nb_bits_lus += stream->nb_bits_restants;
            stream->nb_bits_restants = 0;

            return read_bitstream_rec(stream, nb_bits, dest, discard_byte_stuffing);
        }
    }

    /* Jamais atteint */
    return -1;
}

/*
 * Fonction:  read_bitstream
 * --------------------
 * lit [nb_bits] depuis le flux d'entrée et charge le résultat
 * dans [dest].
 * 
 *  stream                : flux jpeg à lire
 *  nb_bits               : nombre de bits à lire
 *  dest                  : variable tampon destination
 *  discard_byte_stuffing : vaut true si les 0x00 après un 0xFF doivent
 *                          être ignorés
 * 
 * renvoie : le nombre total de bits effectivement lus.
 */
uint8_t read_bitstream(struct bitstream *stream, uint8_t nb_bits, uint32_t *dest, bool discard_byte_stuffing)
{
    *dest = 0;

    uint8_t nb_lus = read_bitstream_rec(stream, nb_bits, dest, discard_byte_stuffing);

    stream->nb_bits_lus = 0;

    return nb_lus;
}

/*
 * Fonction:  read_byte
 * --------------------
 * lecture d'un octet depuis le stream ouvert.
 * une erreur est renvoyée si le nombre de bits lus n'est
 * pas égal à 8.
 *
 *  stream        : bitstream du fichier ouvert
 *  bits_ptr      : pointeur vers la variable résultat
 *  byte_stuffing : voir read_bitstream
 * 
 */
void read_byte(struct bitstream* stream, uint8_t* bits_ptr, bool byte_stuffing)
{
    uint32_t read_data;
    uint8_t  read_bits = read_bitstream(stream, 8, &read_data, byte_stuffing);
    if (read_bits != 8) {
        EXIT_ERROR("jpeg_reader", "Impossible de lire un octet dans le flux. Nombre de bits lus : %u", read_bits);
    }
    *bits_ptr = (uint8_t) read_data;
}

/*
 * Fonction:  skip_bytes
 * --------------------
 * avancer de n_bytes octets dans le flux ouvert.
 *
 *  stream  : bitstream du fichier ouvert
 *  n_bytes : nombre d'octets à passer dans le flux
 * 
 */
void skip_bytes(struct bitstream* stream, uint32_t n_bytes)
{
    if (fseek(stream->filehandle, n_bytes, SEEK_CUR) != 0 && ferror(stream->filehandle)) {
        EXIT_ERROR("bitstream", "Fin du fichier inattendue après skip_bytes.");
    }

    /* Remise à zéro du stream */
    flush_stream(stream);
}

/*
 * Fonction:  flush_stream
 * --------------------
 * remet à zéro le flux pointé par [stream].
 * 
 *  stream : flux ouvert
 * 
 */
void flush_stream(struct bitstream* stream)
{
    stream->bits_restants = 0;
    stream->nb_bits_lus = 0;
    stream->nb_bits_restants = 0;
}

/*
 * Fonction:  print_offset
 * --------------------
 * affiche l'offset courant dans le flux ouvert
 * en octets.
 * 
 *  stream : flux ouvert
 * 
 */
void print_offset(struct bitstream *stream)
{
    if (P_VERBOSE) {
        fseek(stream->filehandle, 0L, SEEK_CUR);
        size_t sz = ftell(stream->filehandle);
        printf("Offset : %zx \n", sz);
    }
}

/*
 * Fonction:  end_of_bitstream
 * --------------------
 * verifie si le curseur de fichier est positionné sur le marqueur
 * de fin de flux JPEG.
 * 
 *  stream : flux ouvert
 * 
 */
bool end_of_bitstream(struct bitstream *stream)
{
    uint8_t lecture = 0;

    /* Lecture d'un octet pour tester la fin du fichier */
    safe_read(&lecture, stream->filehandle);
    if (lecture != EOI) {
        /* On recule le curseur d'un octet */
        fseek(stream->filehandle, -1, SEEK_CUR);
        return false;
    }

    return true;
}
