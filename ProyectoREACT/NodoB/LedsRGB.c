/**
 ******************************************************************************
 * @file    LedsRGB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Driver WS2812B - dos lineas independientes con TIM4_CH1 y TIM4_CH2.
 *
 * Calculos (TIM4 clock = 90 MHz cuando SYSCLK = 180 MHz):
 *   Periodo timer = 113 cuentas -> ARR = 112 -> bit_time = 1.256 us
 *   T0H (bit 0 alto) ~ 0.40 us -> CCR = 36
 *   T1H (bit 1 alto) ~ 0.80 us -> CCR = 72
 *
 * BUG CORREGIDO (vs version anterior):
 *   Antes enlazaba el DMA a TIM_DMA_ID_UPDATE, pero HAL_TIM_PWM_Start_DMA
 *   usa internamente TIM_DMA_ID_CC1/CC2. Resultado: DMA nunca arrancaba,
 *   CCR1 se quedaba en 0 y los pines no generaban PWM (LEDs apagados).
 *   Solucion: enlazar a TIM_DMA_ID_CC1 / TIM_DMA_ID_CC2 con los streams
 *   correctos del DMA mapeados a las requests TIM4_CH1 / TIM4_CH2.
 *
 * Orden de bytes en el cable WS2812B: G - R - B, cada byte MSB primero.
 ******************************************************************************
 */

#include "LedsRGB.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <string.h>

// === PARAMETROS DEL TIMING WS2812B ===
#define WS_ARR           112U
#define WS_DUTY_0        36U
#define WS_DUTY_1        72U
#define WS_RESET_CYCLES  40U

#define WS_BITS_POR_LED  24U
#define WS_BUFFER_LEN    ((WS2812_LEDS_POR_ANILLO * WS_BITS_POR_LED) + WS_RESET_CYCLES)

// === HANDLES Y BUFFERS ===
static TIM_HandleTypeDef htim_ws;
static DMA_HandleTypeDef hdma_ws_ch1;        /* TIM4_CH1 -> DMA1_Stream0_Ch2 */
static DMA_HandleTypeDef hdma_ws_ch2;        /* TIM4_CH2 -> DMA1_Stream3_Ch2 */
static uint16_t          ws_buffer[WS2812_NUM_ANILLOS][WS_BUFFER_LEN];
static uint8_t           ws_grb[WS2812_NUM_ANILLOS][WS2812_LEDS_POR_ANILLO][3];
static volatile uint8_t  ws_busy[WS2812_NUM_ANILLOS] = { 0U, 0U };

// === UTILIDADES INTERNAS ===
static uint8_t Aplicar_Brillo(uint8_t v) {
    return (uint8_t)(((uint16_t)v * WS2812_MAX_BRIGHTNESS) / 255U);
}

static void Codificar_Pixel_En_Buffer(uint8_t anillo_id, uint16_t idx) {
    uint16_t base = idx * WS_BITS_POR_LED;
    uint8_t  canales[3];
    uint8_t  c;
    uint8_t  i;
    uint8_t  bit;

    canales[0] = ws_grb[anillo_id][idx][0];   /* G */
    canales[1] = ws_grb[anillo_id][idx][1];   /* R */
    canales[2] = ws_grb[anillo_id][idx][2];   /* B */

    for (c = 0U; c < 3U; c++) {
        for (i = 0U; i < 8U; i++) {
            bit = (canales[c] >> (7U - i)) & 0x01U;
            ws_buffer[anillo_id][base + (c * 8U) + i] = (bit) ? WS_DUTY_1 : WS_DUTY_0;
        }
    }
}

static void Pintar_Anillo_Bloqueante(uint8_t anillo_id, uint32_t channel) {
    uint16_t i;

    /* 1. Codifico todos los pixels del anillo en su buffer DMA */
    for (i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
        Codificar_Pixel_En_Buffer(anillo_id, i);
    }

    /* 2. Disparo la transferencia PWM-DMA */
    ws_busy[anillo_id] = 1U;
    HAL_TIM_PWM_Start_DMA(&htim_ws, channel,
                          (uint32_t *)ws_buffer[anillo_id], WS_BUFFER_LEN);

    /* 3. Espero a que el callback marque ws_busy = 0 */
    while (ws_busy[anillo_id]) {
        osDelay(1U);
    }
}

