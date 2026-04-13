/**
 ******************************************************************************
 * @file    comFibraB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del receptor/transmisor de Fibra para el NODO B.
 * Integra la misma protección contra interferencias electromagnéticas que 
 * diseñé para el Nodo A, utilizando una máquina de estados de 1 byte no bloqueante.
 ******************************************************************************
 */

#include "comFibraB.h"
#include "stm32f4xx_hal.h"
#include "Driver_USART.h"

extern ARM_DRIVER_USART Driver_USART3;

// Mi variable global para conectar la ISR con el RTOS
osMessageQueueId_t colaFibraRX_B = NULL;

// Buffers estáticos para mi máquina de estados
static uint8_t rx_byte;
static uint8_t rx_buffer[FRAME_SIZE];
static uint8_t rx_index = 0;

/**
 * @brief Callback de interrupción de recepción. 
 * Reconstruye la trama al vuelo y valida la integridad de los datos.
 */
void USART3_Callback(uint32_t event) {
    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) { 
        
        // Fase de Sincronización
        if (rx_index == 0) {
            // Filtro ruido: solo arranco si veo mi byte de Start Of Frame
            if (rx_byte == SOF_BYTE) {
                rx_buffer[rx_index++] = rx_byte;
            }
        } 
        // Fase de Recolección
        else {
            rx_buffer[rx_index++] = rx_byte;
            
            if (rx_index >= FRAME_SIZE) {
                rx_index = 0; // Reseteo para el siguiente mensaje
                
                // Calculo el Checksum mediante XOR para verificar la integridad
                uint8_t calc_chk = rx_buffer[1] ^ rx_buffer[2] ^ rx_buffer[3] ^ rx_buffer[4];
                
                if (calc_chk == rx_buffer[5]) {
                    TramaFibra_t nuevaTrama;
                    nuevaTrama.tipo_comando = rx_buffer[1];
                    nuevaTrama.payload[0] = rx_buffer[2];
                    nuevaTrama.payload[1] = rx_buffer[3];
                    nuevaTrama.payload[2] = rx_buffer[4];
                    
                    // Notifico al CerebroB inmediatamente, sin bloquear la ISR
                    if (colaFibraRX_B != NULL) {
                        osMessageQueuePut(colaFibraRX_B, &nuevaTrama, 0, 0U);
                    }
                }
            }
        }
        // Vuelvo a armar la recepción del siguiente byte
        Driver_USART3.Receive(&rx_byte, 1);
    }
}

/**
 * @brief Inicializa el enlace óptico, protecciones de hardware y el RTOS.
 */
void FibraB_Init(void) {
    colaFibraRX_B = osMessageQueueNew(16, sizeof(TramaFibra_t), NULL);
    
    Driver_USART3.Initialize(USART3_Callback);
    Driver_USART3.PowerControl(ARM_POWER_FULL);
    
    // Configuro el estándar UART a 115200 baudios
    Driver_USART3.Control(ARM_USART_MODE_ASYNCHRONOUS |
                          ARM_USART_DATA_BITS_8       |
                          ARM_USART_PARITY_NONE       |
                          ARM_USART_STOP_BITS_1       |
                          ARM_USART_FLOW_CONTROL_NONE, 115200);

    // MI ESCUDO ANTI-TORMENTAS
    // Activo la resistencia Pull-Up en el pin RX (PC11) para evitar transitorios
    // ruidosos si se desconecta el cable físico.
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; 
    GPIO_InitStruct.Pull = GPIO_PULLUP; 
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // Bajo la prioridad en el NVIC para no interferir con el kernel de FreeRTOS
    HAL_NVIC_SetPriority(USART3_IRQn, 15, 0);

    // Habilito y comienzo a escuchar
    Driver_USART3.Control(ARM_USART_CONTROL_TX, 1);
    Driver_USART3.Control(ARM_USART_CONTROL_RX, 1);
    Driver_USART3.Receive(&rx_byte, 1);
}

/**
 * @brief Empaqueta datos de los sensores y los transmite hacia el Nodo A.
 */
void FibraB_SendFrame(uint8_t tipo, uint8_t p1, uint8_t p2, uint8_t p3) {
    static uint8_t tx_buffer[FRAME_SIZE]; 
    
    tx_buffer[0] = SOF_BYTE;
    tx_buffer[1] = tipo;
    tx_buffer[2] = p1;
    tx_buffer[3] = p2;
    tx_buffer[4] = p3;
    
    // Aplico el mismo Checksum XOR para que el Nodo A valide la trama
    tx_buffer[5] = tx_buffer[1] ^ tx_buffer[2] ^ tx_buffer[3] ^ tx_buffer[4];
    
    Driver_USART3.Send(tx_buffer, FRAME_SIZE);
}

