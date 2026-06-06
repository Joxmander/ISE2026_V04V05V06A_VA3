/**
 ******************************************************************************
 * @file    LedsRGB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Driver WS2812B.
 *
 * Mapeo actualizado a los conectores físicos:
 * Set 1 -> J7  (PB6  / TIM4_CH1)
 * Set 2 -> J8  (PD13 / TIM4_CH2)
 * Set 3 -> J9  (PC6  / TIM3_CH1)
 * Set 4 -> J10 (PC7  / TIM3_CH2)
 ******************************************************************************
 */

#include "LedsRGB.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <string.h>

#define WS_ARR           112U
#define WS_DUTY_0        36U
#define WS_DUTY_1        72U
#define WS_RESET_CYCLES  40U
#define WS_BITS_POR_LED  24U
#define WS_BUFFER_LEN    ((WS2812_LEDS_POR_ANILLO * WS_BITS_POR_LED) + WS_RESET_CYCLES)

static TIM_HandleTypeDef htim4_ws;
static TIM_HandleTypeDef htim3_ws;

static DMA_HandleTypeDef hdma_ws_ch0; 
static DMA_HandleTypeDef hdma_ws_ch1; 
static DMA_HandleTypeDef hdma_ws_ch2; 
static DMA_HandleTypeDef hdma_ws_ch3; 

static uint16_t          ws_buffer[WS2812_NUM_ANILLOS][WS_BUFFER_LEN];
static uint8_t           ws_grb[WS2812_NUM_ANILLOS][WS2812_LEDS_POR_ANILLO][3];
static volatile uint8_t  ws_busy[WS2812_NUM_ANILLOS] = { 0U, 0U, 0U, 0U };

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
    for (uint16_t i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
        Codificar_Pixel_En_Buffer(anillo_id, i);
    }

    ws_busy[anillo_id] = 1U;

    if (anillo_id == 0U) {
        HAL_TIM_PWM_Start_DMA(&htim4_ws, TIM_CHANNEL_1, (uint32_t *)ws_buffer[0], WS_BUFFER_LEN); /* Set 1 -> J7 (PB6) */
    } else if (anillo_id == 1U) {
        HAL_TIM_PWM_Start_DMA(&htim4_ws, TIM_CHANNEL_2, (uint32_t *)ws_buffer[1], WS_BUFFER_LEN); /* Set 2 -> J8 (PD13) */
    } else if (anillo_id == 2U) {
        HAL_TIM_PWM_Start_DMA(&htim3_ws, TIM_CHANNEL_1, (uint32_t *)ws_buffer[2], WS_BUFFER_LEN); /* Set 3 -> J9 (PC6) */
    } else if (anillo_id == 3U) {
        HAL_TIM_PWM_Start_DMA(&htim3_ws, TIM_CHANNEL_2, (uint32_t *)ws_buffer[3], WS_BUFFER_LEN); /* Set 4 -> J10 (PC7) */
    }

    while (ws_busy[anillo_id]) { osDelay(1U); }
}

void LedsRGB_Init(void) {
    GPIO_InitTypeDef        gpio = {0};
    TIM_OC_InitTypeDef      oc   = {0};
    TIM_ClockConfigTypeDef  clk  = {0};


    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;

    gpio.Alternate = GPIO_AF2_TIM4;
    gpio.Pin       = GPIO_PIN_6;  HAL_GPIO_Init(GPIOB, &gpio); 
    gpio.Pin       = GPIO_PIN_13; HAL_GPIO_Init(GPIOD, &gpio); 

    gpio.Alternate = GPIO_AF2_TIM3;
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7; HAL_GPIO_Init(GPIOC, &gpio); 

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

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

/* Test hardware: un anillo a la vez, 2 s ON + 1 s OFF */
for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
    LedsRGB_Clear();
    LedsRGB_FillAnillo(a, 0U, 50U, 0U);   /* verde tenue */
    LedsRGB_Show();
    osDelay(2000U);
    LedsRGB_Clear();
    LedsRGB_Show();
    osDelay(1000U);
}
}

void LedsRGB_Clear(void) {
    memset(ws_grb, 0, sizeof(ws_grb));
}

void LedsRGB_SetPixel(uint8_t anillo_id, uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (anillo_id >= WS2812_NUM_ANILLOS || idx >= WS2812_LEDS_POR_ANILLO) return;
    ws_grb[anillo_id][idx][0] = Aplicar_Brillo(g);
    ws_grb[anillo_id][idx][1] = Aplicar_Brillo(r);
    ws_grb[anillo_id][idx][2] = Aplicar_Brillo(b);
}

void LedsRGB_FillAnillo(uint8_t anillo_id, uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
        LedsRGB_SetPixel(anillo_id, i, r, g, b);
    }
}

void LedsRGB_Show(void) {
    Pintar_Anillo_Bloqueante(0U);
    Pintar_Anillo_Bloqueante(1U);
    Pintar_Anillo_Bloqueante(2U);
    Pintar_Anillo_Bloqueante(3U);
}

static void Hue_a_RGB(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint8_t sector   = hue / 43U;          
    uint8_t fraccion = (hue % 43U) * 6U;   
    uint8_t subida   = fraccion;
    uint8_t bajada   = 255U - fraccion;

    switch (sector) {
        case 0:  *r = 255U;   *g = subida;  *b = 0U;      break; 
        case 1:  *r = bajada; *g = 255U;    *b = 0U;      break; 
        case 2:  *r = 0U;     *g = 255U;    *b = subida;  break; 
        case 3:  *r = 0U;     *g = bajada;  *b = 255U;    break; 
        case 4:  *r = subida; *g = 0U;      *b = 255U;    break; 
        default: *r = 255U;   *g = 0U;      *b = bajada;  break; 
    }
}

void LedsRGB_ArcoirisAll(uint8_t vueltas) {
    const uint8_t N = WS2812_LEDS_POR_ANILLO;
    const uint8_t hue_paso = 255U / N;  
    uint8_t r, g, b;

    for (uint8_t led = 0U; led < N; led++) {
        Hue_a_RGB((uint8_t)(led * hue_paso), &r, &g, &b);
        for(uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
            LedsRGB_SetPixel(a, led, r, g, b);
        }
        LedsRGB_Show();
        osDelay(80U);   
    }

    for (uint8_t v = 0U; v < vueltas; v++) {
        for (uint8_t paso = 0U; paso < N; paso++) {
            for (uint8_t led = 0U; led < N; led++) {
                uint8_t hue = (uint8_t)(((uint16_t)(led + paso) * hue_paso) & 0xFFU);
                Hue_a_RGB(hue, &r, &g, &b);
                for(uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
                    LedsRGB_SetPixel(a, led, r, g, b);
                }
            }
            LedsRGB_Show();
            osDelay(80U);
        }
    }
    LedsRGB_Clear();
    LedsRGB_Show();
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM4) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
            ws_busy[0] = 0U; /* Set 1 */
        } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
            ws_busy[1] = 0U; /* Set 2 */
        }
    } else if (htim->Instance == TIM3) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
            ws_busy[2] = 0U; /* Set 3 */
        } else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
            ws_busy[3] = 0U; /* Set 4 */
        }
    }
}

void DMA1_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch0); }
void DMA1_Stream3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch1); }
void DMA1_Stream4_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch2); }
void DMA1_Stream5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch3); }
