/**
 ******************************************************************************
 * @file    fsm_modos.h
 * @author  Equipo R.E.A.C.T.
 * @date    Mayo 2026
 * @brief   Interfaz de la FSM — Capa Lógica.
 *
 * @details SPRINT 5: Modo INHIBICIÓN + DISCRIMINACIÓN implementado.
 *          Cambios respecto al Sprint 4:
 *          - Nuevo enum SubEstadoInhibicion_t con 7 fases internas.
 *          - Nuevo struct CtxInhibicion_t con duración del estímulo,
 *            tiempo restante del nivel, info del estímulo en curso
 *            (pad, color, sonido, regla derivada) y contador de fallos.
 *          - Añadido al FSM_Context_t: CtxInhibicion_t inhib.
 *          - Todo lo de Memoria, Fuerza y Ritmo queda intacto.
 *          - Los 4 modos están ahora completamente funcionales.
 ******************************************************************************
 */

#ifndef FSM_MODOS_H
#define FSM_MODOS_H

#include <stdint.h>
#include <stdbool.h>
#include "comFibraB.h"
#include "spz.h"

/* ========================================================================== */
/*                          ENUMERADORES DEL SISTEMA                          */
/* ========================================================================== */

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

/**
 * @brief Sub-fases internas del Modo Memoria (Simon Says).
 */
typedef enum {
    MEM_INACTIVO = 0,
    MEM_ESPERANDO_MANO,
    MEM_GENERAR_NIVEL,
    MEM_PAUSA_PRE_SECUENCIA,
    MEM_MOSTRAR_LED_ON,
    MEM_MOSTRAR_LED_OFF,
    MEM_FLASH_TURNO,
    MEM_ESPERANDO_INPUT,
    MEM_FEEDBACK_FALLO,
    MEM_FEEDBACK_VICTORIA
} SubEstadoMemoria_t;

/**
 * @brief Sub-fases internas del Modo Fuerza.
 */
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

/**
 * @brief Sub-fases internas del Modo Ritmo.
 */
typedef enum {
    RIT_INACTIVO = 0,
    RIT_INICIANDO_NIVEL,
    RIT_FLASH_INICIAL,
    RIT_BEEPS_AVISO,
    RIT_ESPERANDO_PRIMER_GOLPE,
    RIT_MIDIENDO_INTERVALOS,
    RIT_NIVEL_COMPLETADO,
    RIT_FEEDBACK_FALLO,
    RIT_FEEDBACK_VICTORIA
} SubEstadoRitmo_t;

/**
 * @brief Sub-fases internas del Modo Inhibición + Discriminación.
 *        3 niveles de 20 s con estímulos encadenados de duración
 *        decreciente (1.0 / 0.75 / 0.5 s). Cada estímulo combina color
 *        y sonido. Regla: pulsar solo si (NO rojo) AND (agudo).
 */
typedef enum {
    INH_INACTIVO = 0,
    INH_INICIANDO_NIVEL,        /* Configura duración estímulo y resetea cuenta del nivel */
    INH_GENERAR_ESTIMULO,       /* Elige pad/color/sonido del siguiente estímulo */
    INH_ESTIMULO_ACTIVO,        /* Pad encendido + beep, esperando respuesta */
    INH_PAUSA_ENTRE_NIVELES,    /* 2 s verdes entre niveles */
    INH_FEEDBACK_FALLO,         /* 1.5 s rojos (3 fallos acumulados) */
    INH_FEEDBACK_VICTORIA       /* 2 s verdes tras nivel 3 */
} SubEstadoInhibicion_t;

/* ========================================================================== */
/*                       CONTEXTOS ESPECÍFICOS POR MODO                       */
/* ========================================================================== */

/**
 * @brief Variables específicas del Modo Memoria.
 */
typedef struct {
    SubEstadoMemoria_t sub;
    uint8_t  secuencia[6];
    uint8_t  longitud_secuencia;
    uint8_t  indice_mostrar;
    uint8_t  indice_input;
    uint16_t debounce_tof_ms;
} CtxMemoria_t;

/**
 * @brief Variables específicas del Modo Fuerza.
 */
typedef struct {
    SubEstadoFuerza_t sub;
    uint8_t  ronda_actual;
    uint8_t  pad_objetivo;
    uint8_t  fuerza_objetivo;
    uint8_t  fuerza_conseguida;
    uint8_t  aciertos_total;
    uint16_t suma_fuerzas;
    uint16_t debounce_tof_ms;
} CtxFuerza_t;

/**
 * @brief Variables específicas del Modo Ritmo.
 */
typedef struct {
    SubEstadoRitmo_t sub;
    uint8_t  pad_objetivo;
    uint16_t periodo_ms;
    uint16_t tolerancia_ms;
    uint8_t  golpes_dados;
    uint8_t  beeps_pendientes;
    uint32_t golpe_anterior_ms;
} CtxRitmo_t;

/**
 * @brief Variables específicas del Modo Inhibición + Discriminación.
 */
typedef struct {
    SubEstadoInhibicion_t sub;
    uint16_t duracion_estimulo_ms;       /* 1000, 750 o 500 según nivel */
    uint32_t tiempo_restante_nivel_ms;   /* Cuenta atrás de los 20 s del nivel */
    uint8_t  pad_actual;                 /* Pad del estímulo en curso (0..3) */
    bool     color_es_rojo;              /* Estímulo actual: ¿rojo? */
    bool     sonido_es_agudo;            /* Estímulo actual: ¿agudo? */
    bool     debe_pulsar;                /* Regla maestra: !rojo && agudo */
    bool     jugador_pulso_alguno;       /* ¿Pulsó cualquier pad? (detecta comisión) */
    bool     jugador_pulso_correcto;     /* ¿Pulsó el pad correcto? (detecta omisión) */
    uint8_t  fallos_acumulados;          /* 0..3, acumulan entre niveles */
} CtxInhibicion_t;

/* ========================================================================== */
/*                            CONTEXTO DE LA FSM                              */
/* ========================================================================== */

typedef struct {
    /* Estado y tiempo */
    FSM_State_t estado_actual;
    uint32_t    timer_ms;

    /* Identificación de la partida */
    uint8_t     id_modo_actual;

    /* Acumuladores comunes */
    uint16_t    puntuacion;
    uint8_t     nivel_actual;
    uint8_t     nivel_alcanzado;
    uint8_t     errores;

    /* Variables específicas por modo */
    CtxMemoria_t    mem;
    CtxFuerza_t     fuerza;
    CtxRitmo_t      ritmo;
    CtxInhibicion_t inhib;

    /* Bandera de "primera llamada al handler tras start" */
    bool partida_recien_iniciada;
} FSM_Context_t;

/* ========================================================================== */
/*                                 API                                        */
/* ========================================================================== */

void        FSM_Init(void);
void        FSM_ProcesarEvento(FSM_Event_t evento,
                               const TramaFibra_t* trama_in,
                               void* param_local);
FSM_State_t FSM_GetEstadoActual(void);

#endif /* FSM_MODOS_H */
