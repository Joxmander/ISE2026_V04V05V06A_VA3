/**
 ******************************************************************************
 * @file    Pads.c
 * @author  Equipo R.E.A.C.T.
 * @brief   Driver de pads piezoeléctricos.
 *
 *  Algoritmo:
 *    REPOSO --[delta=UMBRAL y pendiente=PENDIENTE_MINIMA]--? VENTANA
 *    VENTANA --[N muestras bajando o timeout]--? ENFRIAMIENTO
 *    ENFRIAMIENTO --[timer expirado]--? REPOSO
 *
 *  El filtro de pendiente es la clave anti-fantasma: la descarga RC del piezo
 *  tiene pendiente ˜ -0.27 counts/5ms (negativa y lentísima), mientras que un
 *  golpe real tiene pendiente ˜ +200…+500 counts/5ms.
 ******************************************************************************
 */

#include "Pads.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

/* ----------------------- Tipos internos ------------------------------------ */

typedef enum {
    PAD_ESTADO_REPOSO      = 0,
    PAD_ESTADO_VENTANA     = 1,
    PAD_ESTADO_ENFRIAMIENTO = 2
} EstadoPad_t;

typedef struct {
    EstadoPad_t estado;
    uint32_t    tick_inicio_ventana;    /* Tick al entrar en VENTANA */
    uint32_t    tick_fin_enfriamiento;  /* Tick en que acaba el enfriamiento */
    uint16_t    pico_ventana;           /* Delta máximo visto en la ventana */
    int32_t     delta_anterior;         /* Delta de la muestra previa (para pendiente) */
    uint8_t     muestras_bajando;       /* Contador de muestras consecutivas descendentes */
    uint16_t    max_delta_sesion;       /* Máximo histórico para debug */
} MaquinaPad_t;

/* ----------------------- Variables estáticas ------------------------------- */

static ADC_HandleTypeDef hadc_pads;

static uint16_t     linea_base[PADS_NUM_CANALES];
static uint8_t      hay_golpe_flag[PADS_NUM_CANALES];
static uint8_t      fuerza_porcentaje[PADS_NUM_CANALES];
static uint16_t     ultimo_delta[PADS_NUM_CANALES];
static uint8_t      pad_habilitado[PADS_NUM_CANALES] = {1U, 1U, 1U, 1U};
static uint16_t     rango_max_fuerza[PADS_NUM_CANALES] = {1500U, 1500U, 1500U, 1500U};
static MaquinaPad_t maquina[PADS_NUM_CANALES];

static const uint32_t canales_adc[PADS_NUM_CANALES] = {
    ADC_CHANNEL_13,   /* Pad 0 — PC3 */
    ADC_CHANNEL_8,    /* Pad 1 — PF10 */
    ADC_CHANNEL_9,    /* Pad 2 — PF3 */
    ADC_CHANNEL_15    /* Pad 3 — PF5 */
};

/* ----------------------- Funciones privadas -------------------------------- */

static uint16_t Pads_LeerCanal(uint32_t adc_channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = adc_channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    HAL_ADC_ConfigChannel(&hadc_pads, &sConfig);

    HAL_ADC_Start(&hadc_pads);
    if (HAL_ADC_PollForConversion(&hadc_pads, 2U) == HAL_OK) {
        return (uint16_t)HAL_ADC_GetValue(&hadc_pads);
    }
    return 0U;
}

/* ----------------------- API pública --------------------------------------- */

