/**
 ******************************************************************************
 * @file    CerebroB.c
 * @brief   Hilo principal del Nodo B — integración Pads + LEDs + FSM de juego.
 *
 *  Protocolo de fibra (completo):
 *    0x10  A?B  Ping / estado
 *    0x20  B?A  Respuesta ping  [estado | consumo_hi | consumo_lo]
 *    0x30  A?B  Iniciar modo    [id_modo]
 *    0x31  B?A  ACK inicio      [id_modo]
 *    0x40  A?B  Entrar sleep
 *    0x41  B?A  ACK sleep
 *    0x50  B?A  Reporte final   [id_modo | nivel | fallos_pts]
 *    0x60  B?A  Evento RT       [pad | fuerza_pct | t_reaccion_ms10]
 *
 *  Bucle principal a 5 ms (osDelayUntil) — 100 % no bloqueante:
 *    1. Vaciar cola de fibra ? FSM_ComandoFibra
 *    2. Pads_Poll() + recoger golpes ? FSM_GolpePad
 *    3. FSM_Tick() ? animaciones LED y transiciones temporizadas
 *    4. LEDs de reposo (solo cuando JUEGO_REPOSO)
 *    5. osDelayUntil(tick_espera + 5)
 *
 *  Mapeo pad ? anillo LED:
 *    Set 1 (PC3  / PB6)   ? pad 0 / anillo 0
 *    Set 2 (PF10 / PD13)  ? pad 1 / anillo 1
 *    Set 3 (PF3  / PC6)   ? pad 2 / anillo 2
 *    Set 4 (PF5  / PC7)   ? pad 3 / anillo 3
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

#include "Pads.h"
#include "LedsRGB.h"
#include "comFibraB.h"
#include "fsm_modos.h"

/* -- Cola de fibra (definida en comFibraB.c) ------------------------------ */
/* TramaFibra_t ya viene declarada desde comFibraB.h (incluido por fsm_modos.h) */
extern osMessageQueueId_t colaFibraRX_B;

/* -- Parámetros del LED de reposo ----------------------------------------- */
#define IDLE_SEMICICLO_MS   400U   /* mitad del periodo de "respiración" */
#define IDLE_R               8U
#define IDLE_G               8U
#define IDLE_B               8U

static uint32_t tick_reposo_leds  = 0U;
static uint8_t  leds_reposo_on    = 0U;
static uint32_t ultimo_golpe_ms   = 0U;

/* -- Respiración LED en reposo (no bloqueante) ----------------------------- */
static void ActualizarReposo(uint32_t ahora_ms)
{
    if ((ahora_ms - tick_reposo_leds) < IDLE_SEMICICLO_MS) { return; }
    tick_reposo_leds = ahora_ms;

    leds_reposo_on ^= 1U;
    if (leds_reposo_on) {
        for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
            LedsRGB_FillAnillo(a, IDLE_R, IDLE_G, IDLE_B);
        }
    } else {
        LedsRGB_Clear();
    }
    LedsRGB_Show();
}

/* ---------------------------------------------------------------------------
 * Hilo principal
 * --------------------------------------------------------------------------- */

void Hilo_CerebroB(void *argument)
{
    (void)argument;

    /* -- Secuencia de arranque ---------------------------------------------- */
    printf("\r\n======================================\r\n");
    printf("[NODO B] Arrancando...\r\n");
    printf("======================================\r\n");

    /* 1. LEDs: feedback visual inmediato */
    LedsRGB_Init();
    printf("[NODO B] Animacion arcoiris de arranque...\r\n");
    LedsRGB_ArcoirisAll(1U);   /* ~1.9 s; los piezos se descargan mientras */

    /* 2. Pads: descarga RC 3 s + calibración de línea base */
    Pads_Init();

    /* 3. Calibración relativa de 2 puntos (suave + fuerte, por pad) */
    Pads_CalibrarSensibilidad();

    /* 4. FSM de modos de juego */
    FSM_Init();

    /* 5. Fibra óptica */
    FibraB_Init();

    /* Destello blanco breve: "sistema listo" */
    for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        LedsRGB_FillAnillo(a, 200U, 200U, 200U);
    }
    LedsRGB_Show();
    osDelay(400U);
    LedsRGB_Clear();
    LedsRGB_Show();

    printf("[NODO B] Sistema listo. Esperando Nodo A...\r\n");

    /* -- Bucle principal (5 ms) --------------------------------------------- */
    uint32_t tick_espera = osKernelGetTickCount();
    tick_reposo_leds     = tick_espera;

    while (1) {

        uint32_t ahora = osKernelGetTickCount();

        /* -- 1. Comandos recibidos por fibra ------------------------------- */
        {
            TramaFibra_t trama;
            while (osMessageQueueGet(colaFibraRX_B, &trama, NULL, 0U) == osOK) {
                FSM_ComandoFibra(trama.tipo_comando,
                                 trama.payload[0U],
                                 trama.payload[1U],
                                 trama.payload[2U],
                                 ahora);
            }
        }

        /* -- 2. Lectura de los 4 pads piezoeléctricos ---------------------- */
        Pads_Poll();

        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            if (!Pads_HayGolpe(i)) { continue; }

            uint8_t  fuerza = Pads_GetFuerza(i);
            uint16_t delta  = Pads_GetRawDelta(i);

            /* Tiempo entre golpes consecutivos (para modo reposo) */
            uint32_t t_react_ms = ahora - ultimo_golpe_ms;
            ultimo_golpe_ms = ahora;
            uint8_t  t_ms10 = (uint8_t)((t_react_ms / 10U) > 255U
                                         ? 255U
                                         : (t_react_ms / 10U));

            /* Enviar 0x60 en reposo; en juego lo envía internamente la FSM
             * con el tiempo relativo al objetivo correcto del modo activo */
            if (FSM_GetEstado() == JUEGO_REPOSO) {
                FibraB_SendFrame(CMD_EVENTO_RT, i, fuerza, t_ms10);
            }

            printf("[PAD %u] delta=%-4u  fuerza=%u%%\r\n", i, delta, fuerza);

            /* Entregar golpe a la FSM */
            EventoGolpe_t golpe;
            golpe.pad_id    = i;
            golpe.fuerza_pct = fuerza;
            golpe.raw_delta  = delta;
            FSM_GolpePad(&golpe, ahora);

            break;   /* Un golpe por ciclo para no saturar el bus LED */
        }

        /* -- 3. Tick de la FSM --------------------------------------------- */
        FSM_Tick(ahora);

        /* -- 4. Animación de reposo (solo cuando no hay juego activo) ------ */
        if (FSM_GetEstado() == JUEGO_REPOSO) {
            ActualizarReposo(ahora);
        }

        /* -- 5. Cadencia fija 5 ms ----------------------------------------- */
        tick_espera += 5U;
        osDelayUntil(tick_espera);
    }
}
