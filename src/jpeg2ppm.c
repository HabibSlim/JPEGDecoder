#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "jpeg_const.h"
#include "jpeg_reader.h"
#include "bitstream.h"
#include "extract_image.h"
#include "export_ppm.h"


/* Paramètres d'appel */
bool P_VERBOSE, P_BLABLA, P_PROG_STEP, P_MULTITHREAD;
const char *OPT_VERBOSE, *OPT_BLABLA, *OPT_PROG_STEP, *OPT_MULTITHREAD;
const char *USAGE;

static char* create_outputname(const char* jpeg_name);
static void  check_opt(const char* opt_arg);

int main(int argc, char **argv)
{
    OPT_VERBOSE = "-v", OPT_BLABLA = "-b", OPT_PROG_STEP = "-p", OPT_MULTITHREAD = "-m";
    USAGE = "Usage: %s fichier.jpeg [FICHIER] ... [-v|-b|-p|-m] \n";
    P_VERBOSE = false; P_BLABLA = false; P_PROG_STEP = false; P_MULTITHREAD = false;

    if (argc < 2) {
        fprintf(stderr, USAGE, argv[0]);
        return EXIT_FAILURE;
    }

    /* On recupere le nom du fichier JPEG sur la ligne de commande. */
    const char *filename = argv[1]; char *outputname = argv[2];

    bool str_alloc = false;
    switch (argc)
    {
        case 4:
            check_opt(argv[3]);
            break;
        case 3:
            /* Si le deuxième arg n'est pas un nom de fichier valide -> paramètres ?*/
            if (strlen(argv[2])<5 || strchr(argv[2], '.')==NULL) {
                check_opt(argv[2]);
                outputname = create_outputname(filename); str_alloc = true;
            }
            break;
        case 2:
            /* Par défaut :génère un fichier ppm du même nom */
            outputname = create_outputname(filename); str_alloc = true;
            break;
        default:
            fprintf(stderr, USAGE, argv[0]);
            return EXIT_FAILURE;
    }

    /* On cree un jpeg_desc qui permettra de lire ce fichier. */
    struct jpeg_desc *jdesc = read_jpeg(filename);
    image8_t *jpeg_image;

    /* On extrait l'image JPEG du bitstream ouvert */
    jpeg_image = extract_image(jdesc);

    /* Exportation du fichier en PGM\PPM */
    export_img(jpeg_image, jdesc, outputname);
    printf("Fichier décompressé créé : %s > %s\n", filename, outputname);

    /* Libération des ressources */
    free_image(jpeg_image);
    close_jpeg(jdesc);
    if (str_alloc) free(outputname);

    return EXIT_SUCCESS;
}

static char* create_outputname(const char* jpeg_name)
{
    size_t name_len = strlen(jpeg_name);
    if (name_len < 5) {
        EXIT_ERROR("jpeg2ppm", "Nom du fichier d'entrée invalide : %s", jpeg_name);
    }

    /* On calcul l'index du caractère d'extension */
    char* dot_ptr = strrchr(jpeg_name, '.');
    if (dot_ptr == NULL) {
        EXIT_ERROR("jpeg2ppm", "Extension du fichier d'entrée invalide : %s", jpeg_name);
    }
    size_t index = (size_t)(dot_ptr - jpeg_name);
    if (index == 0) {
        EXIT_ERROR("jpeg2ppm", "Nom du fichier d'entrée invalide : %s", jpeg_name);
    }

    /* On alloue une chaîne de caractères pour le nouveau nom du fichier */
    char* outputname = malloc(sizeof(char)*100);
    strncpy(outputname, jpeg_name, index + 1);
    outputname[index] = '\0';
    outputname = strcat(outputname, ".ppm");

    return outputname;
}

static void check_opt(const char* opt_arg)
{
    // Mode verbose activé
    if (!strcmp(OPT_VERBOSE, opt_arg))
        P_VERBOSE = true;
    // Mode JPEGBlabla activé
    else if (!strcmp(OPT_BLABLA, opt_arg))
        P_BLABLA = true;
    // Mode progressif step activé
    else if (!strcmp(OPT_PROG_STEP, opt_arg))
        P_PROG_STEP = true;
    // Mode calcul parallèle activé
    else if (!strcmp(OPT_MULTITHREAD, opt_arg))
        P_MULTITHREAD = true;
    else
        EXIT_ERROR("jpeg2ppm", "Option inconnue : %s", opt_arg);    
}
