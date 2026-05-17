/**
 ******************************************************************************
 * @file    CerebroB_test.c
 * @author  Jose Vargas Gonzaga (Adaptado a 4 Canales)
 * @brief   Hilo de prueba integrado para el mockup del Nodo B.
 *
 * Secuencia de arranque (te ayuda a aislar el problema paso a paso):
 * 1. Init de Pads, LEDs y ToF (con prints).
 * 2. Test ALL-SOLID -> los CUATRO anillos deben verse ROJOS, luego VERDES, luego AZULES.
 * Si no se ven -> el problema esta en wiring / alimentacion, NO en software.
 * 3. Scan secuencial LED por LED en blanco -> para contar fisicamente.
 * 4. Bucle principal: golpe en cualquier pad -> su anillo correspondiente se ilumina.
 * Tambien printea cada 100 ms la distancia del ToF y los valores raw de los pads.
 *
 * Salida de los printf: VENTANA "Debug (printf) Viewer" de Keil via ITM SWO.
 * Necesitas activar trace en Keil: Debug -> Settings -> Trace ->
 * Enable Trace + ITM Stimulus Port 0 + Core Clock = 180 MHz.
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
 * Truco magico de Jox: con esto, todos los printf de cualquier .c
 * salen por la ventana de debug sin necesidad de USART.
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
    printf("\r\n=== NODO B MOCKUP - TEST INTEGRADO 4 CANALES ===\r\n");
    printf("SYSCLK=%lu Hz  HCLK=%lu Hz\r\n",
           (unsigned long)HAL_RCC_GetSysClockFreq(),
           (unsigned long)HAL_RCC_GetHCLKFreq());

    /* ===== FASE 1: init de perifericos ===== */
    printf("[INIT] Pads (PC3, PF10, PF3, PF5) -> ADC3 ... ");
    Pads_Init();
    printf("OK\r\n");

    printf("[INIT] LedsRGB (PB6, PD13, PC6, PC7) ... ");
    LedsRGB_Init();
    printf("OK\r\n");

    printf("[INIT] SensorProximidad VL53L0X (PB8/PB9) ... ");
    SensorProximidad_Init();
    printf("OK\r\n");

    /* ===== FASE 2: TEST ALL-SOLID de los LEDs ===== */
    printf("[TEST] LEDs solido ROJO 2s. Mira los 4 anillos AHORA.\r\n");
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
    /* Descomenta las dos lineas de abajo si quieres hacer el escaneo lento inicial */
    // printf("[TEST] Scan LED-por-LED en blanco. Cuenta cuantos hay en cada anillo.\r\n");
    // LedsRGB_Scan(); 

    /* ===== FASE 4: bucle principal de juego ===== */
    printf("[MAIN] Golpea los pads. Cada pad enciende su anillo.\r\n");
    while (1) {

        /* 1. Sondeo de pads (cada iteracion, ~2 ms) */
        Pads_Poll();

        /* 2. Feedback de golpes */
        if (Pads_HayGolpe(0U)) {
            printf("[PAD] Golpe PAD 0 (raw=%u)\r\n", (unsigned)Pads_RawValue(0U));
            LedsRGB_Clear(); LedsRGB_FillAnillo(0U, 0U, 255U, 0U); LedsRGB_Show();
            osDelay(200U); LedsRGB_Clear(); LedsRGB_Show();
        }
        if (Pads_HayGolpe(1U)) {
            printf("[PAD] Golpe PAD 1 (raw=%u)\r\n", (unsigned)Pads_RawValue(1U));
            LedsRGB_Clear(); LedsRGB_FillAnillo(1U, 0U, 0U, 255U); LedsRGB_Show();
            osDelay(200U); LedsRGB_Clear(); LedsRGB_Show();
        }
        if (Pads_HayGolpe(2U)) {
            printf("[PAD] Golpe PAD 2 (raw=%u)\r\n", (unsigned)Pads_RawValue(2U));
            LedsRGB_Clear(); LedsRGB_FillAnillo(2U, 255U, 0U, 0U); LedsRGB_Show();
            osDelay(200U); LedsRGB_Clear(); LedsRGB_Show();
        }
        if (Pads_HayGolpe(3U)) {
            printf("[PAD] Golpe PAD 3 (raw=%u)\r\n", (unsigned)Pads_RawValue(3U));
            LedsRGB_Clear(); LedsRGB_FillAnillo(3U, 255U, 255U, 0U); LedsRGB_Show();
            osDelay(200U); LedsRGB_Clear(); LedsRGB_Show();
        }

        /* 3. Telemetria periodica (cada 100 ms = 50 iteraciones de 2 ms) */
        contador++;
        if (contador >= 50U) {
            contador = 0U;
            printf("[STAT] ToF=%u mm | p0=%u p1=%u p2=%u p3=%u\r\n",
                   (unsigned)ToF_GetDistancia(),
                   (unsigned)Pads_RawValue(0U), (unsigned)Pads_RawValue(1U),
                   (unsigned)Pads_RawValue(2U), (unsigned)Pads_RawValue(3U));
        }

        /* 4. Cadencia del hilo */
        osDelay(2U);
    }
}

// === FUNCION PUBLICA PARA ARRANCAR EL TEST DESDE main.c ===
void CerebroBTest_Start(void) {
    tid_CerebroBTest = osThreadNew(Hilo_CerebroBTest, NULL, &attr_CerebroBTest);
}
