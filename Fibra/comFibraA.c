/**
 ******************************************************************************
 * @file    comFibraA.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación de la comunicación con el Nodo B vía Fibra Óptica.
 * * Utiliza CMSIS-Driver USART. La transmisión se hace mediante un hilo que lee
 * de una cola, y la recepción se construye byte a byte en una interrupción 
 * para filtrar el ruido, enviando la trama final a otra cola.
 ******************************************************************************
 */

#include "comFibraA.h"
                
/* --- Prototipos Privados --- */
static void Hilo_FibraA_Tx(void* args);                   
static void Callback_FibraA_USART(uint32_t event);
static void USART_FibraA_Config(void);

/* --- Recursos RTOS --- */
static osThreadId_t       tid_FibraATx;     /*!< ID del hilo transmisor */
static osMessageQueueId_t mid_MsgQueueTx;   /*!< Cola de salida (A -> B) */
static osMessageQueueId_t mid_MsgQueueRx;   /*!< Cola de entrada (B -> A) */

/* --- Variables Privadas de la Máquina de Estados RX (Interrupción) --- */
static uint8_t rx_byte;                     // Byte crudo leído por el hardware
static char    rx_buffer[TAM_MAX_TRAMA];    // Buffer temporal donde se ensambla la trama
static uint8_t rx_index = 0;                // Posición actual en el buffer
static bool    is_receiving = false;        // Bandera: true si estamos dentro de un <...>

/* --- Driver Hardware --- */
/* IMPORTANTE: Cambia Driver_USART2 por la que uses para los pines de la fibra en la Nucleo */
extern ARM_DRIVER_USART Driver_USART2; 
static ARM_DRIVER_USART * USARTdrv = &Driver_USART2;
 
/**
 * @brief  Inicialización del módulo de Fibra para el Nodo A.
 */
int Init_ComFibraA(void) {

    // 1. Crear las Colas de Mensajes (RX y TX)
    mid_MsgQueueTx = osMessageQueueNew(MAX_MENSAJES_Q, TAM_MAX_TRAMA, NULL);
    mid_MsgQueueRx = osMessageQueueNew(MAX_MENSAJES_Q, TAM_MAX_TRAMA, NULL);
    
    if (mid_MsgQueueTx == NULL || mid_MsgQueueRx == NULL) {
        return(-1); // Error fatal de memoria RTOS
    }
  
    // 2. Inicializar el Hardware (UART)
    USART_FibraA_Config();

    // 3. Crear el Hilo Transmisor (Prioridad un poco más alta por ser IO)
    const osThreadAttr_t tx_attr = {
        .name = "FibraA_TX_Thread",
        .priority = osPriorityAboveNormal
    };
    tid_FibraATx = osThreadNew(Hilo_FibraA_Tx, NULL, &tx_attr);
    
    if (tid_FibraATx == NULL) {
        return(-1);
    }

    // 4. ¡VITAL PARA RX! Armar el receptor para el primer byte en background
    USARTdrv->Receive(&rx_byte, 1);

    return(0);
}

/* ==============================================================================
 * WRAPPERS PÚBLICOS PARA EL ORQUESTADOR
 * ============================================================================== */

int ComFibraA_EnviarMensaje(const char* msg) {
    if (msg == NULL) return -1;
    char temp_msg[TAM_MAX_TRAMA];
    
    // Evitamos desbordamientos al copiar
    strncpy(temp_msg, msg, TAM_MAX_TRAMA - 1);
    temp_msg[TAM_MAX_TRAMA - 1] = '\0';
    
    // Timeout 0: Si la cola está llena, descartamos, pero NO bloqueamos al jefe
    osStatus_t status = osMessageQueuePut(mid_MsgQueueTx, temp_msg, 0U, 0U);
    return (status == osOK) ? 0 : -1;
}

int ComFibraA_RecibirMensaje(char* out_msg, uint32_t timeout_ms) {
    if (out_msg == NULL) return -1;
    
    osStatus_t status = osMessageQueueGet(mid_MsgQueueRx, out_msg, NULL, timeout_ms);
    return (status == osOK) ? 0 : -1;
}

/* ==============================================================================
 * HILO DE TRANSMISIÓN (TX)
 * ============================================================================== */

