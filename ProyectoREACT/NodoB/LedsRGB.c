/**
 ******************************************************************************
 * @file    LedsRGB.c
 * @author  Jose Vargas Gonzaga (Adaptado a 4 Anillos)
 * @brief   Driver WS2812B - cuatro lineas independientes con TIM4 y TIM3.
 *
 * PINES Y MOTORES:
 * Anillo 0 -> PB6  (TIM4_CH1 -> DMA1_Stream0)
 * Anillo 1 -> PD13 (TIM4_CH2 -> DMA1_Stream3)
 * Anillo 2 -> PC6  (TIM3_CH1 -> DMA1_Stream4)
 * Anillo 3 -> PC7  (TIM3_CH2 -> DMA1_Stream5)
 *
 * BUG CORREGIDO (vs version anterior):
 * Enlazar a TIM_DMA_ID_CC1 / TIM_DMA_ID_CC2 con los streams correctos
 * del DMA mapeados a las requests de los canales correspondientes.
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
static TIM_HandleTypeDef htim4_ws;
static TIM_HandleTypeDef htim3_ws;

static DMA_HandleTypeDef hdma_ws_ch0; /* TIM4_CH1 -> DMA1_Stream0_Ch2 */
static DMA_HandleTypeDef hdma_ws_ch1; /* TIM4_CH2 -> DMA1_Stream3_Ch2 */
static DMA_HandleTypeDef hdma_ws_ch2; /* TIM3_CH1 -> DMA1_Stream4_Ch5 */
static DMA_HandleTypeDef hdma_ws_ch3; /* TIM3_CH2 -> DMA1_Stream5_Ch5 */

static uint16_t          ws_buffer[WS2812_NUM_ANILLOS][WS_BUFFER_LEN];
static uint8_t           ws_grb[WS2812_NUM_ANILLOS][WS2812_LEDS_POR_ANILLO][3];
static volatile uint8_t  ws_busy[WS2812_NUM_ANILLOS] = { 0U, 0U, 0U, 0U };

// === UTILIDADES INTERNAS ===
static uint8_t Aplicar_Brillo(uint8_t v) {
    return (uint8_t)(((uint16_t)v * WS2812_MAX_BRIGHTNESS) / 255U);
}

