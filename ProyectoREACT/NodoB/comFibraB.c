#include "comFibraB.h"
#include "stm32f4xx_hal.h"
#include "Driver_USART.h"

extern ARM_DRIVER_USART Driver_USART3;

osMessageQueueId_t colaFibraRX_B = NULL;

static uint8_t rx_byte;
static uint8_t rx_buffer[FRAME_SIZE];
static uint8_t rx_index = 0;

void USART3_Callback_B(uint32_t event) {
    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        if (rx_index == 0) {
            if (rx_byte == SOF_BYTE) {
                rx_buffer[rx_index++] = rx_byte;
            }
        } 
        else {
            rx_buffer[rx_index++] = rx_byte;
            if (rx_index >= FRAME_SIZE) {
                rx_index = 0; 
                uint8_t calc_chk = rx_buffer[1] ^ rx_buffer[2] ^ rx_buffer[3] ^ rx_buffer[4];
                
                if (calc_chk == rx_buffer[5]) {
                    TramaFibra_t nuevaTrama;
                    nuevaTrama.tipo_comando = rx_buffer[1];
                    nuevaTrama.payload[0] = rx_buffer[2];
                    nuevaTrama.payload[1] = rx_buffer[3];
                    nuevaTrama.payload[2] = rx_buffer[4];
                    
                    if (colaFibraRX_B != NULL) {
                        osMessageQueuePut(colaFibraRX_B, &nuevaTrama, 0, 0U);
                    }
                }
            }
        }
        Driver_USART3.Receive(&rx_byte, 1);
    }
}

void FibraB_Init(void) {
    colaFibraRX_B = osMessageQueueNew(16, sizeof(TramaFibra_t), NULL);
    
    Driver_USART3.Initialize(USART3_Callback_B);
    Driver_USART3.PowerControl(ARM_POWER_FULL);
    Driver_USART3.Control(ARM_USART_MODE_ASYNCHRONOUS |
                          ARM_USART_DATA_BITS_8       |
                          ARM_USART_PARITY_NONE       |
                          ARM_USART_STOP_BITS_1       |
                          ARM_USART_FLOW_CONTROL_NONE, 9600);

    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; 
    GPIO_InitStruct.Pull = GPIO_PULLUP; 
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);

    Driver_USART3.Control(ARM_USART_CONTROL_TX, 1);
    Driver_USART3.Control(ARM_USART_CONTROL_RX, 1);
    Driver_USART3.Receive(&rx_byte, 1);
}

void FibraB_SendFrame(uint8_t tipo, uint8_t p1, uint8_t p2, uint8_t p3) {
    static uint8_t tx_buffer[FRAME_SIZE]; 
    tx_buffer[0] = SOF_BYTE;
    tx_buffer[1] = tipo;
    tx_buffer[2] = p1;
    tx_buffer[3] = p2;
    tx_buffer[4] = p3;
    tx_buffer[5] = tx_buffer[1] ^ tx_buffer[2] ^ tx_buffer[3] ^ tx_buffer[4];
    
    Driver_USART3.Send(tx_buffer, FRAME_SIZE);
}
