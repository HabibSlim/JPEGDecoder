#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define M_PI 3.14159265358979323846

/* Stockage des coefficients utiles pour l'iDCT */
#define a_c 0.98078528040323044912618223613
#define b_c 0.83146961230254523707878837761
#define c_c 0.55557023301960222474283081394
#define d_c 0.19509032201612826784828486847
#define e_c 0.92387953251128675612818318939
#define f_c 0.38268343236508977172845998403
#define g_c 0.70710678118654752440084436210


// Tableau avec les coeffs pour le calcul tab_coefs[i][j] = cos((2i+1)j*M_PI/16)
static float tab_coefs[8][8] = {{1.0,  a_c,  e_c,  b_c,  g_c,  c_c,  f_c,  d_c},
                                {1.0,  b_c,  f_c, -d_c, -g_c, -a_c, -e_c, -c_c},
                                {1.0,  c_c, -f_c, -a_c, -g_c,  d_c,  e_c,  b_c},
                                {1.0,  d_c, -e_c, -c_c,  g_c,  b_c, -f_c, -a_c},
                                {1.0, -d_c, -e_c,  c_c,  g_c, -b_c, -f_c,  a_c},
                                {1.0, -c_c, -f_c,  a_c, -g_c, -d_c,  e_c, -b_c},
                                {1.0, -b_c,  f_c,  d_c, -g_c,  a_c, -e_c,  c_c},
                                {1.0, -a_c,  e_c, -b_c,  g_c, -c_c,  f_c, -d_c}};

/*
 * Fonction:  C
 * --------------------
 * fonction mathématique C définie dans le poly
 */
static float C(uint8_t lambda)
{
  float racine = 1.41421356237309504880168872420;
  return (lambda == 0) ? (1.0/racine) : 1.0;
}

/*
 * Fonction:  inv_DCT
 * --------------------
 * transformée DCT inverse sur le bloc passé en paramètres
 * passe de fréquentiel [-128, 127] à spatial [0, 255]
 *
 * renvoie : bloc de 64 entiers sur 8 bits avec DCT inverse appliquée
 */
uint8_t* inv_DCT(int16_t* bloc)
{
    uint8_t colonne, ligne;
    /* Allocation d'un nouveau bloc de valeurs sur 8 bits */
    uint8_t* nvx_bloc = malloc(64*sizeof(uint8_t));
    for (uint8_t x = 0; x < 8; x++) {
        /* Calcul de S(x,y) */
        for (uint8_t y = 0; y < 8; y++) {
            float valeur = 0.0;
            /* Application du produit sur toutes les valeurs du bloc*/
            for (uint8_t i = 0; i < 64; i++) {
                colonne = i % 8;
                ligne = (i - colonne) / 8;
                valeur += C(ligne)*C(colonne)*tab_coefs[x][ligne]*tab_coefs[y][colonne]*bloc[i];
            }
            valeur *= 0.25;
            valeur += 128.0;
            
            /* Saturation des valeurs sur 8 bits */
            if (valeur < 0.0) {
                valeur = 0.0;
            } else if (valeur > 255.0) {
                valeur = 255.0;
            }
            nvx_bloc[8*x + y] = (uint8_t) round(valeur);
        }
    }
    free(bloc);

    return nvx_bloc;
}
