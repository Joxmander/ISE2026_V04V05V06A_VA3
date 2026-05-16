/**
 ******************************************************************************
 * @file    LedsRGB.h
 * @author  Jose Vargas Gonzaga
 * @brief   Driver WS2812B con DOS lineas de datos independientes (PB6 y PD13)
 *          usando TIM4_CH1, TIM4_CH2 y dos canales DMA.
 *
 * Pines:
 *   Anillo 0 DIN -> PB6  (TIM4_CH1, AF2) -> DMA1_Stream0_Ch2 (TIM_DMA_ID_CC1)
 *   Anillo 1 DIN -> PD13 (TIM4_CH2, AF2) -> DMA1_Stream3_Ch2 (TIM_DMA_ID_CC2)
 *
 * El refresco es secuencial: Show() pinta anillo 0, espera, pinta anillo 1.
 * Tiempo total < 2 ms (sobra para cualquier juego).
 *
 * IMPORTANTE: tras montar y antes de jugar, llama a LedsRGB_Scan() para
 *             contar fisicamente cuantos LEDs tiene cada anillo y ajusta
 *             WS2812_LEDS_POR_ANILLO si no son 16.
 ******************************************************************************
 */

#ifndef LEDS_RGB_H
#define LEDS_RGB_H

#include <stdint.h>

// === CONFIGURACION (AJUSTAR TRAS EL SCAN INICIAL) ===
#define WS2812_LEDS_POR_ANILLO     16U   /* Cuenta real por anillo. Ajustar tras el scan. */
#define WS2812_NUM_ANILLOS         2U
#define WS2812_MAX_BRIGHTNESS      80U   /* 0-255. Limita la corriente total por USB. */

// === API PUBLICA ===
/** @brief Configura TIM4 + 2 DMAs + GPIOs PB6 y PD13. Al volver: ambos anillos negros. */
void LedsRGB_Init(void);

/** @brief Borra el buffer interno de ambos anillos (NO empuja al hardware). */
void LedsRGB_Clear(void);

/** @brief Escribe un color en un LED de un anillo. Aplica MAX_BRIGHTNESS. */
void LedsRGB_SetPixel(uint8_t anillo_id, uint16_t idx, uint8_t r, uint8_t g, uint8_t b);

/** @brief Rellena un anillo entero (0 o 1) con un color uniforme. */
void LedsRGB_FillAnillo(uint8_t anillo_id, uint8_t r, uint8_t g, uint8_t b);

/** @brief Empuja ambos anillos al hardware (secuencial, < 2 ms total). */
void LedsRGB_Show(void);

/** @brief TEST: pone los DOS anillos al mismo color solido. Util para verificar hw. */
void LedsRGB_TestAllSolid(uint8_t r, uint8_t g, uint8_t b);

/** @brief TEST: enciende LED por LED con 200ms entre cada uno (10 s aprox). */
void LedsRGB_Scan(void);

#endif /* LEDS_RGB_H */
