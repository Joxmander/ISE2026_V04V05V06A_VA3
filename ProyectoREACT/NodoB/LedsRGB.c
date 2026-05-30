/**
 ******************************************************************************
 * @file    LedsRGB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del driver de LEDs con acceso directo a memoria (DMA).
 * * Utilizo TIM3 y TIM4 acoplados a 4 flujos de DMA. Esto me permite "escupir"
 * los datos a 800 Kbps sin intervenir con la CPU, dejándola 100% libre 
 * para procesar la lectura de los piezos y la lógica del arcade.
 ******************************************************************************
 */

#include "LedsRGB.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <string.h>

/* --- Tiempos del protocolo WS2812B ---
 * Basado en el reloj de mi microcontrolador, calculo los ticks del Timer
 * para generar los anchos de pulso exactos que codifican un '0' y un '1'. */
#define WS_ARR           112U
#define WS_DUTY_0        36U
#define WS_DUTY_1        72U
#define WS_RESET_CYCLES  40U
#define WS_BITS_POR_LED  24U
#define WS_BUFFER_LEN    ((WS2812_LEDS_POR_ANILLO * WS_BITS_POR_LED) + WS_RESET_CYCLES)

static TIM_HandleTypeDef htim4_ws;
static TIM_HandleTypeDef htim3_ws;

/* Mis 4 flujos de DMA para mandar datos en paralelo a los 4 anillos */
static DMA_HandleTypeDef hdma_ws_ch0; 
static DMA_HandleTypeDef hdma_ws_ch1; 
static DMA_HandleTypeDef hdma_ws_ch2; 
static DMA_HandleTypeDef hdma_ws_ch3; 

/* Buffers de memoria. 'ws_grb' guarda el color lógico (0-255) y 'ws_buffer' 
 * guarda el tren de pulsos PWM final que el DMA empuja a los pines. */
static uint16_t          ws_buffer[WS2812_NUM_ANILLOS][WS_BUFFER_LEN];
static uint8_t           ws_grb[WS2812_NUM_ANILLOS][WS2812_LEDS_POR_ANILLO][3];
static volatile uint8_t  ws_busy[WS2812_NUM_ANILLOS] = { 0U, 0U, 0U, 0U };

/* Escalo el valor RGB ingresado según mi límite máximo de seguridad */
static uint8_t Aplicar_Brillo(uint8_t v) {
    return (uint8_t)(((uint16_t)v * WS2812_MAX_BRIGHTNESS) / 255U);
}

/* Traduzco los canales de color (R, G, B) a los anchos de pulso del Timer.
 * El LED WS2812B espera los datos en orden Verde-Rojo-Azul (GRB). */
static void Codificar_Pixel_En_Buffer(uint8_t anillo_id, uint16_t idx) {
    uint16_t base = idx * WS_BITS_POR_LED;
    uint8_t  canales[3];
    uint8_t  c, i, bit;

    canales[0] = ws_grb[anillo_id][idx][0];   /* Verde */
    canales[1] = ws_grb[anillo_id][idx][1];   /* Rojo  */
    canales[2] = ws_grb[anillo_id][idx][2];   /* Azul  */

    for (c = 0U; c < 3U; c++) {
        for (i = 0U; i < 8U; i++) {
            bit = (canales[c] >> (7U - i)) & 0x01U;
            ws_buffer[anillo_id][base + (c * 8U) + i] = (bit) ? WS_DUTY_1 : WS_DUTY_0;
        }
    }
}

/* Disparo la transferencia DMA para un anillo en concreto y espero a que termine. */
static void Pintar_Anillo_Bloqueante(uint8_t anillo_id) {
    for (uint16_t i = 0U; i < WS2812_LEDS_POR_ANILLO; i++) {
        Codificar_Pixel_En_Buffer(anillo_id, i);
    }

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

    /* El flag se limpiará automáticamente en la interrupción de Fin de Pulso (Callback) */
    while (ws_busy[anillo_id]) { osDelay(1U); }
}

void LedsRGB_Init(void) {
    GPIO_InitTypeDef        gpio = {0};
    TIM_OC_InitTypeDef      oc   = {0};
    TIM_ClockConfigTypeDef  clk  = {0};
    uint16_t                k;
    uint8_t                 a;

    // 1. Habilito los relojes necesarios
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // 2. Configuro los pines físicos de mi PCB como salidas alternativas (AF2)
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;

    gpio.Alternate = GPIO_AF2_TIM4;
    gpio.Pin       = GPIO_PIN_6;  HAL_GPIO_Init(GPIOB, &gpio); // Anillo 0
    gpio.Pin       = GPIO_PIN_13; HAL_GPIO_Init(GPIOD, &gpio); // Anillo 1

    gpio.Alternate = GPIO_AF2_TIM3;
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7; HAL_GPIO_Init(GPIOC, &gpio); // Anillos 2 y 3

    // 3. Inicializo los Timers base (TIM3 y TIM4)
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

    // 4. Enlazo los DMAs con los canales correspondientes
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

    // 5. Habilito interrupciones DMA
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5, 0); HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

    // 6. Configuro la señal de "Reset" (Ceros) al final del tren de pulsos
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

static void LedsRGB_SetPixel(uint8_t anillo_id, uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
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

/* ====================================================================
 * === ANIMACIONES Y EFECTOS
 * ==================================================================== */

/* Convierto un valor Hue (espectro circular de 0 a 255) a componentes RGB. 
 * Divido el círculo de color en 6 sectores para crear un arcoíris perfecto. */
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

/* Animación de bienvenida para el hardware: Construye el arcoíris paso a paso 
 * y luego lo hace rotar sobre el anillo. */
void LedsRGB_Arcoiris(uint8_t anillo_id, uint8_t vueltas) {
    if (anillo_id >= WS2812_NUM_ANILLOS) return;

    const uint8_t N        = WS2812_LEDS_POR_ANILLO;
    const uint8_t hue_paso = 255U / N;  
    uint8_t r, g, b;

    /* Fase 1: Enciendo los LEDs uno a uno (Efecto Build-up) */
    for (uint8_t led = 0U; led < N; led++) {
        Hue_a_RGB((uint8_t)(led * hue_paso), &r, &g, &b);
        LedsRGB_SetPixel(anillo_id, led, r, g, b);
        LedsRGB_Show();
        osDelay(80U);   
    }

    /* Fase 2: Roto todo el patrón de colores (Efecto Spinner) */
    for (uint8_t v = 0U; v < vueltas; v++) {
        for (uint8_t paso = 0U; paso < N; paso++) {
            for (uint8_t led = 0U; led < N; led++) {
                uint8_t hue = (uint8_t)(((uint16_t)(led + paso) * hue_paso) & 0xFFU);
                Hue_a_RGB(hue, &r, &g, &b);
                LedsRGB_SetPixel(anillo_id, led, r, g, b);
            }
            LedsRGB_Show();
            osDelay(80U);
        }
    }

    LedsRGB_Clear();
    LedsRGB_Show();
}

/* ====================================================================
 * === INTERRUPCIONES DEL SISTEMA
 * ==================================================================== */
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

void DMA1_Stream0_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch0); }
void DMA1_Stream3_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch1); }
void DMA1_Stream4_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch2); }
void DMA1_Stream5_IRQHandler(void) { HAL_DMA_IRQHandler(&hdma_ws_ch3); }
