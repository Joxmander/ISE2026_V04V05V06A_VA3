/**
 ******************************************************************************
 * @file    CerebroB.c
 * @brief   Hilo principal del Nodo B. Integra Pads + LEDs con feedback
 *          de fuerza visual: Azul=Suave | Verde=Perfecto | Rojo=Pasado.
 ******************************************************************************
 */

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

#include "Pads.h"
#include "LedsRGB.h"
#include "sensorProximidad.h"
#include "comFibraB.h"

/* -- Umbrales de fuerza (Modo 2) ---------------------------------------------
 * Con curva cubica (ratio^3):
 *   Golpe al 40% fisico -> ~6%  juego  -> SUAVE
 *   Golpe al 60% fisico -> ~22% juego  -> PERFECTO
 *   Golpe al 80% fisico -> ~51% juego  -> DURO
 * Ajusta segun lo que veas en los logs "[PADS] Golpe pad 0 pico=xxx fuerza=xx%". */
#define FUERZA_UMBRAL_SUAVE     18U   /* por debajo -> Suave    (Azul)  */
#define FUERZA_UMBRAL_PERFECTO  48U   /* por debajo -> Perfecto (Verde) */
                                      /* por encima -> Duro     (Rojo)  */

#define LED_FLASH_MS            300U  /* Duracion del destello de feedback */

/* -- Estado del sistema --------------------------------------------------- */
static uint8_t  modo_juego_actual   = 2U;
static uint32_t ultimo_tiempo_golpe = 0U;

/* -- Prototipos externos --------------------------------------------------- */
extern osMessageQueueId_t colaFibraRX_B;
extern void SensorProximidad_Init(void);

/* -- Utilidades privadas --------------------------------------------------- */

static float CalcularVoltaje(uint16_t valor_adc) {
    return ((float)valor_adc * 3.3f) / 4095.0f;
}

/**
 * @brief  Destello de color en el anillo segun la fuerza del golpe:
 *           < FUERZA_UMBRAL_SUAVE    -> Azul  (suave)
 *           < FUERZA_UMBRAL_PERFECTO -> Verde (perfecto)
 *           >= FUERZA_UMBRAL_PERFECTO -> Rojo (duro)
 */
static void MostrarFeedbackFuerza(uint8_t anillo_id, uint8_t fuerza_pct)
{
    uint8_t r, g, b;

    if (fuerza_pct < FUERZA_UMBRAL_SUAVE) {
        r = 0U;   g = 0U;   b = 255U;   /* Azul */
    } else if (fuerza_pct < FUERZA_UMBRAL_PERFECTO) {
        r = 0U;   g = 255U; b = 0U;     /* Verde */
    } else {
        r = 255U; g = 0U;   b = 0U;     /* Rojo */
    }

    LedsRGB_FillAnillo(anillo_id, r, g, b);
    LedsRGB_Show();
    osDelay(LED_FLASH_MS);
    LedsRGB_Clear();
    LedsRGB_Show();
}

/* -- Hilo principal -------------------------------------------------------- */

