/**
 ******************************************************************************
 * @file    LedsRGB.h
 * @author  Jose Vargas Gonzaga (Adaptado a 4 Anillos)
 * @brief   Driver WS2812B usando TIM4_CH1/CH2 y TIM3_CH1/CH2 + DMAs en STM32.
 ******************************************************************************
 */

#ifndef LEDS_RGB_H
#define LEDS_RGB_H

#include <stdint.h>

// === CONFIGURACION (AJUSTAR TRAS EL SCAN INICIAL) ===
#define WS2812_LEDS_POR_ANILLO     16U   /* Cuenta fisica real */
#define WS2812_NUM_ANILLOS         4U    /* 4 anillos independientes */
#define WS2812_LEDS_TOTAL          (WS2812_LEDS_POR_ANILLO * WS2812_NUM_ANILLOS)
#define WS2812_MAX_BRIGHTNESS      80U   /* 0-255. Limita corriente por USB */

// === API PUBLICA ===
/** @brief Configura Timers + DMA + GPIOs (PB6, PD13, PC6, PC7). */
void LedsRGB_Init(void);

/** @brief Borra el buffer interno (todos los LEDs a negro). NO empuja al hardware. */
void LedsRGB_Clear(void);

/** @brief Escribe un color en un LED concreto del buffer interno. Aplica MAX_BRIGHTNESS. */
void LedsRGB_SetPixel(uint8_t anillo_id, uint16_t idx, uint8_t r, uint8_t g, uint8_t b);

/** @brief Rellena un anillo entero con un color uniforme. */
void LedsRGB_FillAnillo(uint8_t anillo_id, uint8_t r, uint8_t g, uint8_t b);

/** @brief Empuja el buffer interno al hardware via DMA. */
void LedsRGB_Show(void);

/** @brief Enciende todos los anillos de un color (Util para tests). */
void LedsRGB_TestAllSolid(uint8_t r, uint8_t g, uint8_t b);

/** @brief Modo TEST: enciende LED por LED en blanco con 200ms entre LEDs. */
void LedsRGB_Scan(void);

#endif /* LEDS_RGB_H */
