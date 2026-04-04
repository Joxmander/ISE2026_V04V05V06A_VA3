/**
 ******************************************************************************
 * @file    comFibraA.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del Protocolo R.E.A.C.T. en UART con Checksum
 ******************************************************************************
 */

#include "comFibraA.h"
#include <stdio.h> 
                
/* --- Prototipos Privados --- */
static void Hilo_FibraA_Tx(void* args);                   
static void Callback_FibraA_USART(uint32_t event);
static void USART_FibraA_Config(void);

/* --- Recursos RTOS --- */
static osThreadId_t       tid_FibraATx;     
static osMessageQueueId_t mid_MsgQueueTx;   
static osMessageQueueId_t mid_MsgQueueRx;   

/* --- Máquina de Estados RX --- */
static uint8_t rx_byte;                     
static uint8_t rx_buffer[sizeof(TramaFibra_t)]; 
static uint8_t rx_index = 0;                

extern ARM_DRIVER_USART Driver_USART3; 
static ARM_DRIVER_USART * USARTdrv = &Driver_USART3;
 
int Init_ComFibraA(void) {
    mid_MsgQueueTx = osMessageQueueNew(MAX_MENSAJES_Q, sizeof(TramaFibra_t), NULL);
    mid_MsgQueueRx = osMessageQueueNew(MAX_MENSAJES_Q, sizeof(TramaFibra_t), NULL);
    
    if (!mid_MsgQueueTx || !mid_MsgQueueRx) return -1;
  
    USART_FibraA_Config(); 

    const osThreadAttr_t tx_attr = {
        .name = "FibraA_TX",
        .priority = osPriorityAboveNormal,
        .stack_size = 512 
    };
    tid_FibraATx = osThreadNew(Hilo_FibraA_Tx, NULL, &tx_attr);
    if (!tid_FibraATx) return -1;

    rx_index = 0;
    // RECEPCIÓN APAGADA HASTA TENER LA SEGUNDA PLACA (Evita cuelgues de red)
    // USARTdrv->Receive(&rx_byte, 1); 

    return 0;
}

int ComFibraA_EnviarTrama(TramaFibra_t* trama) {
    if (trama == NULL) return -1;
    trama->sof = TRAMA_SOF;
    trama->checksum = trama->tipo_modo ^ trama->payload1 ^ trama->payload2 ^ trama->payload3;
    
    osStatus_t status = osMessageQueuePut(mid_MsgQueueTx, trama, 0U, 0U);
    return (status == osOK) ? 0 : -1;
}

int ComFibraA_RecibirTrama(TramaFibra_t* out_trama, uint32_t timeout_ms) {
    if (out_trama == NULL) return -1;
    osStatus_t status = osMessageQueueGet(mid_MsgQueueRx, out_trama, NULL, timeout_ms);
    return (status == osOK) ? 0 : -1;
}

static void Hilo_FibraA_Tx(void* args){
    TramaFibra_t txTrama;
    while(1){
        if (osMessageQueueGet(mid_MsgQueueTx, &txTrama, NULL, osWaitForever) == osOK) {
            osThreadFlagsClear(ARM_USART_EVENT_SEND_COMPLETE);
            USARTdrv->Send(&txTrama, sizeof(TramaFibra_t));
            osThreadFlagsWait(ARM_USART_EVENT_SEND_COMPLETE, osFlagsWaitAny, osWaitForever);
        }
    }
}
 
static void Callback_FibraA_USART(uint32_t event){
    if (event & ARM_USART_EVENT_SEND_COMPLETE) {
        osThreadFlagsSet(tid_FibraATx, ARM_USART_EVENT_SEND_COMPLETE);
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        if (rx_index == 0) {
            if (rx_byte == TRAMA_SOF) {
                rx_buffer[rx_index++] = rx_byte;
            }
        } 
        else {
            rx_buffer[rx_index++] = rx_byte;
            if (rx_index == sizeof(TramaFibra_t)) {
                TramaFibra_t* trama_recibida = (TramaFibra_t*)rx_buffer;
                uint8_t chk_calc = trama_recibida->tipo_modo ^ trama_recibida->payload1 ^ trama_recibida->payload2 ^ trama_recibida->payload3;
                
                if (trama_recibida->checksum == chk_calc) {
                    osMessageQueuePut(mid_MsgQueueRx, trama_recibida, 0U, 0U);
                }
                rx_index = 0; 
            }
        }
        USARTdrv->Receive(&rx_byte, 1); 
    }
}

static void USART_FibraA_Config(void){
    USARTdrv->Initialize(Callback_FibraA_USART);
    USARTdrv->PowerControl(ARM_POWER_FULL);
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS | 
                      ARM_USART_DATA_BITS_8       | 
                      ARM_USART_PARITY_NONE       | 
                      ARM_USART_STOP_BITS_1       | 
                      ARM_USART_FLOW_CONTROL_NONE, 
                      FIBRA_SPEED);
                      
    // Encendemos SOLO TX para mantener el Ping vivo. 
    USARTdrv->Control(ARM_USART_CONTROL_TX, 1); 
}




