/**
 ******************************************************************************
 * @file    Pads.c
 * @author  Jose Vargas Gonzaga
 * @brief   Deteccion robusta de impactos en sensores piezoeléctricos.
 *
 *  Máquina de estados por pad:
 *    REPOSO       : ajusta EMA de línea base. Trigger por umbral + pendiente.
 *    ENFRIAMIENTO : bloquea re-trigger mientras el piezo se descarga.
 *
 *  FIX v2 — delta_anterior:
 *    Al entrar/salir de ENFRIAMIENTO se asigna delta_anterior = delta (no 0).
 *    El case ENFRIAMIENTO usa break (no continue) para que la asignación al
 *    final del switch actualice delta_anterior en cada ciclo, siguiendo la
 *    rampa descendente del RC. Así deriv ˜ 0…1 count/muestra al salir del
 *    ENFRIAMIENTO ? el filtro de pendiente rechaza cualquier fantasma.
 ******************************************************************************
 */

#include "Pads.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "LedsRGB.h"
#include <stdio.h>

/* -- Tipos internos ------------------------------------------------------- */

typedef enum {
    PAD_ESTADO_REPOSO       = 0,
    PAD_ESTADO_ENFRIAMIENTO = 1
} EstadoPad_t;

typedef struct {
    EstadoPad_t estado;
    uint32_t    tick_fin_enfriamiento;
    int32_t     delta_anterior;     /* Sigue la rampa RC ? pendiente real */
    uint16_t    max_delta_sesion;   /* Diagnóstico */
} MaquinaPad_t;

/* -- Variables estáticas -------------------------------------------------- */

static ADC_HandleTypeDef hadc_pads;

static uint16_t     linea_base[PADS_NUM_CANALES];
static uint8_t      hay_golpe_flag[PADS_NUM_CANALES];
static uint8_t      fuerza_porcentaje[PADS_NUM_CANALES];
static uint16_t     ultimo_delta[PADS_NUM_CANALES];
static uint8_t      pad_habilitado[PADS_NUM_CANALES] = {1U, 1U, 1U, 1U};
static uint16_t     rango_max_fuerza[PADS_NUM_CANALES] = {1500U, 1500U, 1500U, 1500U};
static MaquinaPad_t maquina[PADS_NUM_CANALES];

/* -- Enrutamiento físico de canales ADC3 ---------------------------------- */

static const uint32_t canales_adc[PADS_NUM_CANALES] = {
    ADC_CHANNEL_13, /* Set 1 -> J3 (PC3)  */
    ADC_CHANNEL_8,  /* Set 2 -> J4 (PF10) */
    ADC_CHANNEL_9,  /* Set 3 -> J5 (PF3)  */
    ADC_CHANNEL_15  /* Set 4 -> J6 (PF5)  */
};

/* ----------------------------------------------------------------------------
 * Privado: lectura ADC con doble muestra (descarta la primera por contaminación
 * del canal anterior en el MUX interno del ADC).
 * ---------------------------------------------------------------------------- */

static uint16_t Pads_LeerCanal(uint32_t adc_channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    HAL_ADC_Stop(&hadc_pads);
    sConfig.Channel      = adc_channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    HAL_ADC_ConfigChannel(&hadc_pads, &sConfig);

    /* Primera conversión: descartada (flush del capacitor interno del S&H) */
    HAL_ADC_Start(&hadc_pads);
    if (HAL_ADC_PollForConversion(&hadc_pads, 1U) == HAL_OK) {
        HAL_ADC_GetValue(&hadc_pads);
    }
    HAL_ADC_Stop(&hadc_pads);

    /* Segunda conversión: dato válido */
    HAL_ADC_Start(&hadc_pads);
    if (HAL_ADC_PollForConversion(&hadc_pads, 2U) == HAL_OK) {
        uint16_t val = (uint16_t)HAL_ADC_GetValue(&hadc_pads);
        HAL_ADC_Stop(&hadc_pads);
        return val;
    }
    HAL_ADC_Stop(&hadc_pads);
    return 0U;
}