// === API PUBLICA ===
void LedsRGB_Init(void) {
    GPIO_InitTypeDef        gpio = {0};
    TIM_OC_InitTypeDef      oc   = {0};
    TIM_ClockConfigTypeDef  clk  = {0};
    TIM_MasterConfigTypeDef mst  = {0};
    uint16_t                k;
    uint8_t                 a;

    // 1. Relojes
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // 2. PB6 (TIM4_CH1, AF2) - anillo 0
    gpio.Pin       = GPIO_PIN_6;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOB, &gpio);

    // 3. PD13 (TIM4_CH2, AF2) - anillo 1
    gpio.Pin       = GPIO_PIN_13;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(GPIOD, &gpio);

    // 4. TIM4 base
    htim_ws.Instance               = TIM4;
    htim_ws.Init.Prescaler         = 0U;
    htim_ws.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim_ws.Init.Period            = WS_ARR;
    htim_ws.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim_ws.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim_ws);

    clk.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim_ws, &clk);

    mst.MasterOutputTrigger = TIM_TRGO_RESET;
    mst.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim_ws, &mst);

    // 5. Configuro CH1 y CH2 en modo PWM1
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0U;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim_ws, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim_ws, &oc, TIM_CHANNEL_2);

    // 6. DMA para TIM4_CH1 (PB6) -> DMA1_Stream0 Channel 2
    hdma_ws_ch1.Instance                 = DMA1_Stream0;
    hdma_ws_ch1.Init.Channel             = DMA_CHANNEL_2;
    hdma_ws_ch1.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_ws_ch1.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_ws_ch1.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_ws_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_ws_ch1.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_ws_ch1.Init.Mode                = DMA_NORMAL;
    hdma_ws_ch1.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_ws_ch1.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_ws_ch1);
    __HAL_LINKDMA(&htim_ws, hdma[TIM_DMA_ID_CC1], hdma_ws_ch1);

    // 7. DMA para TIM4_CH2 (PD13) -> DMA1_Stream3 Channel 2
    hdma_ws_ch2.Instance                 = DMA1_Stream3;
    hdma_ws_ch2.Init.Channel             = DMA_CHANNEL_2;
    hdma_ws_ch2.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_ws_ch2.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_ws_ch2.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_ws_ch2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_ws_ch2.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_ws_ch2.Init.Mode                = DMA_NORMAL;
    hdma_ws_ch2.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_ws_ch2.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    HAL_DMA_Init(&hdma_ws_ch2);
    __HAL_LINKDMA(&htim_ws, hdma[TIM_DMA_ID_CC2], hdma_ws_ch2);

    // 8. Habilito IRQs de los DMAs
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);

    // 9. Cola de RESET final (zeros) en ambos buffers
    for (a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        for (k = (WS2812_LEDS_POR_ANILLO * WS_BITS_POR_LED); k < WS_BUFFER_LEN; k++) {
            ws_buffer[a][k] = 0U;
        }
    }

    // 10. Estado inicial: todos los pixels apagados
    LedsRGB_Clear();
    LedsRGB_Show();
}

void LedsRGB_Clear(void) {
    memset(ws_grb, 0, sizeof(ws_grb));
}

void LedsRGB_SetPixel(uint8_t anillo_id, uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (anillo_id >= WS2812_NUM_ANILLOS) return;
    if (idx >= WS2812_LEDS_POR_ANILLO)   return;
    ws_grb[anillo_id][idx][0] = Aplicar_Brillo(g);
    ws_grb[anillo_id][idx][1] = Aplicar_Brillo(r);
    ws_grb[anillo_id][idx][2] = Aplicar_Brillo(b);
}

void LedsRGB_FillAnillo(uint8_t anillo_id, uint8_t r, uint8_t g, uint8_t b) {
    uint16_t i;
    if (anillo_id >= WS2812_NUM_ANILLOS) return;
    for (i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
        LedsRGB_SetPixel(anillo_id, i, r, g, b);
    }
}

void LedsRGB_Show(void) {
    /* Refresco secuencial: primero anillo 0 (PB6), luego anillo 1 (PD13). */
    Pintar_Anillo_Bloqueante(0U, TIM_CHANNEL_1);
    Pintar_Anillo_Bloqueante(1U, TIM_CHANNEL_2);
}

void LedsRGB_TestAllSolid(uint8_t r, uint8_t g, uint8_t b) {
    LedsRGB_FillAnillo(0U, r, g, b);
    LedsRGB_FillAnillo(1U, r, g, b);
    LedsRGB_Show();
}

void LedsRGB_Scan(void) {
    uint16_t i;
    uint8_t  a;

    for (a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        for (i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
            LedsRGB_Clear();
            LedsRGB_SetPixel(a, i, 255U, 255U, 255U);  /* blanco brillo limitado */
            LedsRGB_Show();
            osDelay(200U);
        }
    }
    LedsRGB_Clear();
    LedsRGB_Show();
}

// === CALLBACK DE FIN DE DMA - marca el anillo como libre ===
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance != TIM4) return;

    if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        ws_busy[0] = 0U;
    } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
        ws_busy[1] = 0U;
    }
}

// === IRQ HANDLERS - redirigen a HAL ===
void DMA1_Stream0_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_ws_ch1);
}

void DMA1_Stream3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_ws_ch2);
}
