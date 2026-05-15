#ifndef COMFIBRAB_H
#define COMFIBRAB_H

#include <stdint.h>
#include "cmsis_os2.h"

#define FRAME_SIZE 6
#define SOF_BYTE   0xAA

typedef struct {
    uint8_t tipo_comando;
    uint8_t payload[3];
} TramaFibra_t;

extern osMessageQueueId_t colaFibraRX_B;

void FibraB_Init(void);
void FibraB_SendFrame(uint8_t tipo, uint8_t p1, uint8_t p2, uint8_t p3);

#endif