void Pads_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    /* PC3 — Pad 0 */
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* PF10, PF3, PF5 — Pads 1, 2, 3 */
    GPIO_InitStruct.Pin  = GPIO_PIN_10 | GPIO_PIN_3 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

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

    /* -- Espera 3 s para que la carga estática del piezo se descargue ------
     * Con R=1MO y t˜7.5 s, a los 3 s el piezo ya está por debajo de ~0.7 V.
     * Si calibras antes, la línea base queda inflada y el UMBRAL falla. */
    printf("[PADS] Descargando piezo... espera 3 s sin tocar los pads.\r\n");
    osDelay(3000U);

    /* -- Auto-calibración de línea base ------------------------------------ */
    printf("[PADS] Calibrando baseline (no toques nada los proximos ~2 s)...\r\n");

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        uint32_t suma    = 0U;
        uint16_t max_ruido = 0U;

        /* 1ª pasada: calcular media */
        for (uint8_t j = 0U; j < 50U; j++) {
            suma += Pads_LeerCanal(canales_adc[i]);
            osDelay(2U);
        }
        linea_base[i] = (uint16_t)(suma / 50U);

        /* 2ª pasada: medir ruido pico */
        for (uint8_t j = 0U; j < 50U; j++) {
            uint16_t v  = Pads_LeerCanal(canales_adc[i]);
            int32_t  d  = (int32_t)v - (int32_t)linea_base[i];
            uint16_t md = (uint16_t)((d < 0) ? -d : d);
            if (md > max_ruido) { max_ruido = md; }
            osDelay(2U);
        }

        /* Inicializar estado de la máquina */
        maquina[i].estado               = PAD_ESTADO_REPOSO;
        maquina[i].tick_inicio_ventana  = 0U;
        maquina[i].tick_fin_enfriamiento = 0U;
        maquina[i].pico_ventana         = 0U;
        maquina[i].delta_anterior       = 0;
        maquina[i].muestras_bajando     = 0U;
        maquina[i].max_delta_sesion     = 0U;

        hay_golpe_flag[i]   = 0U;
        fuerza_porcentaje[i] = 0U;
        ultimo_delta[i]     = 0U;

        if (max_ruido > PADS_NOISE_LIMIT) {
            pad_habilitado[i] = 0U;
            printf(" -> Pad %d  BASELINE=%-4u  RUIDO=%-4u  *** DESHABILITADO ***\r\n",
                   i, linea_base[i], max_ruido);
        } else {
            pad_habilitado[i] = 1U;
            printf(" -> Pad %d  BASELINE=%-4u  RUIDO=%-4u  OK\r\n",
                   i, linea_base[i], max_ruido);
        }

        /* Fuerza-deshabilitar pads 1-3 mientras sólo hay hardware en pad 0 */
        if (i != 0U) {
            pad_habilitado[i] = 0U;
        }
    }
    printf("[PADS] Calibracion terminada.\r\n");
}

