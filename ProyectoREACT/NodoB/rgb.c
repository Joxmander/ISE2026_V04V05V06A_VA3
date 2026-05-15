#include "rgb.h"



/* ============================================================
   CONFIGURACI?N WS2812B
   ============================================================ */

// Frecuencia WS2812B = 800 kHz ? periodo = 1.25 us
// Usamos un timer a 84 MHz (APB1 * 2 en F429)
#define TIMER_CLOCK     84000000UL
#define WS2812_FREQ     800000UL
#define WS2812_PERIOD   104//(TIMER_CLOCK / WS2812_FREQ)   // ? 105 ticks

// Duraci?n del bit "0" y "1" en ticks del timer
#define WS2812_T0H   20//((uint16_t)(WS2812_PERIOD * 32 / 100))  // ~0.4us
#define WS2812_T1H   80//((uint16_t)(WS2812_PERIOD * 64 / 100))  // ~0.8us

// LEDs por anillo
#define LEDS_PER_RING   8

// Cada LED = 24 bits ? 24 pulsos PWM
#define BITS_PER_LED    24

// Buffer total para un anillo
//static uint16_t ws_buffer[LEDS_PER_RING * BITS_PER_LED];



/*----------------------------------------------------------------------------
 *      Variables del modulo
 *---------------------------------------------------------------------------*/
 
osThreadId_t tid_rgb;                        // thread id
osMessageQueueId_t id_cola_rgb;

/* Buffer de LEDs: 4 anillos ? 8 LEDs ? 3 colores */
//static rgb_color_t led_buffer[RGB_NUM_RINGS][RGB_LEDS_PER_RING];

/* Buffer global para un anillo (8 LEDs ? 24 bits) */
static uint16_t ws_buffer[LEDS_PER_RING * BITS_PER_LED];


///* Handle del timer y DMA */
//extern TIM_HandleTypeDef htim4;
//extern DMA_HandleTypeDef hdma_tim4_ch1;
TIM_HandleTypeDef htim4;
DMA_HandleTypeDef hdma_tim4_ch1;

/*Prototipos de funciones*/
void Th_rgb (void *argument);                   // thread function

//static void rgb_set_led(uint8_t ring, uint8_t index, rgb_color_t c);
static void rgb_update_ring(uint8_t ring, rgb_color_t color);
static void select_gpio_for_ring(uint8_t ring, GPIO_TypeDef **port, uint16_t *pin);
//static void ws2812_fill_buffer(rgb_color_t color);
//static void select_gpio_for_ring(uint8_t ring, GPIO_TypeDef **port, uint16_t *pin);

static void ws2812_fill_buffer(rgb_color_t color);
void Init_pins(void);
void Init_tim4(void);
void Init_pin_tim(void);
 
 
 /******************HILO TEST**********/
void Th_test_rgb(void *argument);

 /******************************/
 
 /* ============================================================
   ADAPTADOR PARA LA MÁQUINA DE ESTADOS (FSM)
   ============================================================ */

void rgb_encender_pad(uint8_t pad, rgb_color_t color) {
    rgb_msg_t msg;
    msg.cmd = RGB_CMD_SET_COLOR;
    msg.ring_id = pad;
    msg.color = color;
    // Mandamos el mensaje a la cola sin bloquear (timeout 0)
    osMessageQueuePut(id_cola_rgb, &msg, 0, 0); 
}

void rgb_apagar_pad(uint8_t pad) {
    rgb_msg_t msg;
    msg.cmd = RGB_CMD_OFF;
    msg.ring_id = pad;
    osMessageQueuePut(id_cola_rgb, &msg, 0, 0);
}

void rgb_encender_todos(rgb_color_t color) {
    for (uint8_t i = 0; i < RGB_NUM_RINGS; i++) {
        rgb_encender_pad(i, color);
    }
}

void rgb_apagar_todos(void) {
    for (uint8_t i = 0; i < RGB_NUM_RINGS; i++) {
        rgb_apagar_pad(i);
    }
}
 
