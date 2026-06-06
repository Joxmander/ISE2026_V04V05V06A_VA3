/**
 * @file    testCerebroB.c
 * @brief   Hilo de PRUEBAS AISLADAS del Nodo B (Sin Fibra)
 * * Uso: Muestra por consola todos los impactos y estados de LED.
 * Para cambiar entre los 4 modos de juego, pulsa el BOTON AZUL (USER) de la Nucleo.
 */

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

#include "Pads.h"
#include "LedsRGB.h"
#include "fsm_modos.h" /* La maquina de estados de los juegos */

/* Parámetros del LED de reposo */
#define IDLE_SEMICICLO_MS   400U
#define IDLE_R               8U
#define IDLE_G               8U
#define IDLE_B               8U

static uint32_t tick_reposo_leds  = 0U;
static uint8_t  leds_reposo_on    = 0U;

/* -- Respiración LED en reposo (con debug) ----------------------------- */
static void ActualizarReposo(uint32_t ahora_ms)
{
    if ((ahora_ms - tick_reposo_leds) < IDLE_SEMICICLO_MS) { return; }
    tick_reposo_leds = ahora_ms;

    leds_reposo_on ^= 1U;
    if (leds_reposo_on) {
        // printf("[LEDs] Reposo: Encendiendo anillos suavemente\r\n"); // Descomentar si quieres verlo, pero llena la consola
        for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
            LedsRGB_FillAnillo(a, IDLE_R, IDLE_G, IDLE_B);
        }
    } else {
        // printf("[LEDs] Reposo: Apagando anillos\r\n");
        LedsRGB_Clear();
    }
    LedsRGB_Show();
}

/* ---------------------------------------------------------------------------
 * Hilo principal de Test
 * --------------------------------------------------------------------------- */

void Hilo_TestCerebroB(void *argument)
{
    (void)argument;

    printf("\r\n==================================================\r\n");
    printf("   [TEST NODO B] Arrancando en MODO AISLADO (Local)\r\n");
    printf("==================================================\r\n");

    /* 1. LEDs */
    LedsRGB_Init();
    printf("[TEST] LEDs -> Iniciando animacion arcoiris de arranque...\r\n");
    LedsRGB_ArcoirisAll(1U);

    /* 2. Pads */
    printf("[TEST] Pads -> Iniciando e ignorando ruido inicial (3s)...\r\n");
    Pads_Init();
    printf("[TEST] Pads -> Ejecutando calibracion de linea base...\r\n");
    Pads_CalibrarSensibilidad();

    /* 3. FSM de juegos */
    printf("[TEST] FSM -> Iniciando logica de juegos...\r\n");
    FSM_Init();

    /* Destello blanco */
    printf("[TEST] LEDs -> Destello BLANCO (Sistema Listo)\r\n");
    for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        LedsRGB_FillAnillo(a, 200U, 200U, 200U);
    }
    LedsRGB_Show();
    osDelay(400U);
    LedsRGB_Clear();
    LedsRGB_Show();

    printf("\r\n>>> SISTEMA LISTO. Pulsa el BOTON AZUL para empezar un modo de juego <<<\r\n\r\n");

    uint32_t tick_espera = osKernelGetTickCount();
    tick_reposo_leds     = tick_espera;

    uint8_t modo_simulado = 1; 
    uint8_t estado_boton_ant = 0;

    while (1) {
        uint32_t ahora = osKernelGetTickCount();

        /* -- SIMULADOR DE FIBRA: Usamos el Botón Azul (USER) para cambiar de juego -- */
        // La Nucleo-144 tiene el botón de usuario típicamente en el puerto PC13 (Activo en ALTO)
        uint8_t boton_actual = (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET); 
        
        if (boton_actual && !estado_boton_ant) {
            printf("\r\n------------------------------------------------\r\n");
            printf("[Boton Azul] Forzando inicio del MODO JUEGO %u\r\n", modo_simulado);
            printf("------------------------------------------------\r\n");
            
            // Engañamos a la FSM enviándole el comando 0x30 (Iniciar Modo)
            FSM_ComandoFibra(0x30, modo_simulado, 0, 0, ahora);
            
            modo_simulado++;
            if (modo_simulado > 4) modo_simulado = 1; // Cicla entre 1 y 4
        }
        estado_boton_ant = boton_actual;

        /* -- Lectura de Pads ---------------------------------------------- */
        Pads_Poll();

        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            if (!Pads_HayGolpe(i)) { continue; }

            uint8_t  fuerza = Pads_GetFuerza(i);
            uint16_t delta  = Pads_GetRawDelta(i);

            // Printf GIGANTE para ver claramente los golpes
            printf("\r\n>>> [GOLPE DETECTADO] Pad Físico: %u | Fuerza: %u%% | Delta Raw: %u\r\n", i, fuerza, delta);

            EventoGolpe_t golpe;
            golpe.pad_id     = i;
            golpe.fuerza_pct = fuerza;
            golpe.raw_delta  = delta;
            
            // Le pasamos el golpe al juego
            FSM_GolpePad(&golpe, ahora);

            break; // Procesamos solo uno por ciclo de 5ms
        }

        /* -- Tick de la FSM (Juegos) --------------------------------------- */
        FSM_Tick(ahora);

        /* -- Animación de reposo ------------------------------------------- */
        if (FSM_GetEstado() == JUEGO_REPOSO) {
            ActualizarReposo(ahora);
        }

        /* -- Cadencia fija 5 ms -- */
        tick_espera += 5U;
        osDelayUntil(tick_espera);
    }
}