/* ----------------------------------------------------------------------------
 * Pads_Init — inicializa GPIO, ADC3 y calibra la línea base de reposo.
 * ---------------------------------------------------------------------------- */

void Pads_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    /* PC3 ? ADC3_CH13 */
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* PF10, PF3, PF5 ? ADC3_CH8/9/15 */
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_3 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* ADC3 */
    hadc_pads.Instance                   = ADC3;
    hadc_pads.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc_pads.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc_pads.Init.ScanConvMode          = DISABLE;
    hadc_pads.Init.ContinuousConvMode    = DISABLE;
    hadc_pads.Init.DiscontinuousConvMode = DISABLE;
    hadc_pads.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc_pads.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc_pads.Init.NbrOfConversion       = 1;
    hadc_pads.Init.DMAContinuousRequests = DISABLE;
    hadc_pads.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc_pads);

    /* Descarga del condensador RC del circuito piezo (t ˜ 7.5 s) */
    printf("[PADS] Descargando piezos (3s)...\r\n");
    osDelay(3000U);

    /* Calibración de la línea base y medición de ruido */
    printf("[PADS] Calibrando baseline de reposo...\r\n");
    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        uint32_t suma     = 0U;
        uint16_t max_ruido = 0U;

        /* 50 muestras para la media */
        for (uint8_t j = 0U; j < 50U; j++) {
            suma += Pads_LeerCanal(canales_adc[i]);
            osDelay(2U);
        }
        linea_base[i] = (uint16_t)(suma / 50U);

        /* 50 muestras para el ruido pico */
        for (uint8_t j = 0U; j < 50U; j++) {
            uint16_t v  = Pads_LeerCanal(canales_adc[i]);
            int32_t  d  = (int32_t)v - (int32_t)linea_base[i];
            uint16_t md = (uint16_t)((d < 0) ? -d : d);
            if (md > max_ruido) { max_ruido = md; }
            osDelay(2U);
        }

        /* Inicializar la máquina de estados */
        maquina[i].estado                = PAD_ESTADO_REPOSO;
        maquina[i].tick_fin_enfriamiento = 0U;
        maquina[i].delta_anterior        = 0;
        maquina[i].max_delta_sesion      = 0U;

        hay_golpe_flag[i]    = 0U;
        fuerza_porcentaje[i] = 0U;
        ultimo_delta[i]      = 0U;

        if (max_ruido > PADS_NOISE_LIMIT) {
            pad_habilitado[i] = 0U;
            printf(" -> Set %d  BASELINE=%-4u  RUIDO=%-4u  *** DESHABILITADO ***\r\n",
                   i + 1, linea_base[i], max_ruido);
        } else {
            pad_habilitado[i] = 1U;
            printf(" -> Set %d  BASELINE=%-4u  RUIDO=%-4u  OK\r\n",
                   i + 1, linea_base[i], max_ruido);
        }
    }
}

/* ----------------------------------------------------------------------------
 * Pads_Poll — llamar cada 5 ms desde el bucle principal.
 * ---------------------------------------------------------------------------- */