int Init_Th_rgb (void) {
 
  id_cola_rgb = osMessageQueueNew(16, sizeof(rgb_msg_t), NULL);
  tid_rgb = osThreadNew(Th_rgb, NULL, NULL);
  if (tid_rgb == NULL) {
    return(-1);
  }
 
  return(0);
}
 
void Th_rgb (void *argument) {
    rgb_msg_t msg;

    while (1)
      {
        if (osMessageQueueGet(id_cola_rgb, &msg, NULL, osWaitForever) == osOK)
        {
            switch (msg.cmd)
            {
                case RGB_CMD_SET_COLOR:
                    rgb_update_ring(msg.ring_id, msg.color);
                    break;

                case RGB_CMD_OFF:
                    rgb_update_ring(msg.ring_id, (rgb_color_t){0,0,0});
                    break;
            }
        }

        osThreadYield();
    }
}

osMessageQueueId_t rgb_get_queue(void){
    return id_cola_rgb;
}

/*Escribir un led*/
// Como usare un color por anillo no necesito esta funcion ya que esta sirve para 
// controlar cada led del anillo

//static void rgb_set_led(uint8_t ring, uint8_t index, rgb_color_t c){
//    if (ring >= RGB_NUM_RINGS || index >= RGB_LEDS_PER_RING)
//        return;

//    led_buffer[ring][index] = c;
//}


void Init_pins(void){
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  // PB6 -> TIM4_CH1 (AF2)
//  GPIO_InitStruct.Pin = GPIO_PIN_6;
//  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;        // Alternate Function Push-Pull
//  GPIO_InitStruct.Pull = GPIO_NOPULL;
//  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//  GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;     // AF2 = TIM4_CH1
//  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  //Configuracion de todos los pines como GPIO OUTPUT LOW
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  
  
  //Anillo 1 - PB6
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  // Anillo 2 - PD13
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);

  // Anillo 3 - PD12
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);

  // Anillo 4 - PE5
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, GPIO_PIN_RESET);
  
}


/* Llamar Init_tim4() o similar */
void Init_tim4(void){
  __HAL_RCC_TIM4_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* ---------------- Timer 4 base ---------------- */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;                       // Timer a 84 MHz
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = WS2812_PERIOD;                        // 84MHz / (104+1) ? 800kHz
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_PWM_Init(&htim4);

  /* ---------------- Canal PWM CH1 ---------------- */
  TIM_OC_InitTypeDef sConfigOC = {0};
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;                            // Duty inicial 0%
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim4, &sConfigOC, TIM_CHANNEL_1);

  /* ---------------- DMA para TIM4_CH1 ---------------- */
  hdma_tim4_ch1.Instance = DMA1_Stream0;
  hdma_tim4_ch1.Init.Channel = DMA_CHANNEL_2;     // TIM4_CH1 ? Channel 2
  hdma_tim4_ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
  hdma_tim4_ch1.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma_tim4_ch1.Init.MemInc = DMA_MINC_ENABLE;
  hdma_tim4_ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  hdma_tim4_ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  hdma_tim4_ch1.Init.Mode = DMA_NORMAL;
  hdma_tim4_ch1.Init.Priority = DMA_PRIORITY_HIGH;
  hdma_tim4_ch1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
  HAL_DMA_Init(&hdma_tim4_ch1);

  /* Vincular DMA al timer (canal 1) */
  __HAL_LINKDMA(&htim4, hdma[TIM_DMA_ID_CC1], hdma_tim4_ch1);

  /* ---------------- NVIC para DMA ---------------- */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}



/* -------------------------------------------------
   Env?a un color s?lido a un anillo WS2812B
   usando TIM4_CH1 + DMA sobre PB6.

   De momento ignoramos 'ring' y asumimos que
   solo hay un anillo en PB6.
   ------------------------------------------------- */