void Hilo_CerebroB(void *argument)
{
    printf("\r\n======================================\r\n");
    printf("[NODO B] Arrancando...\r\n");
    printf("======================================\r\n");

    /* 1. LEDs primero: respuesta visual inmediata */
    LedsRGB_Init();

    /* 2. Animacion arcoiris de arranque:
     *      Build-up : 12 LEDs x 80 ms = ~960 ms
     *      Rotacion : 1 vuelta x 12 pasos x 80 ms = ~960 ms
     *    Total ~2 s — aprovechamos este tiempo para que el piezo se descargue
     *    antes de que Pads_Init() haga la calibracion. */
    printf("[NODO B] Inicializando... (animacion de arranque)\r\n");
    LedsRGB_Arcoiris(0U, 1U);

    /* 3. Pads: espera interna de 3 s + calibracion de ~2 s */
    Pads_Init();

    /* 4. Calibracion interactiva de fuerza maxima del jugador */
    Pads_CalibrarSensibilidad();

    /* 5. Resto del hardware */
    SensorProximidad_Init();
    FibraB_Init();

    /* Destello blanco breve: sistema listo */
    LedsRGB_FillAnillo(0U, 200U, 200U, 200U);
    LedsRGB_Show();
    osDelay(200U);
    LedsRGB_Clear();
    LedsRGB_Show();

    printf("[NODO B] Hardware OK. Modo por defecto: %u (Fuerza)\r\n", modo_juego_actual);

    /* -- Bucle principal --------------------------------------------------- */
    uint32_t tick_espera = osKernelGetTickCount();

    while (1) {

        /* Comandos de fibra */
        TramaFibra_t tramaRX;
        if (osMessageQueueGet(colaFibraRX_B, &tramaRX, NULL, 0) == osOK) {
            if (tramaRX.tipo_comando == 0x30) {
                modo_juego_actual = tramaRX.payload[0];
                printf("\n[FIBRA] -> Modo de juego: %u\n", modo_juego_actual);
                FibraB_SendFrame(0x31, modo_juego_actual, 0, 0);
            } else if (tramaRX.tipo_comando == 0x10) {
                uint16_t dist = ToF_GetDistancia();
                FibraB_SendFrame(0x11, (uint8_t)(dist >> 8), (uint8_t)(dist & 0xFFU), 0);
            }
        }

        /* Polling de pads */
        Pads_Poll();

        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            if (!Pads_HayGolpe(i)) { continue; }

            uint8_t  fuerza_pct      = Pads_GetFuerza(i);
            uint16_t raw_adc         = Pads_GetRawDelta(i);
            float    voltaje         = CalcularVoltaje(raw_adc);
            uint32_t ahora           = osKernelGetTickCount();
            uint32_t ms_desde_ultimo = ahora - ultimo_tiempo_golpe;
            ultimo_tiempo_golpe = ahora;

            /* Feedback LED segun fuerza del golpe */
            MostrarFeedbackFuerza(i, fuerza_pct);

            /* Log diagnostico */
            printf("\n--- IMPACTO PAD %u ---\n", i);
            printf("Senal ADC : %u cuentas (%.3f V)\n", raw_adc, voltaje);
            printf("Fuerza    : %u %%\n", fuerza_pct);

            switch (modo_juego_actual) {

                case 1: /* Memoria de Trabajo */
                    printf("[M1] Pad %u detectado. Listo para secuencias.\n", i);
                    break;

                case 2: /* Control de Fuerza — Propiocepcion */
                    printf("[M2] Objetivo: 50%%. Tu impacto: %u%%.\n", fuerza_pct);
                    if (fuerza_pct < FUERZA_UMBRAL_SUAVE) {
                        printf("     -> SUAVE  (azul)\n");
                    } else if (fuerza_pct < FUERZA_UMBRAL_PERFECTO) {
                        printf("     -> PERFECTO (verde)\n");
                    } else {
                        printf("     -> DURO   (rojo)\n");
                    }
                    break;

                case 3: /* Inhibicion Motora */
                    printf("[M3] Tiempo desde ultimo golpe: %u ms\n", ms_desde_ultimo);
                    if (raw_adc > 2000U) {
                        printf("     -> AVISO: Golpe muy violento.\n");
                    }
                    break;

                case 4: /* Ritmo Constante */
                    {
                        uint32_t bpm = (ms_desde_ultimo > 0U) ? (60000U / ms_desde_ultimo) : 0U;
                        printf("[M4] Cadencia: ~%u BPM (Intervalo: %u ms)\n", bpm, ms_desde_ultimo);
                    }
                    break;

                default:
                    printf("[??] Modo no reconocido.\n");
                    break;
            }

            FibraB_SendFrame(0x50, i, fuerza_pct, 0);

            break; /* Un golpe por ciclo */
        }

        /* Bucle a 5 ms — alineado con el periodo de muestreo de Pads */
        tick_espera += 5U;
        osDelayUntil(tick_espera);
    }
}
