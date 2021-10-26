#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include "process.h"
#include "jpeg_const.h"
#include "jpeg_reader.h"
#include "extract_image.h"
#include "iqzz.h"
#include "loeffler.h"

#define NTHREADS 1

/* Blocs swap */
// -> Loeffler
static float loeffler_swp[BLOCK_PIXELS];
static float loeffler_swp_th[NTHREADS][BLOCK_PIXELS];
// -> Zig-zag
int16_t zz_swp[64];
int16_t zz_swp_th[NTHREADS][64];


/*
 * Fonction:  jpeg_blabla
 * --------------------
 * affiche les étapes de décompression d'un bloc
 * image quelconque (Y, Cr ou Cb) dans la sortie standard.
 * 
 *  jdesc     : descripteur JPEG du fichier ouvert
 *  zip_image : image à décompresser
 *
 * Remarque : On ne peut pas faire blabla ET redécompresser ensuite,
 * puisque que iDCT libère les blocs passés sur 16 bits qu'on lui passe.
 * Le programme est donc terminé après exécution de la méthode.
 */
void jpeg_blabla(struct jpeg_desc *jdesc, image16_t *zip_image)
{
    char*      component[3] = {"Y", "Cb", "Cr"};
    uint8_t**  blocs_8bits = malloc(sizeof(int8_t*)*(zip_image->num_blocs));
    int16_t**  channels[3] = {zip_image->y_blocs, 
                              zip_image->cb_blocs,
                              zip_image->cr_blocs};

    /* Affichage type JPEGBlabla du traitement des blocs d'une composante */
    for (size_t j=0; j<zip_image->num_blocs; j++) {
        printf("**************************************************************\n");
        printf("*** mcu %zu\n", j);
        for (size_t channel_index=0; channel_index<3; channel_index++) {
            int16_t* bloc = channels[channel_index][j];
            printf("** component %s\n", component[channel_index]);
            printf("* bloc 0\n");

            /* Affichage du contenu du bloc */
            printf("[  bloc] ");
            for (size_t i=0; i<BLOCK_PIXELS; i++) {
                printf("%hx ", bloc[i]);
            }
            printf("\n");

            /* Affichage du bloc après inverse quantification */
            // On suppose que channel index vaut toujours 0 ou 1
            quantification_inverse(jdesc, bloc, (channel_index > 0));

            printf("[iquant] ");
            for (size_t i=0; i<BLOCK_PIXELS; i++) {
                printf("%hx ", bloc[i]);
            }
            printf("\n");

            /* Affichage du bloc après zig-zag inverse */
            int16_t* new_bloc = zig_zag(zz_swp, bloc);

            printf("[   izz] ");
            for (size_t i=0; i<BLOCK_PIXELS; i++) {
                printf("%hx ", new_bloc[i]);
            }
            printf("\n");

            /* Affichage du bloc après iDCT */
            blocs_8bits[j] = loeffler_idct_loeffler(loeffler_swp, new_bloc);

            printf("[  idct] ");
            for (size_t i=0; i<BLOCK_PIXELS; i++) {
                printf("%hhx ", blocs_8bits[j][i]);
            }
            printf("\n");
            printf("* component mcu\n");
            printf("[   mcu] ");
            for (size_t i=0; i<BLOCK_PIXELS; i++) {
                printf("%hhx ", blocs_8bits[j][i]);
            }
            printf("\n");
        }
        printf("\n");
    }
    /* Libération des blocs sur 8 bits créés après iDCT */
    for (size_t i=0; i<zip_image->num_blocs; i++) {
        free(blocs_8bits[i]);
    }
    free (blocs_8bits);
    printf("** Fin du fichier **\n");
    exit(-1);
}

/*
 * Fonction:  unzip_bloc 
 * --------------------
 * décompresse un bloc avec pixels sur 16 bits signés
 * vers un un bloc avec pixels sur 8 bits non signés.
 * 
 *  jdesc     : descripteur JPEG du fichier ouvert
 *  zip_bloc  : bloc JPEG à décoder
 *  dest_bloc : bloc JPEG décodé
 *  comp      : enuméré représentant le type de bloc
 *                  | 0 : Y
 *                  | 1 : Cb
 *                  | 2 : Cr
 */
static void unzip_bloc(struct jpeg_desc *jdesc, int16_t** zip_bloc, uint8_t** dest_bloc, enum component comp)
{
    int16_t* bloc = *zip_bloc;

    /* Quantification inverse */
    quantification_inverse(jdesc, bloc, (comp > 0));

    /* Zig-zag inverse */    
    bloc = zig_zag(zz_swp, bloc);

    /* DCT inverse */
    *dest_bloc = loeffler_idct_loeffler(loeffler_swp, bloc);
}
// Version multi-threads
static void unzip_bloc_th(struct jpeg_desc *jdesc, int16_t** zip_bloc, uint8_t** dest_bloc, enum component comp, uint8_t thread_id)
{
    int16_t* bloc = *zip_bloc;

    /* Quantification inverse */
    quantification_inverse(jdesc, bloc, (comp > 0));

    /* Zig-zag inverse */
    bloc = zig_zag(zz_swp_th[thread_id], bloc);

    /* DCT inverse */
    *dest_bloc = loeffler_idct_loeffler(loeffler_swp_th[thread_id], bloc);
}