static void Codificar_Pixel_En_Buffer(uint8_t anillo_id, uint16_t idx) {
    uint16_t base = idx * WS_BITS_POR_LED;
    uint8_t  canales[3];
    uint8_t  c, i, bit;

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

static void Pintar_Anillo_Bloqueante(uint8_t anillo_id) {
    uint16_t i;

    /* 1. Codifico todos los pixels del anillo en su buffer DMA */
    for (i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
        Codificar_Pixel_En_Buffer(anillo_id, i);
    }

    /* 2. Disparo la transferencia PWM-DMA segun el anillo */
    ws_busy[anillo_id] = 1U;
    
    if (anillo_id == 0U) {
        HAL_TIM_PWM_Start_DMA(&htim4_ws, TIM_CHANNEL_1, (uint32_t *)ws_buffer[0], WS_BUFFER_LEN);
    } else if (anillo_id == 1U) {
        HAL_TIM_PWM_Start_DMA(&htim4_ws, TIM_CHANNEL_2, (uint32_t *)ws_buffer[1], WS_BUFFER_LEN);
    } else if (anillo_id == 2U) {
        HAL_TIM_PWM_Start_DMA(&htim3_ws, TIM_CHANNEL_1, (uint32_t *)ws_buffer[2], WS_BUFFER_LEN);
    } else if (anillo_id == 3U) {
        HAL_TIM_PWM_Start_DMA(&htim3_ws, TIM_CHANNEL_2, (uint32_t *)ws_buffer[3], WS_BUFFER_LEN);
    }

    /* 3. Espero a que el callback marque ws_busy = 0 */
    while (ws_busy[anillo_id]) { osDelay(1U); }
}

// === API PUBLICA ===
void LedsRGB_Init(void) {
    GPIO_InitTypeDef        gpio = {0};
    TIM_OC_InitTypeDef      oc   = {0};
    TIM_ClockConfigTypeDef  clk  = {0};
    uint16_t                k;
    uint8_t                 a;

    // 1. Relojes
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // 2. Configuracion comun de GPIO
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;

    // PB6 (TIM4_CH1) y PD13 (TIM4_CH2) -> AF2
    gpio.Alternate = GPIO_AF2_TIM4;
    gpio.Pin       = GPIO_PIN_6;
    HAL_GPIO_Init(GPIOB, &gpio);
    gpio.Pin       = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOD, &gpio);

    // PC6 (TIM3_CH1) y PC7 (TIM3_CH2) -> AF2
    gpio.Alternate = GPIO_AF2_TIM3;
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    HAL_GPIO_Init(GPIOC, &gpio);

    // 3. Configuracion de TIM4 y TIM3
    htim4_ws.Instance = TIM4;
    htim4_ws.Init.Prescaler = 0U;
    htim4_ws.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4_ws.Init.Period = WS_ARR;
    htim4_ws.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim4_ws);

    htim3_ws.Instance = TIM3;
    htim3_ws.Init.Prescaler = 0U;
    htim3_ws.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3_ws.Init.Period = WS_ARR;
    htim3_ws.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim3_ws);

    clk.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim4_ws, &clk);
    HAL_TIM_ConfigClockSource(&htim3_ws, &clk);

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0U;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim4_ws, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim4_ws, &oc, TIM_CHANNEL_2);
    HAL_TIM_PWM_ConfigChannel(&htim3_ws, &oc, TIM_CHANNEL_1);
    HAL_TIM_PWM_ConfigChannel(&htim3_ws, &oc, TIM_CHANNEL_2);

    // 4. DMA 0 y 1 para TIM4 (PB6 y PD13)
    hdma_ws_ch0.Instance = DMA1_Stream0;
    hdma_ws_ch0.Init.Channel = DMA_CHANNEL_2;
    hdma_ws_ch0.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_ws_ch0.Init.MemInc = DMA_MINC_ENABLE;
    hdma_ws_ch0.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_ws_ch0.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_ws_ch0.Init.Mode = DMA_NORMAL;
    hdma_ws_ch0.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_ws_ch0);
    __HAL_LINKDMA(&htim4_ws, hdma[TIM_DMA_ID_CC1], hdma_ws_ch0);

    hdma_ws_ch1.Instance = DMA1_Stream3;
    hdma_ws_ch1.Init.Channel = DMA_CHANNEL_2;
    hdma_ws_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_ws_ch1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_ws_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_ws_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_ws_ch1.Init.Mode = DMA_NORMAL;
    hdma_ws_ch1.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_ws_ch1);
    __HAL_LINKDMA(&htim4_ws, hdma[TIM_DMA_ID_CC2], hdma_ws_ch1);

    // 5. DMA 2 y 3 para TIM3 (PC6 y PC7)
    hdma_ws_ch2.Instance = DMA1_Stream4;
    hdma_ws_ch2.Init.Channel = DMA_CHANNEL_5;
    hdma_ws_ch2.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_ws_ch2.Init.MemInc = DMA_MINC_ENABLE;
    hdma_ws_ch2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_ws_ch2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_ws_ch2.Init.Mode = DMA_NORMAL;
    hdma_ws_ch2.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_ws_ch2);
    __HAL_LINKDMA(&htim3_ws, hdma[TIM_DMA_ID_CC1], hdma_ws_ch2);

    hdma_ws_ch3.Instance = DMA1_Stream5;
    hdma_ws_ch3.Init.Channel = DMA_CHANNEL_5;
    hdma_ws_ch3.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_ws_ch3.Init.MemInc = DMA_MINC_ENABLE;
    hdma_ws_ch3.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_ws_ch3.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_ws_ch3.Init.Mode = DMA_NORMAL;
    hdma_ws_ch3.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_ws_ch3);
    __HAL_LINKDMA(&htim3_ws, hdma[TIM_DMA_ID_CC2], hdma_ws_ch3);

    // 6. Habilito IRQs de los DMAs
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

    // 7. Cola de RESET final (zeros) en los buffers
    for (a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        for (k = (WS2812_LEDS_POR_ANILLO * WS_BITS_POR_LED); k < WS_BUFFER_LEN; k++) {
            ws_buffer[a][k] = 0U;
        }
    }

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
    /* Refresco secuencial de los 4 anillos */
    Pintar_Anillo_Bloqueante(0U);
    Pintar_Anillo_Bloqueante(1U);
    Pintar_Anillo_Bloqueante(2U);
    Pintar_Anillo_Bloqueante(3U);
}

void LedsRGB_TestAllSolid(uint8_t r, uint8_t g, uint8_t b) {
    LedsRGB_FillAnillo(0U, r, g, b);
    LedsRGB_FillAnillo(1U, r, g, b);
    LedsRGB_FillAnillo(2U, r, g, b);
    LedsRGB_FillAnillo(3U, r, g, b);
    LedsRGB_Show();
}

void LedsRGB_Scan(void) {
    uint16_t i;
    uint8_t  a;

    for (a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        for (i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
            LedsRGB_Clear();
            LedsRGB_SetPixel(a, i, 255U, 255U, 255U);  
            LedsRGB_Show();
            osDelay(150U);
        }
    }
    LedsRGB_Clear();
    LedsRGB_Show();
}

// === CALLBACK DE FIN DE DMA ===
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
            ws_busy[0] = 0U;
        } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
            ws_busy[1] = 0U;
        }
    } else if (htim->Instance == TIM3) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
            ws_busy[2] = 0U;
        } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
            ws_busy[3] = 0U;
        }
    }
}

// === IRQ HANDLERS ===
void DMA1_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch0); }
void DMA1_Stream3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch1); }
void DMA1_Stream4_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch2); }
void DMA1_Stream5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch3); }

