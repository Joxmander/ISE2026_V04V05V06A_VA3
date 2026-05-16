/**
 ******************************************************************************
 * @file    CerebroB_test.c
 * @author  Jose Vargas Gonzaga
 * @brief   Hilo de prueba integrado para el mockup del Nodo B.
 *
 * Secuencia de arranque (te ayuda a aislar el problema paso a paso):
 *   1. Init de Pads, LEDs y ToF (con prints).
 *   2. Test ALL-SOLID -> los DOS anillos deben verse ROJOS, luego VERDES, luego AZULES.
 *      Si no se ven -> el problema esta en wiring / alimentacion, NO en software.
 *   3. Scan secuencial LED por LED en blanco -> para contar fisicamente.
 *   4. Bucle principal: golpe pad 0 -> anillo 0 verde; golpe pad 1 -> anillo 1 azul.
 *      Tambien printea cada 100 ms la distancia del ToF y los valores raw de los pads.
 *
 * Salida de los printf: VENTANA "Debug (printf) Viewer" de Keil via ITM SWO.
 *   Necesitas activar trace en Keil: Debug -> Settings -> Trace ->
 *   Enable Trace + ITM Stimulus Port 0 + Core Clock = 180 MHz.
 ******************************************************************************
 */

#include "Pads.h"
#include "LedsRGB.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

// === DECLARACIONES EXTERNAS DEL DRIVER ToF EXISTENTE ===
extern void     SensorProximidad_Init(void);
extern uint16_t ToF_GetDistancia(void);
extern uint8_t  ToF_ManoEnPosicionNeutra(void);

// === IDENTIFICADORES DE THREAD ===
static osThreadId_t tid_CerebroBTest;
static const osThreadAttr_t attr_CerebroBTest = {
    .name       = "CerebroBTest",
    .stack_size = 1024U,
    .priority   = osPriorityNormal
};

/**
 * @brief Retarget de printf -> ITM SWO (Debug (printf) Viewer de Keil).
 *        Truco magico de Jox: con esto, todos los printf de cualquier .c
 *        salen por la ventana de debug sin necesidad de USART.
 */
int fputc(int ch, FILE *f) {
    (void)f;
    ITM_SendChar((uint32_t)ch);
    return ch;
}

// === HILO PRINCIPAL DE TEST ===
static void Hilo_CerebroBTest(void *argument) {
    uint32_t contador = 0U;
    (void)argument;

    /* ===== FASE 0: arranque ===== */
    printf("\r\n=== NODO B MOCKUP - TEST INTEGRADO ===\r\n");
    printf("SYSCLK=%lu Hz  HCLK=%lu Hz\r\n",
           (unsigned long)HAL_RCC_GetSysClockFreq(),
           (unsigned long)HAL_RCC_GetHCLKFreq());

    /* ===== FASE 1: init de perifericos ===== */
    printf("[INIT] Pads (PC3, PF10) -> ADC3 ... ");
    Pads_Init();
    printf("OK\r\n");

    printf("[INIT] LedsRGB (PB6 + PD13) TIM4 + 2xDMA ... ");
    LedsRGB_Init();
    printf("OK\r\n");

    printf("[INIT] SensorProximidad VL53L0X (PB8/PB9) ... ");
    SensorProximidad_Init();
    printf("OK\r\n");

    /* ===== FASE 2: TEST ALL-SOLID de los LEDs ===== */
    printf("[TEST] LEDs solido ROJO 2s. Mira los anillos AHORA.\r\n");
    LedsRGB_TestAllSolid(255U, 0U, 0U);
    osDelay(2000U);

    printf("[TEST] LEDs solido VERDE 2s.\r\n");
    LedsRGB_TestAllSolid(0U, 255U, 0U);
    osDelay(2000U);

    printf("[TEST] LEDs solido AZUL 2s.\r\n");
    LedsRGB_TestAllSolid(0U, 0U, 255U);
    osDelay(2000U);

    printf("[TEST] Si no se vio ningun color -> revisa cableado/alimentacion.\r\n");

    /* ===== FASE 3: SCAN secuencial para contar LEDs por anillo ===== */
    printf("[TEST] Scan LED-por-LED en blanco. Cuenta cuantos hay en cada anillo.\r\n");
    LedsRGB_Scan();
    printf("[TEST] Scan terminado. Si los anillos no tenian 16 LEDs cada uno,\r\n");
    printf("       edita WS2812_LEDS_POR_ANILLO en LedsRGB.h y recompila.\r\n");

    /* ===== FASE 4: bucle principal de juego ===== */
    printf("[MAIN] Golpea los pads. Pad 0 -> verde, Pad 1 -> azul.\r\n");
    while (1) {

        /* 1. Sondeo de pads (cada iteracion, ~2 ms) */
        Pads_Poll();

        /* 2. Pad 0 (PC3) golpeado -> Anillo 0 verde durante 200 ms */
        if (Pads_HayGolpe(0U)) {
            printf("[PAD] Golpe PAD 0 (raw=%u)\r\n", (unsigned)Pads_RawValue(0U));
            LedsRGB_Clear();
            LedsRGB_FillAnillo(0U, 0U, 255U, 0U);
            LedsRGB_Show();
            osDelay(200U);
            LedsRGB_Clear();
            LedsRGB_Show();
        }

        /* 3. Pad 1 (PF10) golpeado -> Anillo 1 azul durante 200 ms */
        if (Pads_HayGolpe(1U)) {
            printf("[PAD] Golpe PAD 1 (raw=%u)\r\n", (unsigned)Pads_RawValue(1U));
            LedsRGB_Clear();
            LedsRGB_FillAnillo(1U, 0U, 0U, 255U);
            LedsRGB_Show();
            osDelay(200U);
            LedsRGB_Clear();
            LedsRGB_Show();
        }

        /* 4. Telemetria periodica (cada 100 ms = 50 iteraciones de 2 ms) */
        contador++;
        if (contador >= 50U) {
            contador = 0U;
            printf("[STAT] ToF=%u mm  neutro=%u | RAW p0=%u p1=%u\r\n",
                   (unsigned)ToF_GetDistancia(),
                   (unsigned)ToF_ManoEnPosicionNeutra(),
                   (unsigned)Pads_RawValue(0U),
                   (unsigned)Pads_RawValue(1U));
        }

        /* 5. Cadencia del hilo */
        osDelay(2U);
    }
}

// === FUNCION PUBLICA PARA ARRANCAR EL TEST DESDE main.c ===
void CerebroBTest_Start(void) {
    tid_CerebroBTest = osThreadNew(Hilo_CerebroBTest, NULL, &attr_CerebroBTest);
}