void Pads_Poll(void)
{
    static uint32_t ultimo_print_dbg = 0U;

    uint32_t ahora = osKernelGetTickCount();

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        if (!pad_habilitado[i]) { continue; }

        uint16_t v_raw = Pads_LeerCanal(canales_adc[i]);
        int32_t  delta = (int32_t)v_raw - (int32_t)linea_base[i];
        if (delta < 0) { delta = -delta; }   /* trabajamos siempre con magnitud */

        ultimo_delta[i] = (uint16_t)delta;
        if ((uint16_t)delta > maquina[i].max_delta_sesion) {
            maquina[i].max_delta_sesion = (uint16_t)delta;
        }

        /* Pendiente: diferencia entre muestra actual y anterior (counts/5ms) */
        int32_t deriv = delta - maquina[i].delta_anterior;

        MaquinaPad_t *p = &maquina[i];

        switch (p->estado) {

        /* -- REPOSO ------------------------------------------------------- */
        case PAD_ESTADO_REPOSO:

            /* Actualizar línea base con EMA lenta sólo cuando hay calma */
            if (delta < (int32_t)(PADS_UMBRAL_DELTA / 4U)) {
                /* a = 1/256 ? t ˜ 256 muestras × 5 ms = 1.28 s */
                linea_base[i] = (uint16_t)(
                    ((uint32_t)linea_base[i] * 255U + v_raw) >> 8U
                );
            }

            /* Trigger: delta sobre umbral Y pendiente positiva suficiente.
             * Esto rechaza la descarga RC (pendiente ˜ -0.27 counts/5ms). */
            if ((delta  >= (int32_t)PADS_UMBRAL_DELTA) &&
                (deriv  >= (int32_t)PADS_PENDIENTE_MINIMA))
            {
                p->estado              = PAD_ESTADO_VENTANA;
                p->tick_inicio_ventana = ahora;
                p->pico_ventana        = (uint16_t)delta;
                p->muestras_bajando    = 0U;
            }
            break;

        /* -- VENTANA DE PICO ----------------------------------------------- */
        case PAD_ESTADO_VENTANA:

            /* Rastrear el pico máximo */
            if ((uint16_t)delta > p->pico_ventana) {
                p->pico_ventana     = (uint16_t)delta;
                p->muestras_bajando = 0U;
            } else {
                p->muestras_bajando++;
            }

            {
                uint8_t  fin_bajada  = (p->muestras_bajando >= PADS_MUESTRAS_BAJADA);
                uint8_t  fin_timeout = ((ahora - p->tick_inicio_ventana) >= PADS_VENTANA_PICO_MS);

                if (fin_bajada || fin_timeout) {
/* -- Impacto confirmado ------------------------------- */
// 1. Calculamos el ratio de fuerza de 0.0 a 1.0
float ratio = (float)p->pico_ventana / (float)rango_max_fuerza[i];
if (ratio > 1.0f) { ratio = 1.0f; }

// 2. Aplicamos la curva cuadrática (ratio al cuadrado)
// Esto expande el rango dinámico: un golpe del 50% de fuerza bruta 
// ahora será un 25% en el juego. Tienes que golpear más duro para subir.
uint32_t pct = (uint32_t)(ratio * ratio * ratio * 100.0f);

if (pct == 0U)  { pct = 1U;   }
if (pct > 100U) { pct = 100U; }

fuerza_porcentaje[i] = (uint8_t)pct;
hay_golpe_flag[i]    = 1U;

                    printf("[PADS] Golpe pad %d  pico=%u  fuerza=%u%%\r\n",
                           i, p->pico_ventana, (unsigned)pct);

                    p->estado                = PAD_ESTADO_ENFRIAMIENTO;
                    p->tick_fin_enfriamiento = ahora + PADS_ENFRIAMIENTO_MS;
                    p->pico_ventana          = 0U;
                    p->muestras_bajando      = 0U;
                }
            }
            break;

        /* -- ENFRIAMIENTO -------------------------------------------------- */
        case PAD_ESTADO_ENFRIAMIENTO:
            if (ahora >= p->tick_fin_enfriamiento) {
                p->estado         = PAD_ESTADO_REPOSO;
                p->delta_anterior = 0;
            }
            /* Saltar la actualización de delta_anterior para no contaminar
             * la pendiente al volver a REPOSO. */
            continue;

        default:
            p->estado = PAD_ESTADO_REPOSO;
            break;
        }

        /* Actualizar muestra anterior (sólo fuera de ENFRIAMIENTO) */
        p->delta_anterior = delta;
    }

    /* -- Debug periódico -------------------------------------------------- */
    if ((ahora - ultimo_print_dbg) >= 2000U) {
        ultimo_print_dbg = ahora;

        uint8_t hay_actividad = 0U;
        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            if (maquina[i].max_delta_sesion > 40U) { hay_actividad = 1U; break; }
        }

        if (hay_actividad) {
            printf("[PADS DBG] max_delta: p0=%u p1=%u p2=%u p3=%u  umbral=%u  pend_min=%d\r\n",
                   maquina[0].max_delta_sesion, maquina[1].max_delta_sesion,
                   maquina[2].max_delta_sesion, maquina[3].max_delta_sesion,
                   (unsigned)PADS_UMBRAL_DELTA, (int)PADS_PENDIENTE_MINIMA);

            for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
                maquina[i].max_delta_sesion = 0U;
            }
        }
    }
}

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

void Pads_CalibrarSensibilidad(void)
{
    printf("\r\n======================================================\r\n");
    printf("   CALIBRACION DE FUERZA MAXIMA (MODO 2)             \r\n");
    printf("======================================================\r\n");

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        if (!pad_habilitado[i]) { continue; }

        printf("\n>>> GOLPEA EL PAD %d CON TODA TU FUERZA (5 s) <<<\r\n", i);

        uint32_t inicio       = osKernelGetTickCount();
        uint16_t max_detectado = 0U;

        while ((osKernelGetTickCount() - inicio) < 5000U) {
            uint16_t v  = Pads_LeerCanal(canales_adc[i]);
            int32_t  d  = (int32_t)v - (int32_t)linea_base[i];
            uint16_t md = (uint16_t)((d < 0) ? -d : d);
            if (md > max_detectado) { max_detectado = md; }
            osDelay(2U);
        }

        if (max_detectado < PADS_UMBRAL_DELTA) {
            printf(" -> Sin golpe detectado. Usando valor por defecto (1500).\r\n");
            rango_max_fuerza[i] = 1500U;
        } else {
            printf(" -> Impacto registrado. 100%% = %u cuentas.\r\n", max_detectado);
            rango_max_fuerza[i] = max_detectado;
        }
    }

    printf("\n======================================================\r\n");
    printf("           CALIBRACION COMPLETADA                     \r\n");
    printf("======================================================\r\n\n");
}

