/**
 ******************************************************************************
 * @file    comFibraA.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementaci?n eficiente del Protocolo Binario en UART (Nodo A).
 ******************************************************************************
 */

#include "comFibraA.h"
#include <stdio.h> // Para la funci?n de Debug (printf)
                
/* --- Prototipos Privados --- */
static void Hilo_FibraA_Tx(void* args);                   
static void Callback_FibraA_USART(uint32_t event);
static void USART_FibraA_Config(void);

/* --- Recursos RTOS --- */
static osThreadId_t       tid_FibraATx;     
static osMessageQueueId_t mid_MsgQueueTx;   
static osMessageQueueId_t mid_MsgQueueRx;   

/* --- M?quina de Estados RX (Interrupci?n) --- */
static uint8_t rx_byte;                     // ?ltimo byte le?do
static uint8_t rx_buffer[sizeof(TramaFibra_t)]; // Array de 6 bytes exactos
static uint8_t rx_index = 0;                // Posici?n (0 a 5)

/* --- Driver Hardware --- */
extern ARM_DRIVER_USART Driver_USART2; // <-- Reemplazar por la UART correcta
static ARM_DRIVER_USART * USARTdrv = &Driver_USART2;
 
/* ==============================================================================
 * INICIALIZACI?N
 * ============================================================================== */
int Init_ComFibraA(void) {
    // Colas adaptadas al tama?o exacto de la estructura binaria (6 bytes)
    mid_MsgQueueTx = osMessageQueueNew(MAX_MENSAJES_Q, sizeof(TramaFibra_t), NULL);
    mid_MsgQueueRx = osMessageQueueNew(MAX_MENSAJES_Q, sizeof(TramaFibra_t), NULL);
    
    if (!mid_MsgQueueTx || !mid_MsgQueueRx) return -1;
  
    USART_FibraA_Config();

    const osThreadAttr_t tx_attr = {
        .name = "FibraA_TX",
        .priority = osPriorityAboveNormal // Alta prioridad para comunicaciones
    };
    tid_FibraATx = osThreadNew(Hilo_FibraA_Tx, NULL, &tx_attr);
    if (!tid_FibraATx) return -1;

    // Arrancamos el motor de recepci?n del primer byte
    rx_index = 0;
    USARTdrv->Receive(&rx_byte, 1);

    return 0;
}

/* ==============================================================================
 * FUNCIONES P?BLICAS DE ENV?O/RECEPCI?N (Para el Orquestador)
 * ============================================================================== */
int ComFibraA_EnviarTrama(const TramaFibra_t* trama) {
    if (trama == NULL) return -1;
    // Env?o a la cola sin bloqueo (timeout = 0)
    osStatus_t status = osMessageQueuePut(mid_MsgQueueTx, trama, 0U, 0U);
    return (status == osOK) ? 0 : -1;
}

int ComFibraA_RecibirTrama(TramaFibra_t* out_trama, uint32_t timeout_ms) {
    if (out_trama == NULL) return -1;
    osStatus_t status = osMessageQueueGet(mid_MsgQueueRx, out_trama, NULL, timeout_ms);
    return (status == osOK) ? 0 : -1;
}

void ComFibraA_DebugPrintTrama(const TramaFibra_t* trama) {
    // Esta funci?n asume que tienes printf redirigido al ST-Link (ITM o UART)
    printf("Trama -> Tipo: %02X | ID: %02X | P1: %d | P2: %d\r\n", 
           trama->tipo, trama->id_accion, trama->param1, trama->param2);
}

/* ==============================================================================
 * HILO DE TRANSMISI?N (TX)
 * ============================================================================== */
static void Hilo_FibraA_Tx(void* args){
    TramaFibra_t txTrama;
  
    while(1){
        // Esperamos una trama del Hilo Principal
        if (osMessageQueueGet(mid_MsgQueueTx, &txTrama, NULL, osWaitForever) == osOK) {
            
            osThreadFlagsClear(ARM_USART_EVENT_SEND_COMPLETE);
            
            // Enviamos los 6 bytes crudos directamente desde la memoria
            USARTdrv->Send(&txTrama, sizeof(TramaFibra_t));
        
            // Dormimos hasta que la transmisi?n f?sica acabe
            osThreadFlagsWait(ARM_USART_EVENT_SEND_COMPLETE, osFlagsWaitAny, osWaitForever);
        }
    }
}
 
/* ==============================================================================
 * M?QUINA DE ESTADOS DE RECEPCI?N (INTERRUPCI?N)
 * ============================================================================== */
static void Callback_FibraA_USART(uint32_t event){
    
    // 1. TRANSMISI?N COMPLETADA
    if (event & ARM_USART_EVENT_SEND_COMPLETE) {
        osThreadFlagsSet(tid_FibraATx, ARM_USART_EVENT_SEND_COMPLETE);
    }

    // 2. RECEPCI?N DE UN BYTE
    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        
        // Fase 1: Buscando el byte de inicio
        if (rx_index == 0) {
            if (rx_byte == TRAMA_START_BYTE) {
                rx_buffer[rx_index++] = rx_byte;
            }
        } 
        // Fase 2: Llenando el cuerpo de la trama
        else {
            rx_buffer[rx_index++] = rx_byte;
            
            // Fase 3: ?Hemos llegado a los 6 bytes?
            if (rx_index == sizeof(TramaFibra_t)) {
                
                // Validaci?n final de integridad
                if (rx_buffer[5] == TRAMA_END_BYTE) {
                    
                    // Casteamos el buffer crudo directamente a la estructura (S?per r?pido)
                    TramaFibra_t* trama_recibida = (TramaFibra_t*)rx_buffer;
                    
                    // Metemos a la cola
                    osMessageQueuePut(mid_MsgQueueRx, trama_recibida, 0U, 0U);
                }
                
                // Reiniciamos ?ndice para la siguiente trama (sea v?lida o corrupta)
                rx_index = 0; 
            }
        }
        
        // Rearmar el receptor HW
        USARTdrv->Receive(&rx_byte, 1);
    }
}

/* ==============================================================================
 * CONFIGURACI?N HARDWARE
 * ============================================================================== */
static void USART_FibraA_Config(void){
    USARTdrv->Initialize(Callback_FibraA_USART);
    USARTdrv->PowerControl(ARM_POWER_FULL);
    // Configuraci?n binaria est?ndar
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS | 
                      ARM_USART_DATA_BITS_8       | 
                      ARM_USART_PARITY_NONE       | 
                      ARM_USART_STOP_BITS_1       | 
                      ARM_USART_FLOW_CONTROL_NONE, 
                      FIBRA_SPEED);
    USARTdrv->Control(ARM_USART_CONTROL_TX | ARM_USART_CONTROL_RX, 1);
}


