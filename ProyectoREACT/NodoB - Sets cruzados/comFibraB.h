#ifndef COMFIBRAB_H
#define COMFIBRAB_H

#include <stdint.h>
#include "cmsis_os2.h"

#define FRAME_SIZE 6
#define SOF_BYTE   0xAA

/* COMANDOS DEL PROTOCOLO FIBRA ÓPTICA */
#define CMD_PING             0x10
#define CMD_START_JUEGO      0x30
#define ACK_START_JUEGO      0x31
#define CMD_GOTO_LOW_POWER   0x40
#define ACK_GOTO_LOW_POWER   0x41
#define REPORTE_FINAL        0x50
#define EVENTO_TIEMPO_REAL   0x60

/* MODOS DE JUEGO */
#define MODO_MEMORIA         1
#define MODO_FUERZA          2
#define MODO_INHIBICION      3
#define MODO_RITMO           4

typedef struct {
    uint8_t tipo_comando;
    uint8_t payload[3];
} TramaFibra_t;

extern osMessageQueueId_t colaFibraRX_B;

void FibraB_Init(void);
void FibraB_SendFrame(uint8_t tipo, uint8_t p1, uint8_t p2, uint8_t p3);


#endif
