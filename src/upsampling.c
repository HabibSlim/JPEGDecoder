#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#include "extract_image.h"
#include "jpeg_reader.h"
#include "jpeg_const.h"


/* Fonction: upsample_horizontal_ligne
 * -------------------------------------
 * upsample horizontalement une ligne (i.e dedouble la ligne)
 *
 *  ligne_b1 : pointeur sur une case d'un bloc rempli (col = 0, ligne € [0;7])
 *  ligne_b2 : pointeur sur une case d'un autre bloc  (col = 0, ligne € [0;7])
 * 
 */
static void upsample_horizontal_ligne(uint8_t* ligne_b1, uint8_t* ligne_b2)
{
    int8_t indice_courant = 15;
    uint8_t* ligne_courante;
    for (int8_t c = 7; c >= 0; c--) {
        // On fait attention a bien remplir au bon endroit
        ligne_courante = (c >= 4 ? ligne_b2 : ligne_b1);
        ligne_courante[indice_courant-- % 8] = ligne_b1[c];
        ligne_courante[indice_courant-- % 8] = ligne_b1[c];
    }
}

/* Fonction upsample_horizontal
 * -----------------------------------
 * upsample horizontalement un bloc
 * 
 *  bloc1 : bloc compressé
 *  bloc2 : bloc vide
 * 
 */
static void upsample_horizontal(uint8_t* bloc1, uint8_t* bloc2)
{
    for (int i = 0; i < 8; i++) {
        upsample_horizontal_ligne(&bloc1[i*8], &bloc2[i*8]);
    }
}

/* Fonction upsample_vertical_col
 * -------------------------------------
 * upsample verticalement une colonne (i.e dedouble la colonne)
 *
 *  col_b1 : pointeur sur une case d'un bloc rempli (ligne = 0, colonne € [0;7])
 *  col_b2 : pointeur sur une case d'un autre bloc (ligne = 0, colonne € [0;7])
 * 
 */
static void upsample_vertical_col(uint8_t* col_b1, uint8_t* col_b2)
{
    int8_t indice_courant = 15;
    uint8_t* col_courante;
    for (int8_t l = 7; l >= 0; l--) {
        // On fait attention a bien remplir au bon endroit
        col_courante = (l >= 4 ? col_b2 : col_b1);
        col_courante[((indice_courant--)%8) * 8] = col_b1[l*8];
        col_courante[((indice_courant--)%8) * 8] = col_b1[l*8];
    }
}

/* Fonction upsample_vertical
 * -----------------------------------
 * upsample horizontalement un bloc
 * 
 *  bloc1 : bloc compressé
 *  bloc2 : bloc vide
 * 
 */
static void upsample_vertical(uint8_t* bloc1, uint8_t* bloc2)
{
    for (int i = 0; i < 8; i++) {
      upsample_vertical_col(&bloc1[i], &bloc2[i]);
    }
}

/* Fonction upsample 
 * ------------------------------
 * récupère les facteur de sampling puis réalise l'upsampling adapté (pour le moment supporte uniquement l'échantillonnage
 * horizontal ou vertical ou les deux d'un coup).
 *
 *  image              : image a upsampler
 *  indice             : détermine la composante sur la quelle on travaille
 *                       | 1 -> on travaille sur Cb
 *                       | 2 -> on travaille sur Cr
 *  h_MCU, v_MCU       : facteurs d'échantillonnage du composant Y (= MCU)
 *  facteur_horizontal : facteur horizontal d'échantillonnage du composant
 *  facteur_vertical   : facteur vertical d'échantillonnage du composant
 *
 */
