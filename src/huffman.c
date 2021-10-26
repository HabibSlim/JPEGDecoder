#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "huffman.h"
#include "bitstream.h"
#include "jpeg_const.h"

#define TAILLE_MAX 16 // Longueur maximale d'un code de Huffman


/* Définition d'une structure de type liste dynamique
   utilisée pour reconstruire l'arbre de Huffman */
typedef struct {
    struct huff_table** list;
    uint8_t max_size;  // Taille courant de la liste
    uint8_t size;      // Nombre de cases réellement utilisées
} NodeList;

// Création de la liste
static NodeList* create_list(uint32_t start_size)
{
    NodeList *new_list = malloc(sizeof(NodeList));

    new_list->max_size = start_size;
    new_list->list = malloc(sizeof(struct huff_table*)*new_list->max_size);
    new_list->size = 0;

    return new_list;
}

// Destruction de la liste
static void free_list(NodeList* node_list)
{
    free(node_list->list);
    free(node_list);
}

/* Création d'un noeud dans l'arbre de Huffman */
static struct huff_table* newNode()
{ 
    struct huff_table* node = malloc(sizeof(struct huff_table)); 
    node->hasValue = false;
    node->next[0] = node->next[1] = NULL;

    return node; 
}

/*
 * Fonction:  expand_tree 
 * --------------------
 * ajoute deux fils [0 - 1] à tous les noeuds de node_list
 * -> ajoute tous les fils créés à la liste node_list
 * -> remplace les fils créés par les noeuds de node_list
 * 
 *  node_list : liste contenant toutes les feuilles de l'arbre
 *              en cours de construction
 * 
 */
static void expand_tree(NodeList** node_list)
{
    NodeList* old_list = *node_list;
    struct huff_table *left_node, *right_node, *current_node;

    /* On crée une nouvelle liste de noeuds */
    uint32_t max_size = old_list->size<<1; // Au maximum, on multiplie par deux le nombre de feuilles
    uint32_t j=0, new_size = (max_size>old_list->max_size) ? max_size : old_list->max_size;
    NodeList* new_list = create_list(new_size);

    for (size_t i=0; i<old_list->size; i++) {
        current_node = old_list->list[i];

        /* On ignore les feuilles valuées */
        if (current_node->hasValue) {
            continue;
        }

        left_node = newNode();    right_node = newNode();

        /* On donne un fils gauche et droit au noeud courant */
        current_node->next[0] = left_node;
        current_node->next[1] = right_node;

        /* On enregistre les nouveaux noeuds créés dans la nouvelle liste */
        new_list->list[j] = left_node;
        new_list->list[j+1] = right_node;
        j+=2; new_list->size+=2;
    }

    /* On remplace l'ancienne liste par la nouvelle */
    free_list(old_list);
    *node_list = new_list;
}

/*
 * Fonction:  fill_leafs 
 * --------------------
 * attribue les symboles de tab_symbole aux premières feuilles
 * de la liste leaf_nodes, dans leur ordre d'apparition (gauche->droite)
 * 
 *  leaf_nodes     : liste contenant toutes les feuilles de l'arbre
 *                   en cours de construction
 *  tab_symbole    : tableau des symboles de la profondeur courante
 *  nb_code_length : nombre de symboles à la profondeur courante
 * 
 */
static void fill_leafs(NodeList* leaf_nodes, uint8_t* tab_symbole, uint8_t nb_code_length)
{
    uint8_t j = 0;

    /* -> Toutes les feuilles sont déjà saturées : erreur */
    if (j==leaf_nodes->size) {
        EXIT_ERROR("huffman", "Impossible d'ajouter un code à l'arbre Huffman : feuilles déjà saturées.");
    }
    /* -> Pas assez de feuilles non valuées pour porter les codes : erreur */
    if ((leaf_nodes->size-j)<nb_code_length) {
        EXIT_ERROR("huffman", "Code Huffman invalide, nombre de codes incorrect.");
    }

    uint8_t set_nodes = 0;
    while(set_nodes<nb_code_length) {
        // On attribue au noeud courant sa valeur dans le tableau de symboles
        leaf_nodes->list[j]->value = tab_symbole[set_nodes];
        leaf_nodes->list[j]->hasValue = true;

        set_nodes++; j++;
    }
}

/*
 * Fonction:  make_tree 
 * --------------------
 * création d'un arbre de Huffman à partir d'un tableau de symboles,
 * et du nombre de symboles par profondeur
 * 
 *  tab_symbole    : tableau des symboles de la profondeur courante
 *  nb_code_length : nombre de symboles à la profondeur courante
 *  max_depth      : profondeur maximale de l'arbre à construire
 * 
 */
struct huff_table *make_tree(uint8_t** tab_symbole, uint8_t* nb_code_length, uint8_t max_depth)
{
    /* Création d'une racine de l'arbre de Huffman */
    struct huff_table *root = newNode();

    /* Création d'une liste de feuilles */
    NodeList* leaf_nodes = create_list(16);
    leaf_nodes->list[0] = root;
    leaf_nodes->size = 1;