/*
 * Fonction:  unzip_image
 * --------------------
 * décompression d'une image en couleur ou grayscale.
 * 
 *  jdesc : descripteur JPEG
 *  zip   : image 16 bits compressée
 *  unzip : image 8 bits décompressée
 * 
 */
void unzip_image(struct jpeg_desc *jdesc, image16_t* zip, image8_t* unzip)
{
    // -> Y
    for (size_t j=0; j<zip->num_blocs; j++)
        unzip_bloc(jdesc, &zip->y_blocs[j],  &unzip->y_blocs[j],  COMP_Y);
    if (zip->color) {
        // -> Cb
        for (size_t j=0; j<zip->num_blocs_Cb; j++)
            unzip_bloc(jdesc, &zip->cb_blocs[j], &unzip->cb_blocs[j], COMP_Cb);
        // -> Cr
        for (size_t j=0; j<zip->num_blocs_Cr; j++)
            unzip_bloc(jdesc, &zip->cr_blocs[j], &unzip->cr_blocs[j], COMP_Cr);
    }
}

/*
 * Fonction:  unzip_multithread
 * --------------------
 * décompression d'une partie des blocs de l'image,
 * 
 *  th : paramètres d'appel du thread courant
 *
 */
static void* unzip_multithread(void* param)
{
    work_thread* th = (work_thread*)param;
    // -> Y
    for (size_t j=th->work_range[0][0]; j<=th->work_range[0][1]; j++)
        unzip_bloc_th(th->jdesc, &th->zip->y_blocs[j], &th->unzip->y_blocs[j], COMP_Y, th->thread_id);
    if (th->zip->color) {
        // -> Cb
        for (size_t j=th->work_range[1][0]; j<=th->work_range[1][1]; j++)
            unzip_bloc_th(th->jdesc, &th->zip->cb_blocs[j], &th->unzip->cb_blocs[j], COMP_Cb, th->thread_id);
        // -> Cr
        for (size_t j=th->work_range[2][0]; j<=th->work_range[2][1]; j++)
            unzip_bloc_th(th->jdesc, &th->zip->cr_blocs[j], &th->unzip->cr_blocs[j], COMP_Cr, th->thread_id);
    }

    return NULL;
}

/*
 * Fonction:  split_jobs
 * --------------------
 * décompression d'une image en couleur ou grayscale,
 * via parallélisation.
 * 
 *  threads : descripteur JPEG
 *  zip     : image 16 bits compressée
 * 
 */
static bool split_jobs(work_thread* threads, image16_t* zip)
{
    uint32_t num_blocs[3] = {zip->num_blocs, zip->num_blocs_Cb, zip->num_blocs_Cr};
    uint32_t steps[3] = {num_blocs[0]/NTHREADS, 
                         num_blocs[1]/NTHREADS,
                         num_blocs[2]/NTHREADS},
             start[3] = {0, 0, 0};

    for (size_t i=0; i<3; i++) if (num_blocs[i]<NTHREADS*2) {
        return false;
    }

    /* Division des intervalles d'indices pour chaque thread */
    for (size_t i=0; i<NTHREADS-1; i++)
    for (size_t w=0; w<3; w++) {
        threads[i].work_range[w][0] = start[w];
        threads[i].work_range[w][1] = start[w]+steps[w]-1;
        start[w] = threads[i].work_range[w][1]+1;
    }
    for (size_t w=0; w<3; w++) {
        threads[NTHREADS-1].work_range[w][0] = start[w];
        threads[NTHREADS-1].work_range[w][1] = num_blocs[w]-1;
    }

    return true;
}

/*
 * Fonction:  unzip_parallel
 * --------------------
 * décompression d'une image en couleur ou grayscale,
 * via parallélisation.
 * 
 *  jdesc : descripteur JPEG
 *  zip   : image 16 bits compressée
 *  unzip : image 8 bits décompressée
 * 
 */
void unzip_parallel(struct jpeg_desc *jdesc, image16_t* zip, image8_t* unzip)
{
    /* Création des threads */
    work_thread threads[NTHREADS];

    /* Parallélisation inutile -> unzip normal */
    if (!split_jobs(threads, zip)) {        
        INFO_MSG("Parallélisation inutile - méthode de décompression classique utilisée.");
        unzip_image(jdesc, zip, unzip);
        return;
    }

    /* Démarrage des threads */
    for (size_t i=0; i<NTHREADS; i++) {
        // Initialisation de la structure
        threads[i].jdesc     = jdesc;
        threads[i].zip       = zip;
        threads[i].unzip     = unzip;
        threads[i].thread_id = i;

        pthread_create(&threads[i].thread, NULL, unzip_multithread, (void*)&threads[i]);
    }

    /* Joining threads */
    for (size_t i=0; i<NTHREADS; i++) {
        pthread_join(threads[i].thread, NULL);
    }
}