void Pads_Poll(void)
{
    uint32_t ahora = osKernelGetTickCount();

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        if (!pad_habilitado[i]) { continue; }

        uint16_t v_raw = Pads_LeerCanal(canales_adc[i]);
        int32_t  delta = (int32_t)v_raw - (int32_t)linea_base[i];
        if (delta < 0) { delta = -delta; }

        ultimo_delta[i] = (uint16_t)delta;
        if ((uint16_t)delta > maquina[i].max_delta_sesion) {
            maquina[i].max_delta_sesion = (uint16_t)delta;
        }

        int32_t      deriv = delta - maquina[i].delta_anterior;
        MaquinaPad_t *p    = &maquina[i];

        switch (p->estado) {

        case PAD_ESTADO_REPOSO:
            /* EMA de línea base (solo cuando la señal está quieta) */
            if (delta < (int32_t)(PADS_UMBRAL_DELTA / 4U)) {
                linea_base[i] = (uint16_t)(
                    ((uint32_t)linea_base[i] * 255U + v_raw) >> 8U);
            }

            /* Trigger: umbral + pendiente mínima ? golpe real */
            if ((delta  >= (int32_t)PADS_UMBRAL_DELTA) &&
                (deriv  >= (int32_t)PADS_PENDIENTE_MINIMA)) {

                /* Sprint de caza: 4 ms a máxima velocidad para capturar el pico */
                uint16_t pico_maximo  = (uint16_t)delta;
                uint32_t inicio_sprint = HAL_GetTick();
                while ((HAL_GetTick() - inicio_sprint) < 4U) {
                    uint16_t v_fast = Pads_LeerCanal(canales_adc[i]);
                    int32_t  d_fast = (int32_t)v_fast - (int32_t)linea_base[i];
                    if (d_fast < 0) { d_fast = -d_fast; }
                    if (d_fast > pico_maximo) { pico_maximo = (uint16_t)d_fast; }
                }

                /* Mapeo cúbico ? discrimina mejor la zona de fuerza baja */
                float ratio = (float)pico_maximo / (float)rango_max_fuerza[i];
                if (ratio > 1.0f) { ratio = 1.0f; }
                uint32_t pct = (uint32_t)(ratio * ratio * ratio * 100.0f);
                if (pct == 0U)   { pct = 1U;   }
                if (pct > 100U)  { pct = 100U; }

                fuerza_porcentaje[i] = (uint8_t)pct;
                hay_golpe_flag[i]    = 1U;

                p->estado                = PAD_ESTADO_ENFRIAMIENTO;
                p->tick_fin_enfriamiento = ahora + PADS_ENFRIAMIENTO_MS;
                /* FIX: no reseteamos delta_anterior a 0; sigue en delta
                 * para que al salir del ENFRIAMIENTO la pendiente calculada
                 * refleje la lenta rampa del RC, no un salto ficticio desde 0. */
                p->delta_anterior = delta;
            }
            break;

        case PAD_ESTADO_ENFRIAMIENTO:
            if (ahora >= p->tick_fin_enfriamiento) {
                p->estado = PAD_ESTADO_REPOSO;
                /* delta_anterior se actualiza al final del switch con el valor
                 * actual, que ya habrá bajado siguiendo la descarga RC. */
            }
            /* FIX: break (no continue) ? la línea al final del switch
             * actualiza delta_anterior en cada ciclo del ENFRIAMIENTO,
             * siguiendo la rampa descendente ? deriv ˜ 0 al salir ? sin fantasmas. */
            break;

        default:
            p->estado = PAD_ESTADO_REPOSO;
            break;
        }

        /* Actualización del predictor de pendiente (para TODOS los estados) */
        p->delta_anterior = delta;
    }
}

/* ----------------------------------------------------------------------------
 * API pública
 * ---------------------------------------------------------------------------- */

uint8_t Pads_HayGolpe(uint8_t pad_id)
{
    if (pad_id >= PADS_NUM_CANALES) { return 0U; }
    uint8_t f = hay_golpe_flag[pad_id];
    hay_golpe_flag[pad_id] = 0U;
    return f;
}

uint8_t Pads_GetFuerza(uint8_t pad_id)
{
    if (pad_id >= PADS_NUM_CANALES) { return 0U; }
    return fuerza_porcentaje[pad_id];
}

uint16_t Pads_GetRawDelta(uint8_t pad_id)
{
    if (pad_id >= PADS_NUM_CANALES) { return 0U; }
    return ultimo_delta[pad_id];
}

/**
 * @brief  Lee el ADC del pad indicado y devuelve |delta| instantáneo
 *         SIN tocar la máquina de estados. Útil para el módulo de test.
 */
uint16_t Pads_GetRawInstant(uint8_t pad_id)
{
    if (pad_id >= PADS_NUM_CANALES || !pad_habilitado[pad_id]) { return 0U; }
    uint16_t v_raw = Pads_LeerCanal(canales_adc[pad_id]);
    int32_t  delta = (int32_t)v_raw - (int32_t)linea_base[pad_id];
    return (uint16_t)((delta < 0) ? -delta : delta);
}

