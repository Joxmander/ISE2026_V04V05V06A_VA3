/**
 ******************************************************************************
 * @file    Pads.c
 * @author  Jose Vargas Gonzaga
 * @brief   Driver de pads piezo (mockup) - lectura por software polling con ADC3.
 *
 * Estrategia:
 *  - ADC3 modo single-conversion, dos canales reconfigurados en cada lectura:
 *      Pad 0 -> ADC_CHANNEL_13 (PC3)
 *      Pad 1 -> ADC_CHANNEL_8  (PF10)
 *  - Llamar Pads_Poll() cada 2 ms minimo para no perderse picos breves del piezo.
 *  - Si raw > PADS_UMBRAL_GOLPE -> marca hay_golpe_flag con debounce temporal.
 ******************************************************************************
 */

#include "Pads.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"

// === VARIABLES PRIVADAS ===
static ADC_HandleTypeDef hadc_pads;
static uint16_t          raw_value[PADS_NUM_CANALES];
static uint8_t           hay_golpe_flag[PADS_NUM_CANALES];
static uint32_t          ultimo_golpe_tick[PADS_NUM_CANALES];

// === FUNCION INTERNA: seleccionar canal y leer 1 muestra ===
static uint16_t Pads_LeerCanal(uint32_t adc_channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = adc_channel;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;   /* Piezo = alta impedancia: validado por el equipo */
    HAL_ADC_ConfigChannel(&hadc_pads, &sConfig);

    HAL_ADC_Start(&hadc_pads);
    if (HAL_ADC_PollForConversion(&hadc_pads, 2U) == HAL_OK) {
        return (uint16_t)HAL_ADC_GetValue(&hadc_pads);
    }
    return 0U;
}

// === API PUBLICA ===
void Pads_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    uint8_t i;

    // 1. Habilito relojes de GPIOC, GPIOF y ADC3
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_ADC3_CLK_ENABLE();

    // 2. PC3 en modo analogico, sin pull
    GPIO_InitStruct.Pin  = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // 3. PF10 en modo analogico, sin pull
    GPIO_InitStruct.Pin  = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // 4. ADC3 configurado para una sola conversion a la vez
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

    // 5. Limpio estado interno
    for (i = 0U; i < PADS_NUM_CANALES; i++) {
        raw_value[i]         = 0U;
        hay_golpe_flag[i]    = 0U;
        ultimo_golpe_tick[i] = 0U;
    }
}

void Pads_Poll(void) {
    /* Tabla de canales por pad: idx 0 = PC3 (IN13), idx 1 = PF10 (IN8) */
    uint32_t adc_channels[PADS_NUM_CANALES] = { ADC_CHANNEL_13, ADC_CHANNEL_8 };
    uint32_t ahora = osKernelGetTickCount();
    uint8_t  i;
    uint16_t v;

    for (i = 0U; i < PADS_NUM_CANALES; i++) {
        v = Pads_LeerCanal(adc_channels[i]);
        raw_value[i] = v;

        if (v >= PADS_UMBRAL_GOLPE) {
            if ((ahora - ultimo_golpe_tick[i]) > PADS_DEBOUNCE_MS) {
                hay_golpe_flag[i]    = 1U;
                ultimo_golpe_tick[i] = ahora;
            }
        }
    }
}

uint8_t Pads_HayGolpe(uint8_t pad_id) {
    uint8_t f;
    if (pad_id >= PADS_NUM_CANALES) return 0U;
    f = hay_golpe_flag[pad_id];
    hay_golpe_flag[pad_id] = 0U;   /* auto-limpia al leer */
    return f;
}

uint16_t Pads_RawValue(uint8_t pad_id) {
    if (pad_id >= PADS_NUM_CANALES) return 0U;
    return raw_value[pad_id];
}