static void upsample(image8_t* image, uint8_t indice, int8_t h_MCU, int8_t v_MCU, int8_t facteur_horizontal, int8_t facteur_vertical)
{
    /* 
      Le dernier parametre de la fonction get_frame_component_sampling_factor correspond a l'indice de la composante de couleur
        (i.e les facteurs entre Cb et Cr peuvent etre différents, 0 = Y, 1 = Cb, 2 = Cr) 
    */
    int8_t   facteur_produit = facteur_horizontal*facteur_vertical;
    uint16_t nblocMCU = h_MCU*v_MCU;
    
    // On est dans le cas où il n'y a pas de sous echantillonnage : rien à faire
    if (facteur_produit == nblocMCU) return;

    uint8_t** blocs = (indice == 1) ? image->cb_blocs : image->cr_blocs;

    if (facteur_produit == nblocMCU/2) {
        /* On décale tous les blocs remplis à leurs position finale (i.e on laisse un emplacement derriere chaque bloc pour
           le prochain qui va apparaitre avec l'upsampling) */
        for (size_t i=image->num_blocs-1; i>0; i--) {
            if (i>=image->num_blocs/2)
                blocs[i] = malloc(sizeof(uint8_t)*64);
            else
                memcpy(blocs[2*i], blocs[i], sizeof(uint8_t)*BLOCK_PIXELS);
        }
        // Sous échantillonnage horizontal ou vertical
        if (facteur_vertical == v_MCU) {
            // Sous échantillonage horizontal
            for (size_t i=0; i<image->num_blocs; i+=2)
                upsample_horizontal(blocs[i], blocs[i+1]);
        } else {
            // Sous échantillonage vertical
            for (size_t i=0; i<image->num_blocs; i+=2)
                upsample_vertical(blocs[i], blocs[i+1]);
        }
    } else if (facteur_produit == nblocMCU/4) {
        /* On décale tous les blocs deja remplis à leurs position finale (i.e on laisse 3 emplacements derriere chaque bloc pour
           le prochain qui va apparaitre avec l'upsampling) */
        // i > 0 et pas i >= 0 car pour i = 0 on fait memcpy(blocs[0], blocs[0], 64) ON EST MALINS héhéhéhé
        for (size_t i=image->num_blocs - 1; i>0; i--) {
            if (i >= image->num_blocs/4)
                blocs[i] = malloc(sizeof(uint8_t)*BLOCK_PIXELS);
            else 
                memcpy(blocs[4*i], blocs[i], sizeof(uint8_t)*BLOCK_PIXELS);
        }
        // Sous échantillonage horizontal et vertical
        for (size_t i=0; i<image->num_blocs; i+=4) {
            // On fait d'abord un upsampling vertical entre une case et la case+2 
            upsample_vertical(blocs[i], blocs[i+2]);
            // Ensuite un horizontal entre une case et la case+1 ...
            upsample_horizontal(blocs[i], blocs[i+1]);
            // ... et la case+2 et case+3
            upsample_horizontal(blocs[i+2], blocs[i+3]);
        }
    } else {
        EXIT_ERROR("upsampling", "Taux d'échantillonnage non supporté. MCU : %ux%u, composante %u: %ux%u",
                   h_MCU, v_MCU, indice, facteur_horizontal, facteur_vertical);
    }
    if (indice == 1)
        image->cb_blocs = blocs;
    else 
        image->cr_blocs = blocs;
}

/* Fonction upsamples
 * -------------------------
 *  Appel d'upsample pour Cb et pour Cr (voir fonction upsample)
 *
 *  desc  : descripteur JPEG du fichier ouvert
 *  image : image à suréchantillonner
 */
extern void upsamples(image8_t* image, struct jpeg_desc* desc)
{
    int8_t facteur_horizontaux[3], facteur_verticaux[3];
    /* Restrictions sur les valeurs de h et de v (recopie sujet mot pour mot) */
    uint8_t somme_produit = 0;
    for (size_t i = COMP_Y; i < COMP_NB; i++) {
        facteur_horizontaux[i] = get_frame_component_sampling_factor(desc, DIR_H, i);
        facteur_verticaux[i]   = get_frame_component_sampling_factor(desc, DIR_V, i);
        somme_produit += facteur_horizontaux[i] * facteur_verticaux[i];
        /* La valeur de chaque facteur h ou v doit être comprise entre 1 et 4 */
        if (facteur_horizontaux[i] > 4 || facteur_horizontaux[i] < 1 || facteur_verticaux[i] > 4 || facteur_verticaux[i] < 1)
            EXIT_ERROR("upsampling", "Mauvais facteurs d'échantillonnage : Composante %zu en %ix%i alors que les facteurs doivent être compris entre 1 et 4.",
                        i, facteur_horizontaux[i], facteur_verticaux[i]);
        /* Les facteurs d'échantillonnage des chrominances doivent diviser parfaitement ceux de la luminance */
        if (facteur_horizontaux[0] % facteur_horizontaux[i] != 0 || facteur_verticaux[0] % facteur_verticaux[i] != 0)
            EXIT_ERROR("upsampling", "Mauvais facteurs d'échantillonnage : Les facteurs de la composante %zu ne divisent pas ceux de la luminance. (Luminance : %ix%i - Comp %zu : %ix%i)",
                        i, facteur_horizontaux[0], facteur_verticaux[0],
                        i, facteur_horizontaux[i], facteur_verticaux[i]);
    }
    /* La somme des produits hi*vi doit être inférieure ou égale à 10 */
    if (somme_produit > 10)
        EXIT_ERROR("upsampling", "Mauvais facteurs d'échantillonnage : Somme des produits hi*vi doit être inférieure ou égale à 10 (ici : %u)", somme_produit);
    
    upsample(image, 1, facteur_horizontaux[0], facteur_verticaux[0], facteur_horizontaux[1], facteur_verticaux[1]);
    upsample(image, 2, facteur_horizontaux[0], facteur_verticaux[0], facteur_horizontaux[2], facteur_verticaux[2]);
}