static void rgb_update_ring(uint8_t ring, rgb_color_t color){
//    (void)ring; // Evita warning de par?metro no usado por ahora
    GPIO_TypeDef *port;
    uint16_t pin;
  
    // 1. Seleccionar el pin del anillo
    select_gpio_for_ring(ring, &port, &pin);
    if (port == NULL)
        return;

    // 2. Rellenar buffer GRB
    ws2812_fill_buffer(color);

    // 3. Resetear Timer y DMA
    __HAL_TIM_DISABLE(&htim4);
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 0);
    __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);

    __HAL_DMA_DISABLE(&hdma_tim4_ch1);
    __HAL_DMA_CLEAR_FLAG(&hdma_tim4_ch1,
                         DMA_FLAG_TCIF0_4 | DMA_FLAG_HTIF0_4 | DMA_FLAG_TEIF0_4);
    hdma_tim4_ch1.State = HAL_DMA_STATE_READY;

    // 4. Configurar el pin como AF_TIM4_CH1
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(port, &GPIO_InitStruct);

    // 5. Iniciar PWM + DMA
    HAL_TIM_PWM_Start_DMA(&htim4,
                          TIM_CHANNEL_1,
                          (uint32_t *)ws_buffer,
                          LEDS_PER_RING * BITS_PER_LED);

    // 6. Esperar fin de DMA
    while (hdma_tim4_ch1.State != HAL_DMA_STATE_READY) {}

    // 7. Esperar fin del ?ltimo pulso PWM
    while (__HAL_TIM_GET_FLAG(&htim4, TIM_FLAG_UPDATE) == RESET) {}
    __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);

    // 8. Detener PWM + DMA
    HAL_TIM_PWM_Stop_DMA(&htim4, TIM_CHANNEL_1);

    // 9. Forzar LOW real en el pin
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);

    // 10. Reset > 50us
    HAL_Delay(1);

    // 11. Volver a AF para el siguiente frame
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
//  
//    // 1. Rellenar el buffer de pulsos PWM para este color
//    ws2812_fill_buffer(color);

//    // 2. Iniciar PWM con DMA:
//    //    - Timer: TIM4
//    //    - Canal: CH1
//    //    - Buffer: ws_buffer
//    //    - Longitud: n? de bits totales (8 LEDs ? 24 bits)
//    HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_1, (uint32_t *)ws_buffer, LEDS_PER_RING * BITS_PER_LED);

//    // 3. Esperar a que el DMA termine la transferencia
//    //    (el HAL actualiza el estado del DMA)
//////    while (hdma_tim4_ch1.State != HAL_DMA_STATE_READY)
//////    {
//////        // Aqu? podr?as hacer osThreadYield() si quieres
//////        // que el hilo no bloquee tanto.
//////    }
//  // 3. Esperar a que el DMA termine
//    while (hdma_tim4_ch1.State != HAL_DMA_STATE_READY);

//    // 3.1 Esperar a que el timer termine el ?ltimo ciclo PWM
//    while (__HAL_TIM_GET_FLAG(&htim4, TIM_FLAG_UPDATE) == RESET);

//    // 4. Detener el PWM + DMA
//    HAL_TIM_PWM_Stop_DMA(&htim4, TIM_CHANNEL_1);

//    // 4.1 Cambiar PB6 a GPIO OUTPUT
//    GPIO_InitTypeDef GPIO_InitStruct = {0};
//    GPIO_InitStruct.Pin = GPIO_PIN_6;
//    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//    GPIO_InitStruct.Pull = GPIO_NOPULL;
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

//    // 4.2 Forzar LOW real
//    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

//    // 5. Reset de 50 ?s
//    HAL_Delay(100);

//    // 5.1 Volver a Alternate Function TIM4_CH1
//    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4;
//    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
//    
}


/* ============================================================
   FUNCI?N: Seleccionar el GPIO seg?n ring_id
   ============================================================ */
