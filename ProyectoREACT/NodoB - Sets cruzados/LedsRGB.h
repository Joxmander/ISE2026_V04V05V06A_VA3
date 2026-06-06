/**
 ******************************************************************************
 * @file    LedsRGB.h
 * @author  Jose Vargas Gonzaga
 * @brief   Driver para controlar anillos de LEDs WS2812B en STM32.
 * * He diseñado esta arquitectura utilizando 4 canales PWM independientes 
 * en lugar de conectarlos todos en serie. De esta forma, evito caídas de 
 * tensión al final de la cadena, mantengo el blanco puro y consigo 
 * actualizar los 64 LEDs de forma simultánea.
 ******************************************************************************
 */

#ifndef LEDS_RGB_H
#define LEDS_RGB_H

#include <stdint.h>

/* --- Configuración de la Geometría --- */
#define WS2812_LEDS_POR_ANILLO     16U   /* Cantidad física de LEDs por anillo */
#define WS2812_NUM_ANILLOS         4U    /* Uso 4 conectores independientes en mi PCB */

/* Limito el brillo máximo (0-255) por seguridad. Así evito picos de consumo 
 * extremos que puedan colgar la fuente de alimentación o el USB de la Nucleo. */
#define WS2812_MAX_BRIGHTNESS      80U   

void LedsRGB_Init(void);
void LedsRGB_Clear(void);
void LedsRGB_FillAnillo(uint8_t anillo_id, uint8_t r, uint8_t g, uint8_t b);
void LedsRGB_Show(void);
/** @brief Realiza la animación de arcoíris en los 4 anillos simultáneamente */
void LedsRGB_ArcoirisAll(uint8_t vueltas);

#endif /* LEDS_RGB_H */
