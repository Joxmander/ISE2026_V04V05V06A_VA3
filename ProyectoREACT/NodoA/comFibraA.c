/**
 ******************************************************************************
 * @file    comFibraA.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del Protocolo R.E.A.C.T. en UART con Checksum.
 * Aquí gestiono el bajo nivel del puerto serie. Mi principal objetivo con 
 * este diseño es aislar completamente el ruido de la fibra óptica del RTOS,
 * garantizando que el servidor web no colapse por exceso de interrupciones.
 ******************************************************************************
 */

#include "comFibraA.h"
#include "stm32f4xx_hal.h"
#include "Driver_USART.h"

// Importo el driver CMSIS estándar de la USART3
extern ARM_DRIVER_USART Driver_USART3;

// Mi variable global para la cola de mensajes
osMessageQueueId_t colaFibraRX = NULL;

// Variables estáticas para mi máquina de estados de recepción.
// Las declaro globales a este archivo para que no se pierdan entre interrupciones.
static uint8_t rx_byte;              // Mi buffer para leer de byte en byte.
static uint8_t rx_buffer[FRAME_SIZE]; // Almacenamiento temporal de la trama entrante.
static uint8_t rx_index = 0;         // Puntero para saber por qué byte voy.

/**
 * @brief Callback invocado por el Driver CMSIS de USART desde la interrupción.
 * He diseñado esta función para que sea extremadamente corta y no bloqueante.
 * Uso una lectura byte a byte para recuperarme al instante de cualquier 
 * desincronización por ruido eléctrico.
 */
void USART3_Callback(uint32_t event) {
    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        
        // Mi máquina de estados para ensamblar la trama sobre la marcha
        if (rx_index == 0) {
            // Solo empiezo a guardar si el primer byte coincide con mi SOF
            if (rx_byte == SOF_BYTE) {
                rx_buffer[rx_index++] = rx_byte;
            }
            // Si llega basura (ruido), simplemente la ignoro y no avanzo el índice.
        } 
        else {
            rx_buffer[rx_index++] = rx_byte;
            
            // Cuando ya tengo los 6 bytes, evalúo la trama
            if (rx_index >= FRAME_SIZE) {
                rx_index = 0; // Armo la máquina para la siguiente trama
                
                // Calculo el Checksum haciendo un XOR lógico de mis datos
                uint8_t calc_chk = rx_buffer[1] ^ rx_buffer[2] ^ rx_buffer[3] ^ rx_buffer[4];
                
                // Si el paquete está íntegro, extraigo la información útil
                if (calc_chk == rx_buffer[5]) {
                    TramaFibra_t nuevaTrama;
                    nuevaTrama.tipo_comando = rx_buffer[1];
                    nuevaTrama.payload[0] = rx_buffer[2];
                    nuevaTrama.payload[1] = rx_buffer[3];
                    nuevaTrama.payload[2] = rx_buffer[4];
                    
                    // Delega el procesamiento al sistema operativo mediante IPC (Cola).
                    // Uso un timeout de 0 porque dentro de una interrupción no puedo bloquearme.
                    if (colaFibraRX != NULL) {
                        osMessageQueuePut(colaFibraRX, &nuevaTrama, 0, 0U);
                    }
                }
            }
        }
        
        // Le pido al driver que vuelva a escuchar asíncronamente el siguiente byte
        Driver_USART3.Receive(&rx_byte, 1);
    }
}

/**
 * @brief Inicializa el periférico USART3, sus pines, el RTOS y protecciones eléctricas.
 */
void FibraA_Init(void) {
    // 1. Creo la cola del RTOS para conectar la interrupción con mi hilo principal
    colaFibraRX = osMessageQueueNew(16, sizeof(TramaFibra_t), NULL);
    
    // 2. Inicializo el Driver CMSIS
    Driver_USART3.Initialize(USART3_Callback);
    Driver_USART3.PowerControl(ARM_POWER_FULL);
    
    // 3. Configuro a 115200 bps, 8 bits de datos, sin paridad y 1 bit de parada.
    Driver_USART3.Control(ARM_USART_MODE_ASYNCHRONOUS |
                          ARM_USART_DATA_BITS_8       |
                          ARM_USART_PARITY_NONE       |
                          ARM_USART_STOP_BITS_1       |
                          ARM_USART_FLOW_CONTROL_NONE, 115200);

    // 4. MI ESCUDO ANTI-TORMENTAS (Configuraciones de protección por Hardware)
    // Fuerzo una resistencia Pull-Up en el pin de recepción (RX = PC11).
    // Si la fibra óptica se desconecta, esto evita que el pin flote e introduzca interrupciones fantasma.
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; 
    GPIO_InitStruct.Pull = GPIO_PULLUP; 
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    // 5. Protección del Servidor Web (Modificación en el NVIC)
    // Le bajo la prioridad a la interrupción de la USART3 al nivel mínimo (15).
    // Así garantizo que el chip de red Ethernet LAN8742A siempre tenga prioridad de CPU.
    HAL_NVIC_SetPriority(USART3_IRQn, 15, 0);

    // 6. Arranco la máquina habilitando la transmisión y la primera recepción.
    Driver_USART3.Control(ARM_USART_CONTROL_TX, 1);
    Driver_USART3.Control(ARM_USART_CONTROL_RX, 1);
    Driver_USART3.Receive(&rx_byte, 1);
}

/**
 * @brief Empaqueta y envía una trama de control hacia el Nodo B.
 */
void FibraA_SendFrame(uint8_t tipo, uint8_t p1, uint8_t p2, uint8_t p3) {
    // Declaro este buffer estático para que su dirección de memoria persista
    // mientras la DMA o el driver envían los datos en segundo plano.
    static uint8_t tx_buffer[FRAME_SIZE]; 
    
    tx_buffer[0] = SOF_BYTE;
    tx_buffer[1] = tipo;
    tx_buffer[2] = p1;
    tx_buffer[3] = p2;
    tx_buffer[4] = p3;
    
    // Genero mi Checksum usando la operación XOR, que es computacionalmente barata
    tx_buffer[5] = tx_buffer[1] ^ tx_buffer[2] ^ tx_buffer[3] ^ tx_buffer[4];
    
    // Ordeno al driver que envíe los bytes sin bloquear mi código
    Driver_USART3.Send(tx_buffer, FRAME_SIZE);
}
