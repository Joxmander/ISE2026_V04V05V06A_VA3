/**
 ******************************************************************************
 * @file    Pads.c
 * @author  Jose Vargas Gonzaga
 * @brief   Detección robusta de impactos en sensores piezoeléctricos.
 * * He implementado una máquina de estados simplificada:
 * 1. REPOSO: Ajusto la línea base dinámicamente para compensar deriva térmica.
 * 2. SPRINT (Pico): Al superar el umbral, congelo el hilo 4ms leyendo a máxima 
 * velocidad para "cazar" el voltaje máximo del golpe, que a menudo se 
 * perdía en el retardo de 5ms del RTOS.
 * 3. ENFRIAMIENTO: Ignoro la caída natural de la tensión.
 ******************************************************************************
 */

#include "Pads.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

typedef enum {
    PAD_ESTADO_REPOSO      = 0,
    PAD_ESTADO_ENFRIAMIENTO = 1
} EstadoPad_t;

typedef struct {
    EstadoPad_t estado;
    uint32_t    tick_fin_enfriamiento;  
    int32_t     delta_anterior;         /* Usado para medir la velocidad de subida (Pendiente) */
    uint16_t    max_delta_sesion;       
} MaquinaPad_t;

static ADC_HandleTypeDef hadc_pads;

static uint16_t     linea_base[PADS_NUM_CANALES];
static uint8_t      hay_golpe_flag[PADS_NUM_CANALES];
static uint8_t      fuerza_porcentaje[PADS_NUM_CANALES];
static uint16_t     ultimo_delta[PADS_NUM_CANALES];
static uint8_t      pad_habilitado[PADS_NUM_CANALES] = {1U, 1U, 1U, 1U};
static uint16_t     rango_max_fuerza[PADS_NUM_CANALES] = {1500U, 1500U, 1500U, 1500U};
static MaquinaPad_t maquina[PADS_NUM_CANALES];

static const uint32_t canales_adc[PADS_NUM_CANALES] = {
    ADC_CHANNEL_13, ADC_CHANNEL_8, ADC_CHANNEL_9, ADC_CHANNEL_15 
};

/* Función auxiliar para leer mi ADC de forma manual y síncrona */
static uint16_t Pads_LeerCanal(uint32_t adc_channel) {
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

void Pads_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_10 | GPIO_PIN_3 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // Configuro el hardware ADC
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

    /* Espero 3 segundos para que los condensadores RC se descarguen a 0V 
     * antes de tomar mis medidas de línea base. */
    printf("[PADS] Descargando piezo (3s)...\r\n");
    osDelay(3000U);

    printf("[PADS] Calibrando baseline de reposo...\r\n");
    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        uint32_t suma    = 0U;
        uint16_t max_ruido = 0U;

        /* Promedio la línea base en reposo */
        for (uint8_t j = 0U; j < 50U; j++) {
            suma += Pads_LeerCanal(canales_adc[i]);
            osDelay(2U);
        }
        linea_base[i] = (uint16_t)(suma / 50U);

        /* Mido la dispersión (ruido) máxima para ver si el canal es seguro */
        for (uint8_t j = 0U; j < 50U; j++) {
            uint16_t v  = Pads_LeerCanal(canales_adc[i]);
            int32_t  d  = (int32_t)v - (int32_t)linea_base[i];
            uint16_t md = (uint16_t)((d < 0) ? -d : d);
            if (md > max_ruido) { max_ruido = md; }
            osDelay(2U);
        }

        maquina[i].estado                = PAD_ESTADO_REPOSO;
        maquina[i].tick_fin_enfriamiento = 0U;
        maquina[i].delta_anterior        = 0;
        maquina[i].max_delta_sesion      = 0U;

        hay_golpe_flag[i]    = 0U;
        fuerza_porcentaje[i] = 0U;
        ultimo_delta[i]      = 0U;

        /* Deshabilito el canal si hay demasiado ruido fantasma */
        if (max_ruido > PADS_NOISE_LIMIT) {
            pad_habilitado[i] = 0U;
            printf(" -> Pad %d  BASELINE=%-4u  RUIDO=%-4u  *** DESHABILITADO ***\r\n", i, linea_base[i], max_ruido);
        } else {
            pad_habilitado[i] = 1U;
            printf(" -> Pad %d  BASELINE=%-4u  RUIDO=%-4u  OK\r\n", i, linea_base[i], max_ruido);
        }

        /* Fuerza-deshabilitar pads no conectados en el banco de pruebas actual */
        if (i != 0U) pad_habilitado[i] = 0U;
    }
}