static void select_gpio_for_ring(uint8_t ring, GPIO_TypeDef **port, uint16_t *pin){
    /*
       Esta funci?n asigna din?micamente el pin GPIO que corresponde
       al anillo solicitado. Es ?til porque el hilo recibe un ring_id
       y necesita saber qu? pin debe activar.
    */

    switch (ring)
    {
        case 0: *port = GPIOB; *pin = GPIO_PIN_6; break;  // TIM4_CH1
        case 1: *port = GPIOD; *pin = GPIO_PIN_13; break;  // TIM4_CH2
        case 2: *port = GPIOD; *pin = GPIO_PIN_12; break;  // TIM8_CH1
        case 3: *port = GPIOE; *pin = GPIO_PIN_5; break;  // TIM8_CH2
        default: *port = NULL; *pin = 0; break;
    }
}


/* -------------------------------------------------
   Rellena el buffer ws_buffer con los pulsos PWM
   correspondientes al color GRB repetido en los 8 LEDs
   ------------------------------------------------- */
static void ws2812_fill_buffer(rgb_color_t color){
    // WS2812 usa orden GRB, no RGB.
    // Por eso reordenamos los bytes:
    // grb[0] = Green
    // grb[1] = Red
    // grb[2] = Blue
  
    uint8_t grb[3] = { color.g, color.r, color.b };
    //uint8_t grb[3] = { color.r, color.g, color.b };     //VERDE - MORADO - CIAN
  
    //uint8_t grb[3] = { color.b, color.r, color.g };  // ROJO, CIAN, AMARILLO
    //uint8_t grb[3] = { color.r, color.b, color.g };

    // ?ndice dentro del buffer global ws_buffer[]
    uint16_t idx = 0;

    // Repetimos el color para los 8 LEDs del anillo (recorremos)
    for (int led = 0; led < LEDS_PER_RING; led++)
    {
        // Recorremos los 3 bytes del color: G, R, B
        for (int byte = 0; byte < 3; byte++)
        {
            // Recorremos cada bit del byte, de MSB (bit 7) a LSB (bit 0)
            for (int bit = 7; bit >= 0; bit--)
            {
                // Si el bit est? a 1 -> pulso largo (T1H)
                if (grb[byte] & (1 << bit))
                {
                    ws_buffer[idx++] = WS2812_T1H;   //bit 1
                }
                // Si el bit est? a 0 -> pulso corto (T0H)
                else
                {
                    ws_buffer[idx++] = WS2812_T0H;  //bit 0 
                }
            }
        }
    }
}

void Init_pin_tim(void)
{
    Init_pins();
    Init_tim4();
}


/*********************************************/


osThreadId_t tid_test_rgb;

void Th_test_rgb(void *argument);

int Init_Th_test_rgb(void){
    tid_test_rgb = osThreadNew(Th_test_rgb, NULL, NULL);
    if (tid_test_rgb == NULL)
        return -1;

    return 0;
}


void Th_test_rgb(void *argument)
{
    rgb_msg_t msg;
    osDelay(1000); // Espera inicial

    // Colores de prueba para cada anillo
    rgb_color_t colores[4] = {
        {100,   0,   0},   // Rojo
        {  0, 100,   0},   // Verde
        {  0,   0, 100},   // Azul
        {100, 100,   0}    // Amarillo
    };

    for (int ring = 0; ring < 4; ring++)
    {
        // Encender anillo
        msg.cmd = RGB_CMD_SET_COLOR;
        msg.ring_id = ring;
        msg.color = colores[ring];
        osMessageQueuePut(rgb_get_queue(), &msg, 0, 0);

        osDelay(1500);

        // Apagar anillo
        msg.cmd = RGB_CMD_OFF;
        osMessageQueuePut(rgb_get_queue(), &msg, 0, 0);

        osDelay(800);
    }

    // Repetir indefinidamente
    while (1)
    {
        osDelay(1000);
    }
}