/* ----------------------------------------------------------------------------
 * Pads_CalibrarSensibilidad — calibración interactiva con feedback LED
 * ---------------------------------------------------------------------------- */

void Pads_CalibrarSensibilidad(void)
{
    printf("\r\n======================================================\r\n");
    printf("   CALIBRACION INTERACTIVA DE FUERZA MAXIMA\r\n");
    printf("======================================================\r\n");

    uint8_t fallos_calibracion[PADS_NUM_CANALES] = {0};

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        if (!pad_habilitado[i]) {
            fallos_calibracion[i] = 1U;
            continue;
        }

        /* Señal visual: anillo blanco = "golpéame" */
        LedsRGB_Clear();
        LedsRGB_FillAnillo(i, 160U, 160U, 160U);
        LedsRGB_Show();

        printf("\n>>> GOLPEA EL SET %d CON FUERZA (5 s) <<<\r\n", i + 1);
        uint32_t inicio         = osKernelGetTickCount();
        uint16_t max_detectado  = 0U;
        uint8_t  golpe_detectado = 0U;

        while ((osKernelGetTickCount() - inicio) < 5000U) {
            uint16_t v  = Pads_LeerCanal(canales_adc[i]);
            int32_t  d  = (int32_t)v - (int32_t)linea_base[i];
            uint16_t md = (uint16_t)((d < 0) ? -d : d);
            if (md > max_detectado) { max_detectado = md; }

            if (md > PADS_UMBRAL_DELTA * 2U) {
                golpe_detectado = 1U;
                osDelay(20U);  /* Espera a que pase el pico de vibración */
                break;
            }
            osDelay(2U);
        }

        if (max_detectado < PADS_UMBRAL_DELTA && !golpe_detectado) {
            printf(" -> Sin golpe. Usando valor estandar (1500 cuentas).\r\n");
            rango_max_fuerza[i]   = 1500U;
            fallos_calibracion[i] = 1U;
        } else {
            if (max_detectado < 1000U) { max_detectado = 1000U; }
            printf(" -> Registrado! El 100%% fisico seran %u cuentas.\r\n",
                   (unsigned)max_detectado);
            rango_max_fuerza[i]   = max_detectado;
            fallos_calibracion[i] = 0U;
        }

        /* FIX: esperar a que el piezo se descargue tras el golpe de calibración
         * para que delta_anterior (= 0 en este momento) no provoque fantasmas.
         * 1 s es suficiente para bajar del umbral con t = 7.5 s. */
        LedsRGB_Clear();
        LedsRGB_Show();
        osDelay(1000U);

        /* Re-leer baseline tras la descarga parcial */
        uint32_t suma = 0U;
        for (uint8_t j = 0U; j < 10U; j++) {
            suma += Pads_LeerCanal(canales_adc[i]);
            osDelay(10U);
        }
        linea_base[i] = (uint16_t)(suma / 10U);
        maquina[i].delta_anterior = 0;

        osDelay(200U);
    }

    /* Flash doble de confirmación */
    for (uint8_t f = 0U; f < 2U; f++) {
        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            LedsRGB_FillAnillo(i, 160U, 160U, 160U);
        }
        LedsRGB_Show();
        osDelay(200U);
        LedsRGB_Clear();
        LedsRGB_Show();
        osDelay(200U);
    }

    /* Color de resultado: blanco = OK, rosa = default/fallback */
    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        if (fallos_calibracion[i]) {
            LedsRGB_FillAnillo(i, 200U, 0U, 100U);   /* Rosa */
        } else {
            LedsRGB_FillAnillo(i, 160U, 160U, 160U); /* Blanco */
        }
    }
    LedsRGB_Show();
    osDelay(2000U);
    LedsRGB_Clear();
    LedsRGB_Show();

    printf("\n======================================================\r\n\n");
}
