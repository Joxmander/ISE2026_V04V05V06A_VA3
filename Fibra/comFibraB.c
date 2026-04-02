/**
 ******************************************************************************
 * @file    comFibraB.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del receptor/transmisor de Fibra para el NODO B.
 ******************************************************************************
 */

#include "comFibraB.h"
                
/* --- Prototipos Privados --- */
static void Hilo_FibraB_Tx(void* args);                   
static void Callback_FibraB_USART(uint32_t event);
static void USART_FibraB_Config(void);

/* --- Recursos RTOS --- */
static osThreadId_t       tid_FibraBTx;     
static osMessageQueueId_t mid_MsgQueueTx;   
static osMessageQueueId_t mid_MsgQueueRx;   

/* --- Máquina de Estados RX --- */
static uint8_t rx_byte;                     
static uint8_t rx_buffer[sizeof(TramaFibra_t)]; 
static uint8_t rx_index = 0;                

/* --- Driver Hardware (AJUSTAR AL NODO B) --- */
extern ARM_DRIVER_USART Driver_USART3; // <--- Ojo aquí, poner la del Nodo B
static ARM_DRIVER_USART * USARTdrv = &Driver_USART3;
 
/* ==============================================================================
 * INICIALIZACIÓN
 * ============================================================================== */
int Init_ComFibraB(void) {
    mid_MsgQueueTx = osMessageQueueNew(10, sizeof(TramaFibra_t), NULL);
    mid_MsgQueueRx = osMessageQueueNew(10, sizeof(TramaFibra_t), NULL);
    
    if (!mid_MsgQueueTx || !mid_MsgQueueRx) return -1;
  
    USART_FibraB_Config();

    const osThreadAttr_t tx_attr = {
        .name = "FibraB_TX",
        .priority = osPriorityAboveNormal 
    };
    tid_FibraBTx = osThreadNew(Hilo_FibraB_Tx, NULL, &tx_attr);
    if (!tid_FibraBTx) return -1;

    rx_index = 0;
    USARTdrv->Receive(&rx_byte, 1);

    return 0;
}

/* ==============================================================================
 * FUNCIONES PÚBLICAS
 * ============================================================================== */
int ComFibraB_EnviarTrama(const TramaFibra_t* trama) {
    if (trama == NULL) return -1;
    osStatus_t status = osMessageQueuePut(mid_MsgQueueTx, trama, 0U, 0U);
    return (status == osOK) ? 0 : -1;
}

int ComFibraB_RecibirTrama(TramaFibra_t* out_trama, uint32_t timeout_ms) {
    if (out_trama == NULL) return -1;
    osStatus_t status = osMessageQueueGet(mid_MsgQueueRx, out_trama, NULL, timeout_ms);
    return (status == osOK) ? 0 : -1;
}

/* ==============================================================================
 * HILO DE TRANSMISIÓN (TX)
 * ============================================================================== */
static void Hilo_FibraB_Tx(void* args){
    TramaFibra_t txTrama;
  
    while(1){
        if (osMessageQueueGet(mid_MsgQueueTx, &txTrama, NULL, osWaitForever) == osOK) {
            osThreadFlagsClear(ARM_USART_EVENT_SEND_COMPLETE);
            USARTdrv->Send(&txTrama, sizeof(TramaFibra_t));
            osThreadFlagsWait(ARM_USART_EVENT_SEND_COMPLETE, osFlagsWaitAny, osWaitForever);
        }
    }
}
 
/* ==============================================================================
 * MÁQUINA DE ESTADOS DE RECEPCIÓN (INTERRUPCIÓN)
 * ============================================================================== */
static void Callback_FibraB_USART(uint32_t event){
    
    if (event & ARM_USART_EVENT_SEND_COMPLETE) {
        osThreadFlagsSet(tid_FibraBTx, ARM_USART_EVENT_SEND_COMPLETE);
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        if (rx_index == 0) {
            if (rx_byte == TRAMA_START_BYTE) {
                rx_buffer[rx_index++] = rx_byte;
            }
        } else {
            rx_buffer[rx_index++] = rx_byte;
            if (rx_index == sizeof(TramaFibra_t)) {
                if (rx_buffer[5] == TRAMA_END_BYTE) {
                    TramaFibra_t* trama_recibida = (TramaFibra_t*)rx_buffer;
                    osMessageQueuePut(mid_MsgQueueRx, trama_recibida, 0U, 0U);
                }
                rx_index = 0; 
            }
        }
        USARTdrv->Receive(&rx_byte, 1);
    }
}

/* ==============================================================================
 * CONFIGURACIÓN HARDWARE
 * ============================================================================== */
static void USART_FibraB_Config(void){
    USARTdrv->Initialize(Callback_FibraB_USART);
    USARTdrv->PowerControl(ARM_POWER_FULL);
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS | 
                      ARM_USART_DATA_BITS_8       | 
                      ARM_USART_PARITY_NONE       | 
                      ARM_USART_STOP_BITS_1       | 
                      ARM_USART_FLOW_CONTROL_NONE, 
                      115200);
    USARTdrv->Control(ARM_USART_CONTROL_TX | ARM_USART_CONTROL_RX, 1);
}
