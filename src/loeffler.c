#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "jpeg_const.h"


static float tab_cos[9] = {1.0,
			               0.98078528040323044912618223613,
			               0.92387953251128675612818318939,
			               0.83146961230254523707878837761,
			               0.70170678118654752440084436210,
			               0.55557023301960222474283081394,
			               0.38268343236508977172845998403,
			               0.19509032201612826784828486847,
			               0.0};
static float racine = 1.41421356237309504880168872420;

/*
* Fonction:  loeffler_butterfly_inv
* --------------------
* implémente l'inverse du module butterfly de loeffler
*
*  i_0 : flottant sur l'entrée I0
*  i_1 : flottant sur l'entrée I1
*  o_0 : flottant sur la sortie O0
*  o_1 : flottant sur la sortie 01
*
*/
static void loeffler_butterfly_inv(float i_0, float i_1, float *o_0, float *o_1)
// i_1 doit être celui sur les pointilles et o_1 est la sortie côté pointillés
{
	*o_0 = (i_0 + i_1)/2;
	*o_1 = (i_0 - i_1)/2;
}

/*
* Fonction:  loeffler_rotation_inv
* --------------------
* implémente l'inverse du module de rotation de loeffler
*
*  i_0 			: flottant sur l'entrée I0
*  i_1 			: flottant sur l'entrée I1
*  k   			: facteur k de la rotation de paramètres k  et Cn
*  num_coef : facteur n de la rotation de paramètres k et Cn
*  o_0 			: flottant sur la sortie O0
*  o_1 			: flottant sur la sortie 01
*
*/
static void loeffler_rotation_inv(float i_0, float i_1, float k, uint8_t num_coef, float *o_0, float *o_1)
{
	*o_0 = 1.0/k * (i_0 *  tab_cos[num_coef] - i_1 * tab_cos[8 - num_coef]);
	*o_1 = 1.0/k * (i_1 *  tab_cos[num_coef] + i_0 * tab_cos[8 - num_coef]);
}

/*
* Fonction:  loeffler_rond_inv
* --------------------
* implémente l'inverse du module rond de loeffler
*
*  i_0 : flottant sur l'entrée I0
*  o_0 : flottant sur la sortie O0
*
*/
static void loeffler_rond_inv(float i_0, float *o_0)
{
	*o_0 = i_0/racine;
}


/*
* Fonction:  loeffler_inv
* --------------------
* suit l'algorithme de loeffler pour l'iDCT 1D avec 8 points
*
*  tableau_coeffs : vecteur avec les 8 coefficients pour loeffler
*  nouveau_tab    : tableau local pour éviter les effets de bord des opérations de loeffler
*
*/
static void loeffler_inv(float tableau_coeffs[8])
{
	float nouveau_tab[8];

	// etape 4

	loeffler_butterfly_inv(tableau_coeffs[1], tableau_coeffs[7], &nouveau_tab[7], &nouveau_tab[4]);
	loeffler_rond_inv(tableau_coeffs[5], &nouveau_tab[6]);
	loeffler_rond_inv(tableau_coeffs[3], &nouveau_tab[5]);
	//on réordonne la partie paire des coeffs
	nouveau_tab[3] = tableau_coeffs[6];
	nouveau_tab[2] = tableau_coeffs[2];
	nouveau_tab[1] = tableau_coeffs[4];
	nouveau_tab[0] = tableau_coeffs[0];

	// etape 3

	loeffler_butterfly_inv(nouveau_tab[0], nouveau_tab[1], &tableau_coeffs[0], &tableau_coeffs[1]);
	loeffler_rotation_inv(nouveau_tab[2], nouveau_tab[3], racine, 6, &tableau_coeffs[2], &tableau_coeffs[3]);
	loeffler_butterfly_inv(nouveau_tab[4], nouveau_tab[6], &tableau_coeffs[4], &tableau_coeffs[6]);
	loeffler_butterfly_inv(nouveau_tab[7], nouveau_tab[5], &tableau_coeffs[7], &tableau_coeffs[5]);

	// etape 2

	loeffler_butterfly_inv(tableau_coeffs[0], tableau_coeffs[3], &nouveau_tab[0], &nouveau_tab[3]);
	loeffler_butterfly_inv(tableau_coeffs[1], tableau_coeffs[2], &nouveau_tab[1], &nouveau_tab[2]);
	loeffler_rotation_inv(tableau_coeffs[4], tableau_coeffs[7], 1.0, 3, &nouveau_tab[4], &nouveau_tab[7]);
	loeffler_rotation_inv(tableau_coeffs[5], tableau_coeffs[6], 1.0, 1, &nouveau_tab[5], &nouveau_tab[6]);

	// etape 1

	loeffler_butterfly_inv(nouveau_tab[0], nouveau_tab[7], &tableau_coeffs[0], &tableau_coeffs[7]);
	loeffler_butterfly_inv(nouveau_tab[1], nouveau_tab[6], &tableau_coeffs[1], &tableau_coeffs[6]);
	loeffler_butterfly_inv(nouveau_tab[2], nouveau_tab[5], &tableau_coeffs[2], &tableau_coeffs[5]);
	loeffler_butterfly_inv(nouveau_tab[3], nouveau_tab[4], &tableau_coeffs[3], &tableau_coeffs[4]);
}