void Pads_Poll(void) {
    static uint32_t ultimo_print_dbg = 0U;
    uint32_t ahora = osKernelGetTickCount();

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        if (!pad_habilitado[i]) continue;

        uint16_t v_raw = Pads_LeerCanal(canales_adc[i]);
        int32_t  delta = (int32_t)v_raw - (int32_t)linea_base[i];
        if (delta < 0) delta = -delta; 

        ultimo_delta[i] = (uint16_t)delta;
        if ((uint16_t)delta > maquina[i].max_delta_sesion) {
            maquina[i].max_delta_sesion = (uint16_t)delta;
        }

        int32_t deriv = delta - maquina[i].delta_anterior;
        MaquinaPad_t *p = &maquina[i];

        switch (p->estado) {

        case PAD_ESTADO_REPOSO:
            /* Realizo un filtrado EMA (Media Móvil Exponencial) muy lento para
             * auto-corregir el cero si la temperatura del ambiente cambia. */
            if (delta < (int32_t)(PADS_UMBRAL_DELTA / 4U)) {
                linea_base[i] = (uint16_t)(((uint32_t)linea_base[i] * 255U + v_raw) >> 8U);
            }

            /* ¿El impacto rompe mi umbral y además sube muy rápido? ¡Es un golpe real! */
            if ((delta >= (int32_t)PADS_UMBRAL_DELTA) && (deriv >= (int32_t)PADS_PENDIENTE_MINIMA)) {
                
                /* SPRINT DE CAZA (Peak Hunting): Bloqueo el hilo durante 4ms y 
                 * leo a la máxima velocidad que da el hardware para asegurarme 
                 * de capturar la punta matemática exacta del voltaje del piezo. */
                uint16_t pico_maximo = (uint16_t)delta;
                uint32_t inicio_sprint = HAL_GetTick(); 
                
                while ((HAL_GetTick() - inicio_sprint) < 4U) {
                    uint16_t v_fast = Pads_LeerCanal(canales_adc[i]);
                    int32_t d_fast = (int32_t)v_fast - (int32_t)linea_base[i];
                    if (d_fast < 0) d_fast = -d_fast;
                    if (d_fast > pico_maximo) pico_maximo = (uint16_t)d_fast;
                }

                /* Como la fuerza muscular humana y la compresión de la goma EVA
                 * no son lineales, aplico una curva exponencial cúbica (ratio^3). 
                 * Esto expande mi rango dinámico y me permite distinguir bien entre
                 * golpes suaves y fuertes en el código principal. */
                float ratio = (float)pico_maximo / (float)rango_max_fuerza[i];
                if (ratio > 1.0f) ratio = 1.0f; 

                uint32_t pct = (uint32_t)(ratio * ratio * ratio * 100.0f);
                if (pct == 0U)  pct = 1U;
                if (pct > 100U) pct = 100U;

                fuerza_porcentaje[i] = (uint8_t)pct;
                hay_golpe_flag[i]    = 1U;

                printf("[PADS] Golpe pad %d  pico_real=%u  fuerza=%u%%\r\n", i, pico_maximo, (unsigned)pct);

                /* Armo el cooldown para evitar rebotes físicos de la membrana */
                p->estado                = PAD_ESTADO_ENFRIAMIENTO;
                p->tick_fin_enfriamiento = ahora + PADS_ENFRIAMIENTO_MS;
                p->delta_anterior        = 0;
            }
            break;

        case PAD_ESTADO_ENFRIAMIENTO:
            /* Paso el tiempo sordo, esperando a que la membrana física se estabilice */
            if (ahora >= p->tick_fin_enfriamiento) {
                p->estado         = PAD_ESTADO_REPOSO;
                p->delta_anterior = 0;
            }
            continue; /* Salto para no contaminar la derivada durante la bajada */

        default:
            p->estado = PAD_ESTADO_REPOSO;
            break;
        }

        p->delta_anterior = delta;
    }

    /* Print de debug periódico para observar mis picos máximos */
    if ((ahora - ultimo_print_dbg) >= 2000U) {
        ultimo_print_dbg = ahora;
        uint8_t hay_actividad = 0U;
        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            if (maquina[i].max_delta_sesion > 40U) { hay_actividad = 1U; break; }
        }
        if (hay_actividad) {
            printf("[PADS DBG] max_delta: p0=%u p1=%u p2=%u p3=%u\r\n",
                   maquina[0].max_delta_sesion, maquina[1].max_delta_sesion,
                   maquina[2].max_delta_sesion, maquina[3].max_delta_sesion);
            for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) maquina[i].max_delta_sesion = 0U;
        }
    }
}

uint8_t Pads_HayGolpe(uint8_t pad_id) {
    if (pad_id >= PADS_NUM_CANALES) return 0U;
    uint8_t f = hay_golpe_flag[pad_id];
    hay_golpe_flag[pad_id] = 0U;
    return f;
}

uint8_t Pads_GetFuerza(uint8_t pad_id) {
    if (pad_id >= PADS_NUM_CANALES) return 0U;
    return fuerza_porcentaje[pad_id];
}

uint16_t Pads_GetRawDelta(uint8_t pad_id) {
    if (pad_id >= PADS_NUM_CANALES) return 0U;
    return ultimo_delta[pad_id];
}

void Pads_CalibrarSensibilidad(void) {
    printf("\r\n======================================================\r\n");
    printf("   CALIBRACION INTERACTIVA DE FUERZA MAXIMA          \r\n");
    printf("======================================================\r\n");

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        if (!pad_habilitado[i]) continue;

        printf("\n>>> GOLPEA EL PAD %d CON TODA TU FUERZA (5 s) <<<\r\n", i);
        uint32_t inicio       = osKernelGetTickCount();
        uint16_t max_detectado = 0U;

        /* Escucho durante 5 segundos guardando el impacto más salvaje del jugador */
        while ((osKernelGetTickCount() - inicio) < 5000U) {
            uint16_t v  = Pads_LeerCanal(canales_adc[i]);
            int32_t  d  = (int32_t)v - (int32_t)linea_base[i];
            uint16_t md = (uint16_t)((d < 0) ? -d : d);
            if (md > max_detectado) max_detectado = md; 
            osDelay(2U);
        }

        if (max_detectado < PADS_UMBRAL_DELTA) {
            printf(" -> Sin golpe. Usando valor estándar (1500 cuentas).\r\n");
            rango_max_fuerza[i] = 1500U;
        } else {
            printf(" -> ¡Registrado! El 100%% físico serán %u cuentas.\r\n", max_detectado);
            rango_max_fuerza[i] = max_detectado;
        }
    }
    printf("\n======================================================\r\n\n");
}
