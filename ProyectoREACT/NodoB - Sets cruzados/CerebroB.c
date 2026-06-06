/**
 ******************************************************************************
 * @file    CerebroB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Hilo principal del Nodo B — integración Pads + LEDs (SIN ToF).
 *
 * Mapeo Lógico -> Conector Físico:
 * Set 1 -> J3/J7  (PC3  / PB6)
 * Set 2 -> J4/J8  (PF10 / PD13)
 * Set 3 -> J5/J9  (PF3  / PC6)
 * Set 4 -> J6/J10 (PF5  / PC7)
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

#include "Pads.h"
#include "LedsRGB.h"
#include "comFibraB.h"

#define FUERZA_SUAVE      33U   
#define FUERZA_PERFECTO   66U   

#define LED_FLASH_MS     300U   
#define LED_IDLE_MS      200U   

static uint8_t  modo_juego_actual   = 2U;
static uint32_t ultimo_tiempo_golpe = 0U;

static uint32_t tick_fin_flash[WS2812_NUM_ANILLOS] = {0U, 0U, 0U, 0U};

extern osMessageQueueId_t colaFibraRX_B;

static float CalcularVoltaje(uint16_t valor_adc) {
    return ((float)valor_adc * 3.3f) / 4095.0f;
}

static void MostrarFeedbackFuerza(uint8_t anillo_id, uint8_t fuerza_pct) {
    uint8_t r, g, b;

    if (fuerza_pct < FUERZA_SUAVE) {
        r = 0U;   g = 0U;   b = 255U;   /* Azul  */
    } else if (fuerza_pct < FUERZA_PERFECTO) {
        r = 0U;   g = 255U; b = 0U;     /* Verde */
    } else {
        r = 255U; g = 0U;   b = 0U;     /* Rojo  */
    }

    LedsRGB_FillAnillo(anillo_id, r, g, b);
    LedsRGB_Show();
}

static void ActualizarLEDsReposo(void) {
    uint8_t hay_cambio = 0U;

    for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        if (tick_fin_flash[a] == 0U) { 
            LedsRGB_FillAnillo(a, 0U, 0U, 0U);
            hay_cambio = 1U;
        }
    }
    
    if (hay_cambio) {
        LedsRGB_Show();
    }
}

void Hilo_CerebroB(void *argument) {
    printf("\r\n======================================\r\n");
    printf("[NODO B] Arrancando Sistema...\r\n");
    printf("======================================\r\n");

    LedsRGB_Init();
    printf("[NODO B] Animacion de arranque en los 4 anillos...\r\n");
    LedsRGB_ArcoirisAll(1U);

    Pads_Init();
    Pads_CalibrarSensibilidad();
    
    FibraB_Init();

    for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        LedsRGB_FillAnillo(a, 200U, 200U, 200U);
    }
    LedsRGB_Show();
    osDelay(300U);
    LedsRGB_Clear();
    LedsRGB_Show();

    printf("[NODO B] Hardware OK. Modo: %u\r\n", modo_juego_actual);

    uint32_t tick_espera    = osKernelGetTickCount();
    uint32_t tick_idle_leds = osKernelGetTickCount();

    while (1) {
        uint32_t ahora = osKernelGetTickCount();

        TramaFibra_t tramaRX;
        if (osMessageQueueGet(colaFibraRX_B, &tramaRX, NULL, 0) == osOK) {
            if (tramaRX.tipo_comando == 0x30) {
                modo_juego_actual = tramaRX.payload[0];
                printf("\n[FIBRA] Modo de juego: %u\n", modo_juego_actual);
                FibraB_SendFrame(0x31, modo_juego_actual, 0, 0);
            } else if (tramaRX.tipo_comando == 0x10) {
                FibraB_SendFrame(0x11, 0xFF, 0xFF, 0);
            }
        }

        Pads_Poll();

        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            if (!Pads_HayGolpe(i)) { continue; }

            uint8_t  fuerza_pct      = Pads_GetFuerza(i);
            uint16_t raw_adc         = Pads_GetRawDelta(i);
            float    voltaje         = CalcularVoltaje(raw_adc);
            uint32_t ms_desde_ultimo = ahora - ultimo_tiempo_golpe;
            ultimo_tiempo_golpe      = ahora;

            MostrarFeedbackFuerza(i, fuerza_pct);
            tick_fin_flash[i] = ahora + LED_FLASH_MS; 

            printf("\n--- IMPACTO SET %u ---\n", i+1);
            printf("Senal ADC : %u cuentas (%.3f V)\n", raw_adc, voltaje);
            printf("Fuerza    : %u %%\n", fuerza_pct);

            switch (modo_juego_actual) {
                case 1: 
                    printf("[M1] Set %u detectado.\n", i+1);
                    break;
                case 2: 
                    if      (fuerza_pct < FUERZA_SUAVE)    { printf("     -> Evaluacion: SUAVE\n"); }
                    else if (fuerza_pct < FUERZA_PERFECTO) { printf("     -> Evaluacion: PERFECTO\n"); }
                    else                                   { printf("     -> Evaluacion: DURO\n"); }
                    break;
                case 3: 
                    printf("[M3] Reaccion en: %u ms\n", ms_desde_ultimo);
                    break;
                case 4: 
                    {
                        uint32_t bpm = (ms_desde_ultimo > 0U) ? (60000U / ms_desde_ultimo) : 0U;
                        printf("[M4] Cadencia: ~%u BPM (%u ms)\n", bpm, ms_desde_ultimo);
                    }
                    break;
            }

            FibraB_SendFrame(0x50, i, fuerza_pct, 0);
        }

        uint8_t leds_a_refrescar = 0U;
        for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
            if (tick_fin_flash[a] > 0U && ahora >= tick_fin_flash[a]) {
                tick_fin_flash[a] = 0U; 
                LedsRGB_FillAnillo(a, 0U, 0U, 0U); 
                leds_a_refrescar = 1U;
            }
        }
        
        if (leds_a_refrescar) {
            LedsRGB_Show();
        }

        if ((ahora - tick_idle_leds) >= LED_IDLE_MS) {
            tick_idle_leds = ahora;
            ActualizarLEDsReposo();
        }

        tick_espera += 5U;
        osDelayUntil(tick_espera);
    }
}
