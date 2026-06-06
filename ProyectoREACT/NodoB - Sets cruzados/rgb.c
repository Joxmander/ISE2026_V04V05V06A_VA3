#include "rgb.h"

osThreadId_t tid_rgb;
osMessageQueueId_t id_cola_rgb;

void Init_pins_mock(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOD_CLK_ENABLE(); __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = GPIO_PIN_6;  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct); // Anillo 0
    GPIO_InitStruct.Pin = GPIO_PIN_13; HAL_GPIO_Init(GPIOD, &GPIO_InitStruct); // Anillo 1
    GPIO_InitStruct.Pin = GPIO_PIN_12; HAL_GPIO_Init(GPIOD, &GPIO_InitStruct); // Anillo 2
    GPIO_InitStruct.Pin = GPIO_PIN_5;  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct); // Anillo 3
}

static void select_gpio_for_ring(uint8_t ring, GPIO_TypeDef **port, uint16_t *pin) {
    switch (ring) {
        case 0: *port = GPIOB; *pin = GPIO_PIN_6; break;
        case 1: *port = GPIOD; *pin = GPIO_PIN_13; break;
        case 2: *port = GPIOD; *pin = GPIO_PIN_12; break;
        case 3: *port = GPIOE; *pin = GPIO_PIN_5; break;
        default: *port = NULL; *pin = 0; break;
    }
}

void Th_rgb(void *argument) {
    rgb_msg_t msg; GPIO_TypeDef *port; uint16_t pin;
    while (1) {
        if (osMessageQueueGet(id_cola_rgb, &msg, NULL, osWaitForever) == osOK) {
            select_gpio_for_ring(msg.ring_id, &port, &pin);
            if (port != NULL) {
                if (msg.cmd == RGB_CMD_SET_COLOR) HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
                else if (msg.cmd == RGB_CMD_OFF) HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
            }
        }
    }
}

int Init_Th_rgb(void) {
    Init_pins_mock();
    id_cola_rgb = osMessageQueueNew(16, sizeof(rgb_msg_t), NULL);
    tid_rgb = osThreadNew(Th_rgb, NULL, NULL);
    return (tid_rgb == NULL) ? -1 : 0;
}

osMessageQueueId_t rgb_get_queue(void) { return id_cola_rgb; }
void rgb_encender_pad(uint8_t pad, rgb_color_t color) {
    rgb_msg_t msg; msg.cmd = RGB_CMD_SET_COLOR; msg.ring_id = pad; msg.color = color;
    osMessageQueuePut(id_cola_rgb, &msg, 0, 0);
}
void rgb_apagar_pad(uint8_t pad) {
    rgb_msg_t msg; msg.cmd = RGB_CMD_OFF; msg.ring_id = pad;
    osMessageQueuePut(id_cola_rgb, &msg, 0, 0);
}
void rgb_encender_todos(rgb_color_t color) {
    for (uint8_t i = 0; i < RGB_NUM_RINGS; i++) rgb_encender_pad(i, color);
}
void rgb_apagar_todos(void) {
    for (uint8_t i = 0; i < RGB_NUM_RINGS; i++) rgb_apagar_pad(i);
}