    for (size_t depth=0; depth<=max_depth; depth++) {
        // On étend chaque feuille en deux nouvelles feuilles 0 | 1
        expand_tree(&leaf_nodes);

        if (nb_code_length[depth] != 0) {
            // On attribue chaque symbole à une feuille de la liste ouverte ordonnée
            fill_leafs(leaf_nodes, tab_symbole[depth], nb_code_length[depth]);
        }
    }

    /* L'arbre Huffman produit peut potentiellement ne pas être saturé
        -> Le code Huffman n'est pas optimal
       On doit quand même dans ce cas, produire un arbre.
    */
    free_list(leaf_nodes);

    /* Libération des tableaux de symboles et valeurs */
    for (size_t i=0; i<=max_depth; i++) free(tab_symbole[i]);
    free(tab_symbole);

    return root;
}

/*
 * Fonction:  load_huffman_table 
 * --------------------
 * lecture du nombre de codes par profondeur, et des symboles
 * correspondants depuis le flux d'entrée
 * 
 *  stream       : bitstream du fichier ouvert
 *  nb_byte_read : contient le nombre d'octets lus depuis le flux
 *                 à la fin de l'exécution
 * 
 */
struct huff_table *load_huffman_table(struct bitstream *stream, uint16_t *nb_byte_read) {
    uint8_t nb_code_length[TAILLE_MAX]; // nb_code_length[i] = nb d'elements de longueur i+1
    uint8_t max_depth = 0;
    uint16_t somme = 0;
    *nb_byte_read = 0;

    /* Lecture du nombre de codes par longueurs */
    for (size_t i=0; i<TAILLE_MAX; i++) {
        read_byte(stream, &(nb_code_length[i]), true); (*nb_byte_read)++;     
        somme += nb_code_length[i];
        if (nb_code_length[i] != 0) max_depth = i;
    }
    // RMQ : Vérifier si le test qui suit a du sens ?
    if (somme > 256) {
        EXIT_ERROR("huffman", "Code Huffman invalide, nombre total de codes > 256.");
    }

    /* Création du tableau de symboles 
       -> tab_symbole[i] = {liste des symboles de taille i+1} */
    uint8_t **tab_symbole = malloc(sizeof(uint8_t*)*(max_depth+1));

    /* Lecture de tous les symboles 
        -> on lit nb_code_length[i] symboles pour chaque profondeur i */
    for (size_t i=0; i<=max_depth; i++) {
        tab_symbole[i] = malloc(nb_code_length[i]*sizeof(uint8_t)); 
        for (size_t j = 0; j<nb_code_length[i]; j++) {
            read_byte(stream, &(tab_symbole[i][j]), true); (*nb_byte_read)++;
        }
    }

    INFO_MSG("-- Profondeur de l'arbre : %u\n", max_depth);

    /* On crée un arbre de Huffman à partir des tableaux tab_symbole et nb_code_length */
    return make_tree(tab_symbole, nb_code_length, max_depth);
}

/*
 * Fonction:  next_huffman_value_count 
 * --------------------
 * lecture de la première valeur de Huffman dans le flux ouvert
 * 
 *  table        : table de huffman à utiliser pour le décodage
 *  stream       : bitstream du fichier ouvert
 *  nb_bits_read : contient le nombre de bits lus depuis le flux
 *                 à la fin de l'exécution
 */
int8_t next_huffman_value_count(struct huff_table *table, struct bitstream *stream, uint8_t *nb_bits_read) {
    uint32_t bit;
    struct huff_table* huff_it = table;
    *nb_bits_read = 0;

    do {
        read_bitstream(stream, 1, &bit, true); (*nb_bits_read)++;
        huff_it = huff_it->next[bit];
    // On boucle jusqu'à atteindre une feuille valuée, ou une branche non existante
    } while(huff_it != NULL && !huff_it->hasValue);

    if (huff_it == NULL) {
        EXIT_ERROR("huffman", "Séquence de huffman invalide dans le flux.");
    }

    return huff_it->value;
}

/*
 * Fonction:  next_huffman_value 
 * --------------------
 * lecture de la première valeur de Huffman dans le flux ouvert, on ne compte pas le nombre de bits lus
 * (= copie de next_huffman_value).
 *
 *  table        : table de huffman à utiliser pour le décodage
 *  stream       : bitstream du fichier ouvert
 */
int8_t next_huffman_value(struct huff_table *table, struct bitstream *stream) {
    uint32_t bit;
    struct huff_table* huff_it = table;

    do {
        read_bitstream(stream, 1, &bit, true);
        huff_it = huff_it->next[bit];
    /* On boucle jusqu'à atteindre une feuille valuée, ou une branche non existante */
    } while(huff_it != NULL && !huff_it->hasValue);

    if (huff_it == NULL) {
        EXIT_ERROR("huffman", "Séquence de huffman invalide dans le flux.");
    }

    return huff_it->value;
}

/*
 * Fonction:  free_huffman_table 
 * --------------------
 * libère récursivement l'arbre de Huffman pointé par [table]
 * 
 */
void free_huffman_table(struct huff_table *table) {
    if (table != NULL) {
        free_huffman_table(table->next[0]);
        free_huffman_table(table->next[1]);
        free(table);
    }
}
