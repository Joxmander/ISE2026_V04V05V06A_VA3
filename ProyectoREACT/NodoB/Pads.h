/**
 ******************************************************************************
 * @file    Pads.h
 * @author  Jose Vargas Gonzaga
 * @brief   Driver de pads piezoelectricos para el Nodo B (mockup protoboard).
 *
 * Pines asignados:
 *   Pad 0 -> PC3  (ADC3_IN13)
 *   Pad 1 -> PF10 (ADC3_IN8)
 *
 * Ambos pads comparten ADC3 porque PF10 solo esta enrutado a ADC3.
 * He montado los discos piezo SIN amplificador operacional (TLC274 aparcado).
 * La red de proteccion (R 1M descarga + Zener 3V3 + R 10k serie) garantiza
 * que el ADC vea picos positivos < 3.3V sin daniarse.
 ******************************************************************************
 */

#ifndef PADS_H
#define PADS_H

#include <stdint.h>

// === CONFIGURACION DEL DRIVER ===
#define PADS_NUM_CANALES        2U      /* Pad 0 (PC3) y Pad 1 (PF10) */
#define PADS_UMBRAL_GOLPE       300U    /* Raw ADC 12-bit. Valor ya validado por el equipo (modulo SPZ) */
#define PADS_DEBOUNCE_MS        80U     /* Ignora golpes seguidos dentro de esta ventana */

// === API PUBLICA ===
/** @brief Configura ADC3 + canales IN13/IN8 y los pines PC3/PF10 */
void     Pads_Init(void);

/** @brief Lee ambos pads y actualiza flags. Llamar periodicamente (cada 2-5 ms). */
void     Pads_Poll(void);

/** @brief Devuelve 1 si hubo un golpe nuevo en pad_id desde la ultima llamada. Auto-limpia. */
uint8_t  Pads_HayGolpe(uint8_t pad_id);

/** @brief Devuelve el ultimo valor crudo del ADC (12 bits). Util para depurar el umbral. */
uint16_t Pads_RawValue(uint8_t pad_id);

#endif /* PADS_H */
