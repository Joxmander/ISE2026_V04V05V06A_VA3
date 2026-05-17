/**
 ******************************************************************************
 * @file    Pads.h
 * @author  Jose Vargas Gonzaga (Adaptado a 4 Pads)
 * @brief   Driver de pads piezoelectricos para el Nodo B (mockup protoboard).
 *
 * He montado los discos piezo SIN amplificador operacional (TLC274 aparcado).
 * La red de proteccion (R 1M descarga + Schottky/Zener + R 10k serie) garantiza
 * que el ADC vea picos positivos < 3.3V sin daniarse. La deteccion de golpe
 * es por umbral software y debounce de tiempo, hecho en el hilo de test.
 ******************************************************************************
 */

#ifndef PADS_H
#define PADS_H

#include <stdint.h>

// === CONFIGURACION DEL DRIVER ===
#define PADS_NUM_CANALES        4U      /* PC3, PF10, PF3, PF5 */
#define PADS_UMBRAL_GOLPE       500U    /* Raw ADC 12-bit (~0.4V). Subir si hay falsos positivos */
#define PADS_DEBOUNCE_MS        80U     /* Ignora golpes seguidos dentro de esta ventana */

// === API PUBLICA ===
/** @brief Configura ADC3 y los pines PC3, PF10, PF3, PF5 */
void     Pads_Init(void);

/** @brief Lee los 4 pads y actualiza flags. Llamar periodicamente (cada 1-5 ms). */
void     Pads_Poll(void);

/** @brief Devuelve 1 si hubo un golpe nuevo en pad_id desde la ultima llamada. Auto-limpia. */
uint8_t  Pads_HayGolpe(uint8_t pad_id);

/** @brief Devuelve el ultimo valor crudo del ADC (12 bits). Util para depurar el umbral. */
uint16_t Pads_RawValue(uint8_t pad_id);

#endif /* PADS_H */