/*
* Fonction:  loeffler_saturation
* --------------------
* sature les valeurs après le calcul de loeffler
*
*  valeur : flottant qui doit être saturé entre 0 et 255
*
*/
static float loeffler_saturation(float valeur)
{
	if (valeur < 0.0){
		valeur = 0.0;
	} else if (valeur > 255.0) {
		valeur = 255.0;
	}

	return valeur;
}

/*
* Fonction:  loeffler_passage_float
* --------------------
* trans-typage en flottants du bloc d'entiers 16 bits auquel on va appliquer loeffler
*
*  bloc : bloc des 64 valeurs sur lesquelles faire l'iDCT
*
*/
static float *loeffler_passage_float(float* swap_bloc, int16_t *bloc)
{
	for (uint8_t i=0; i<BLOCK_PIXELS; i++) {
		swap_bloc[i] = (float) bloc[i];
	}
	free(bloc);

	return swap_bloc;
}

/*
* Fonction:  loeffler_passage_uint
* --------------------
* trans-typage en entiers non signés 8 bits du bloc de flottants auquel on a appliqué loeffler
*
*  bloc : bloc des 64 valeurs sur lesquelles on vient de faire l'iDCT
*
*/
static uint8_t *loeffler_passage_uint(float *bloc)
{
	uint8_t *nvx_bloc = malloc(64*sizeof(uint8_t));
	for (uint8_t i = 0; i < 64; i++) {
		bloc[i] = loeffler_saturation(8*bloc[i] + 128.0);
		nvx_bloc[i] = (uint8_t) round(bloc[i]);
	}

	return nvx_bloc;
}

/*
* Fonction:  loeffler_idct_loeffler
* --------------------
* iDCT par application de loeffler à un bloc 8x8
*
*       bloc : bloc des 64 valeurs sur lesquelles faire l'iDCT
*  swap_bloc : bloc de flottants utile pour les calculs intermédiaires
*
*/
uint8_t *loeffler_idct_loeffler(float* swap_bloc, int16_t *bloc)
{
	float *bloc_flottant = loeffler_passage_float(swap_bloc, bloc);
	// on passe sur chaque ligne
	for (uint8_t i = 0; i < 8; i++) {
		loeffler_inv(&bloc_flottant[8*i]);
	}
	// on passe sur les colonnes
	float colonne[8];
	for (uint8_t i = 0; i < 8; i++) {
		for (uint8_t j = 0; j < 8; j++) {
		// on construit les colonnes
			colonne[j] = bloc_flottant[i + 8*j];
		}
		loeffler_inv(colonne);
		for (uint8_t j = 0; j < 8; j++) {
			bloc_flottant[i + 8*j] = colonne[j];
		}
	}
	return loeffler_passage_uint(bloc_flottant);
}
