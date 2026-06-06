/**
 ******************************************************************************
 * @file    fsm_modos.h
 * @author  Equipo R.E.A.C.T.
 * @brief   Interfaz de la FSM — Capa Lógica.
 ******************************************************************************
 */

#ifndef FSM_MODOS_H
#define FSM_MODOS_H

#include <stdint.h>
#include <stdbool.h>
#include "comFibraB.h"
#include "spz.h"

typedef enum {
    STATE_IDLE,
    STATE_LOW_POWER,
    STATE_MODO_MEMORIA,
    STATE_MODO_FUERZA,
    STATE_MODO_RITMO,
    STATE_MODO_INHIBICION,
    STATE_SEND_RESULTS
} FSM_State_t;

typedef enum {
    EV_NONE = 0,
    EV_TICK_MS,
    EV_RX_COMANDO,
    EV_SENSOR_HIT,
    EV_SENSOR_TOF,
    EV_POT_CAMBIO
} FSM_Event_t;

// --- MEMORIA ---
typedef enum {
    MEM_INACTIVO = 0,
    MEM_ESPERANDO_MANO,
    MEM_GENERAR_NIVEL,
    MEM_PAUSA_PRE_SECUENCIA,
    MEM_MOSTRAR_LED_ON,
    MEM_MOSTRAR_LED_OFF,
    MEM_FLASH_TURNO,
    MEM_ESPERANDO_INPUT,
    MEM_FEEDBACK_GOLPE,
    MEM_FEEDBACK_FALLO,
    MEM_FEEDBACK_VICTORIA
} SubEstadoMemoria_t;

typedef struct {
    SubEstadoMemoria_t sub;
    uint8_t  secuencia[6];
    uint8_t  longitud_secuencia;
    uint8_t  indice_mostrar;
    uint8_t  indice_input;
    uint16_t debounce_tof_ms;
} CtxMemoria_t;

// --- FUERZA ---
typedef enum {
    FZA_INACTIVO = 0,
    FZA_ESPERANDO_MANO,
    FZA_INICIANDO_RONDA,
    FZA_FLASH_PAD,
    FZA_ESPERANDO_GOLPE,
    FZA_FEEDBACK_RONDA,
    FZA_PAUSA_ENTRE_RONDAS,
    FZA_CIERRE_PARTIDA
} SubEstadoFuerza_t;

typedef struct {
    SubEstadoFuerza_t sub;
    uint8_t  ronda_actual;
    uint8_t  aciertos_total;
    uint16_t suma_fuerzas;
    uint16_t debounce_tof_ms;
    uint8_t  pad_objetivo;
    uint8_t  fuerza_objetivo;
    uint8_t  fuerza_conseguida;
} CtxFuerza_t;

// --- RITMO ---
typedef enum {
    RIT_INACTIVO = 0,
    RIT_ESPERANDO_MANO,
    RIT_INICIANDO_NIVEL,
    RIT_FLASH_INICIAL,
    RIT_BEEPS_AVISO,
    RIT_ESPERANDO_PRIMER_GOLPE,
    RIT_MIDIENDO_INTERVALOS,
    RIT_NIVEL_COMPLETADO,
    RIT_FEEDBACK_FALLO,
    RIT_FEEDBACK_VICTORIA
} SubEstadoRitmo_t;

typedef struct {
    SubEstadoRitmo_t sub;
    uint8_t  pad_objetivo;
    uint16_t periodo_ms;
    uint16_t tolerancia_ms;
    uint8_t  golpes_dados;
    uint8_t  beeps_pendientes;
    uint32_t golpe_anterior_ms;
    uint16_t debounce_tof_ms;
} CtxRitmo_t;

// --- INHIBICION ---
typedef enum {
    INH_INACTIVO = 0,
    INH_ESPERANDO_MANO,
    INH_INICIANDO_NIVEL,
    INH_GENERAR_ESTIMULO,
    INH_ESTIMULO_ACTIVO,
    INH_PAUSA_ENTRE_NIVELES,
    INH_FEEDBACK_FALLO,
    INH_FEEDBACK_VICTORIA
} SubEstadoInhibicion_t;

typedef struct {
    SubEstadoInhibicion_t sub;
    uint8_t  fallos_acumulados;
    uint16_t tiempo_restante_nivel_ms;
    uint16_t duracion_estimulo_ms;
    uint8_t  pad_actual;
    bool     color_es_rojo;
    bool     sonido_es_agudo;
    bool     debe_pulsar;
    bool     jugador_pulso_alguno;
    bool     jugador_pulso_correcto;
    uint16_t debounce_tof_ms;
} CtxInhibicion_t;


typedef struct {
    FSM_State_t estado_actual;
    uint32_t    timer_ms;
    uint8_t     id_modo_actual;
    uint16_t    puntuacion;
    uint8_t     nivel_actual;
    uint8_t     nivel_alcanzado;
    uint8_t     errores;

    CtxMemoria_t    mem;
    CtxFuerza_t     fuerza;
    CtxRitmo_t      ritmo;
    CtxInhibicion_t inhib;

    bool partida_recien_iniciada;
} FSM_Context_t;

void        FSM_Init(void);
void        FSM_ProcesarEvento(FSM_Event_t evento, const TramaFibra_t* trama_in, void* param_local);
FSM_State_t FSM_GetEstadoActual(void);

#endif /* FSM_MODOS_H */