/**
 * @brief Hilo que extrae de la cola TX y envía físicamente al hardware.
 */
static void Hilo_FibraA_Tx(void* args){
    char txMessage[TAM_MAX_TRAMA];
    osStatus_t status;
  
    while(1){
        // 1. Esperamos indefinidamente hasta que el Jefe mande algo a la cola
        status = osMessageQueueGet(mid_MsgQueueTx, txMessage, NULL, osWaitForever);
     
        if (status == osOK) {
            int len = strlen(txMessage);
            if (len > 0) {
                // Limpiamos banderas viejas por seguridad
                osThreadFlagsClear(ARM_USART_EVENT_SEND_COMPLETE);
                
                // 2. Disparamos el envío por Hardware (Retorna al instante)
                USARTdrv->Send(txMessage, len);
            
                // 3. Nos suspendemos (0% CPU) hasta que la interrupción avise que terminó
                osThreadFlagsWait(ARM_USART_EVENT_SEND_COMPLETE, osFlagsWaitAny, osWaitForever);
            }
        }
    }
}
 
/* ==============================================================================
 * RUTINA DE INTERRUPCIÓN (CALLBACK DE HARDWARE)
 * ============================================================================== */

/**
 * @brief  Callback del Driver USART. Gestiona tanto que ha terminado de enviar (TX)
 * como la llegada de un nuevo carácter (RX).
 */
static void Callback_FibraA_USART(uint32_t event){
    
    // ---------------------------------------------------------
    // EVENTO 1: TRANSMISIÓN COMPLETADA
    // ---------------------------------------------------------
    if (event & ARM_USART_EVENT_SEND_COMPLETE) {
        /* Despertamos al hilo TX para que pueda mandar el siguiente mensaje */
        osThreadFlagsSet(tid_FibraATx, ARM_USART_EVENT_SEND_COMPLETE);
    }

    // ---------------------------------------------------------
    // EVENTO 2: UN BYTE NUEVO HA LLEGADO
    // ---------------------------------------------------------
    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE) {
        
        // Detección de Inicio de Trama
        if (rx_byte == '<') {
            is_receiving = true;
            rx_index = 0;
            rx_buffer[rx_index++] = rx_byte; // Guardamos el '<'
        } 
        // Si estamos construyendo la trama...
        else if (is_receiving) {
            
            // Verificamos no pasarnos del límite del array
            if (rx_index < (TAM_MAX_TRAMA - 1)) {
                rx_buffer[rx_index++] = rx_byte;
                
                // Detección de Fin de Trama
                if (rx_byte == '>') {
                    rx_buffer[rx_index] = '\0'; // Cerramos la cadena
                    is_receiving = false;
                    
                    // ¡Trama completa libre de ruido! A la cola para el Orquestador.
                    // Nota: Timeout DEBE ser 0 porque estamos en una Interrupción.
                    osMessageQueuePut(mid_MsgQueueRx, rx_buffer, 0U, 0U);
                }
            } else {
                // Buffer Overflow prevenido: Vino ruido sin el cierre '>'. Abortamos.
                is_receiving = false;
                rx_index = 0;
            }
        }
        
        // ¡VITAL!: Rearmamos el hardware para que capture el próximo byte
        USARTdrv->Receive(&rx_byte, 1);
    }
}

/* ==============================================================================
 * CONFIGURACIÓN DE BAJO NIVEL
 * ============================================================================== */

/**
 * @brief Configura el USART con la API del CMSIS-Driver.
 */
static void USART_FibraA_Config(void){
    USARTdrv->Initialize(Callback_FibraA_USART);
    USARTdrv->PowerControl(ARM_POWER_FULL);
  
    // Configuración 8N1 sin control de flujo
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS | 
                      ARM_USART_DATA_BITS_8       | 
                      ARM_USART_PARITY_NONE       | 
                      ARM_USART_STOP_BITS_1       | 
                      ARM_USART_FLOW_CONTROL_NONE, 
                      FIBRA_SPEED);
  
    // Habilitar tanto el Transmisor (TX) como el Receptor (RX)
    USARTdrv->Control(ARM_USART_CONTROL_TX | ARM_USART_CONTROL_RX, 1);
}
