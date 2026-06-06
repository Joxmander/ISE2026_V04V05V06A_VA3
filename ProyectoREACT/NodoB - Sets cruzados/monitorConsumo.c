#include "monitorConsumo.h"
#include "cmsis_os2.h"

static EstadoEnergia_t estado_actual = MODO_PWR_IDLE;
static ADC_HandleTypeDef hadc1;

/**
 * @brief Configura el pin PA0 y el periférico ADC1 para leer el potenciómetro
 */
void Monitor_Init(void) {
    estado_actual = MODO_PWR_IDLE;

    // 1. Habilitamos los relojes del Puerto A y del ADC1
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    // 2. Configuramos el pin PA0 en modo Analógico
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 3. Configuramos el hardware del ADC1
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B; // Resolución de 12 bits (0 a 4095)
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    // 4. Configuramos el Canal 0 (PA0)
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES; // Tiempo de muestreo estable
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

void Monitor_SetEstado(EstadoEnergia_t estado) {
    estado_actual = estado;
}

/**
 * @brief Dispara una lectura del potenciómetro, la escala y devuelve mA
 */
uint16_t Monitor_GetConsumo_mA(void) {
    // Si el micro está en modo Sleep, forzamos la lectura a 2mA para la web.
    // (En la vida real, el ADC ni siquiera funcionaría en Sleep).
    if (estado_actual == MODO_PWR_SLEEP) {
        return 2; 
    }

    uint32_t valor_bruto_adc = 0;
    uint16_t miliamperios = 0;

    // Disparamos la lectura del hardware
    HAL_ADC_Start(&hadc1);
    
    // Esperamos un máximo de 5 milisegundos a que termine la conversión
    if (HAL_ADC_PollForConversion(&hadc1, 5) == HAL_OK) {
        valor_bruto_adc = HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    // --- ESCALADO MATEMÁTICO ---
    // Tenemos un número de 0 a 4095. Vamos a simular que el tope del potenciómetro
    // equivale a 200 mA de consumo para que la barra de la web se mueva entera.
    // Regla de 3: mA = (valor_adc * 200) / 4095
    miliamperios = (valor_bruto_adc * 200) / 4095;

    return miliamperios;
}
