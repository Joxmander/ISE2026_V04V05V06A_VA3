/**
 ******************************************************************************
 * @file    fsm_modos.h
 * @brief   Tipos y API de la máquina de estados de modos de juego (Nodo B).
 ******************************************************************************
 */

#ifndef FSM_MODOS_H
#define FSM_MODOS_H

#include <stdint.h>
#include "comFibraB.h"   

#ifndef CMD_PING
#define CMD_PING            0x10U
#endif
#ifndef CMD_PING_ACK
#define CMD_PING_ACK        0x20U
#endif
#ifndef CMD_INICIO_JUEGO
#define CMD_INICIO_JUEGO    0x30U
#endif
#ifndef CMD_ACK_INICIO
#define CMD_ACK_INICIO      0x31U
#endif
#ifndef CMD_SLEEP
#define CMD_SLEEP           0x40U
#endif
#ifndef CMD_ACK_SLEEP
#define CMD_ACK_SLEEP       0x41U
#endif
#ifndef CMD_REPORTE_FINAL
#define CMD_REPORTE_FINAL   0x50U
#endif
#ifndef CMD_EVENTO_RT
#define CMD_EVENTO_RT       0x60U
#endif

#ifndef ESTADO_ACTIVO
#define ESTADO_ACTIVO       0U
#endif
#ifndef ESTADO_SLEEP
#define ESTADO_SLEEP        1U
#endif
#ifndef ESTADO_JUGANDO
#define ESTADO_JUGANDO      2U
#endif

#ifndef MODO_MEMORIA
#define MODO_MEMORIA        1U
#endif
#ifndef MODO_FUERZA
#define MODO_FUERZA         2U
#endif
#ifndef MODO_INHIBICION
#define MODO_INHIBICION     3U
#endif
#ifndef MODO_RITMO
#define MODO_RITMO          4U
#endif

#define M4_MAX_SEQ          50U /* Máximo de rondas para el Simon infinito */

typedef enum {
    JUEGO_REPOSO = 0,
    JUEGO_MODO_MEMORIA,
    JUEGO_MODO_FUERZA,
    JUEGO_MODO_INHIBICION,
    JUEGO_MODO_RITMO  /* Aunque se llame RITMO, ahora aloja el Simon Infinito */
} EstadoJuego_t;

typedef struct {
    uint8_t  pad_id;
    uint8_t  fuerza_pct;
    uint16_t raw_delta;
} EventoGolpe_t;

void          FSM_Init(void);
void          FSM_Tick(uint32_t ahora_ms);
void          FSM_GolpePad(const EventoGolpe_t *g, uint32_t ahora_ms);
void          FSM_ComandoFibra(uint8_t cmd, uint8_t p0, uint8_t p1, uint8_t p2, uint32_t ahora_ms);
EstadoJuego_t FSM_GetEstado(void);

#endif /* FSM_MODOS_H */
