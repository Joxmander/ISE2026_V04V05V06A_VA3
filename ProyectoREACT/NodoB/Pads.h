/**
 ******************************************************************************
 * @file    Pads.h
 * @author  Equipo R.E.A.C.T.
 * @brief   Driver de pads piezoeléctricos — máquina de estados + pendiente.
 ******************************************************************************
 */

#ifndef PADS_H
#define PADS_H

#include <stdint.h>

#define PADS_NUM_CANALES        4U

/* ── Detección de impacto ─────────────────────────────────────────────────── */

/* Desviación mínima sobre la línea base para abrir la ventana de detección.
 * Con op-amp como buffer unitario los golpes producen 80-400 cuentas.
 * Sube si hay falsos positivos, baja si no detecta golpes suaves. */
#define PADS_UMBRAL_DELTA       150U

/* Pendiente mínima requerida (cuentas ADC por muestra de 5 ms) para que un
 * delta sobre el umbral se considere un golpe real.
 *
 * RAZONAMIENTO ANTI-FANTASMA:
 *   - Descarga RC del piezo con PCB (R=1MΩ, τ≈7.5 s):
 *       pendiente ≈ -0.27 counts/muestra → NUNCA supera 50 (negativa y lenta).
 *   - Golpe real: pendiente ≈ +200…+500 counts/muestra → SIEMPRE supera 50.
 *
 * Con este valor la probabilidad de falso positivo durante la descarga RC es
 * < 0.04 % (frente al ~15 % que daba el debounce simple).
 * Si sigues viendo fantasmas, sube a 80 o 100. */
#define PADS_PENDIENTE_MINIMA   50

/* ── Ventana de pico ──────────────────────────────────────────────────────── */
#define PADS_VENTANA_PICO_MS    30U   /* Tiempo máx. para buscar el pico tras trigger */
#define PADS_MUESTRAS_BAJADA    3U    /* N muestras consecutivas bajando → fin ventana */

/* ── Enfriamiento ─────────────────────────────────────────────────────────── */
/* Durante este periodo se ignoran nuevos triggers para evitar rebotes. */
#define PADS_ENFRIAMIENTO_MS    500U

/* ── Diagnóstico de hardware ──────────────────────────────────────────────── */
/* Si en reposo el pad oscila más que esto, se deshabilita (op-amp oscilando
 * o cable suelto). */
#define PADS_NOISE_LIMIT        200U

/* ── API pública ──────────────────────────────────────────────────────────── */
void     Pads_Init(void);
void     Pads_Poll(void);
uint8_t  Pads_HayGolpe(uint8_t pad_id);
uint8_t  Pads_GetFuerza(uint8_t pad_id);        /* Fuerza mapeada 1-100 % */
uint16_t Pads_GetRawDelta(uint8_t pad_id);      /* Delta ADC de la última muestra */
uint16_t Pads_GetRawInstant(uint8_t pad_id);    /* Lectura ADC directa sin tocar FSM (para test) */
void     Pads_CalibrarSensibilidad(void);       /* Rutina interactiva de calibración */

#endif /* PADS_H */
