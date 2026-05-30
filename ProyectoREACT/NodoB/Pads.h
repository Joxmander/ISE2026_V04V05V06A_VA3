/**
 ******************************************************************************
 * @file    Pads.h
 * @author  Jose Vargas Gonzaga
 * @brief   Driver de lectura piezoeléctrica mediante máquina de estados y ADC.
 ******************************************************************************
 */

#ifndef PADS_H
#define PADS_H

#include <stdint.h>

#define PADS_NUM_CANALES        4U

/* Umbral principal de detección de impactos. 
 * Tras calibrar la placa y añadir el aislamiento mecánico de metacrilato/goma EVA,
 * he ajustado este valor a 150 para garantizar inmunidad al ruido ambiental. */
#define PADS_UMBRAL_DELTA       150U

/* Pendiente mínima requerida (cuentas por muestra) para aceptar un golpe.
 * Este filtro anti-fantasma es vital: ignoro descargas naturales del circuito RC 
 * (que son lentas y negativas) y solo acepto impactos rápidos (subidas abruptas). */
#define PADS_PENDIENTE_MINIMA   50

/* Cooldown mecánico. Tiempo que mantengo el pad "sordo" tras un golpe para 
 * absorber rebotes de la membrana física antes de rearmar. */
#define PADS_ENFRIAMIENTO_MS    500U

/* Umbral de seguridad en el arranque. Si el op-amp oscila más de 200 cuentas
 * en reposo, deshabilito ese pad automáticamente para no bloquear el sistema. */
#define PADS_NOISE_LIMIT        200U

void     Pads_Init(void);
void     Pads_Poll(void);
uint8_t  Pads_HayGolpe(uint8_t pad_id);
uint8_t  Pads_GetFuerza(uint8_t pad_id);        /* Me devuelve de 1 a 100% */
uint16_t Pads_GetRawDelta(uint8_t pad_id);      
void     Pads_CalibrarSensibilidad(void);       

#endif /* PADS_H */
