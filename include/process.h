#ifndef __PROCESS_H__
#define __PROCESS_H__

#include <pthread.h>

#include "jpeg_reader.h"
#include "extract_image.h"


typedef struct {
    /* Propriétés de la tâche */
    struct jpeg_desc *jdesc;
    image16_t* zip;
    image8_t* unzip;

    /* Caractéristiques du thread */
    pthread_t   thread;
    uint8_t     thread_id;

    /* Indexes départ\arrivée du thread */
    uint32_t    work_range[3][2];
} work_thread;

void jpeg_blabla(struct jpeg_desc *jdesc, image16_t *zip_image);

void unzip_image(struct jpeg_desc *jdesc, image16_t* zip, image8_t* unzip);

void unzip_parallel(struct jpeg_desc *jdesc, image16_t* zip, image8_t* unzip);

#endif