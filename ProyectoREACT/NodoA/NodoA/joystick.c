/**
  ******************************************************************************
  * @file    joystick.c
  * @author  Estudiante
  * @brief   Implementación de la gestión del Joystick mediante Interrupciones y RTOS.
  * * Estrategia de funcionamiento:
  * 1. La pulsación física dispara una Interrupción Hardware (EXTI).
  * 2. La ISR despierta al Hilo mediante un Flag.
  * 3. El Hilo inicia un Timer periódico (50ms) para filtrar rebotes (Debounce).
  * 4. Si la señal es estable, se cuenta el tiempo para distinguir Corta/Larga.
  ******************************************************************************
  */

#include "joystick.h"
#include "stm32f4xx_hal.h"
#include "main.h" 
#include <string.h>

// --- Variables Globales ---
osMessageQueueId_t id_MsgQueue_Joystick;  
osThreadId_t tid_ThJoy;                         
osTimerId_t timJoy_id;                          

/* Variables estáticas para la lógica de estado */
static uint16_t tiempo_pulsado_ms = 0; /*!< Contador de tiempo que la tecla está presionada */
static uint32_t exec;                  /*!< Argumento dummy para el timer */
static MSGQUEUE_JOY_t joy;             /*!< Objeto temporal para enviar el mensaje */

// --- Prototipos Locales ---
void ThJoystick (void *argument);               
void init_GPIOS(void);
static void IdentificarTecla(void);
static void IdentificarPulsacion(void);
static void Timer_Callback(void *arg);

/**
  * @brief  Inicialización del módulo Joystick.
  * Configura hardware y crea los objetos del RTOS (Timer, Cola, Hilo).
  */
int Init_ThJoystick (void) {
    // 1. Configuración de pines e interrupciones EXTI
    init_GPIOS();

    exec = 1U; 
    
    // 2. Timer periódico (50ms)
    // Fundamental para el algoritmo de debounce y para cronometrar la pulsación larga.
    timJoy_id = osTimerNew((osTimerFunc_t)&Timer_Callback, osTimerPeriodic, &exec, NULL);      

    // 3. Cola de mensajes (Profundidad 16 eventos)
    id_MsgQueue_Joystick = osMessageQueueNew(16, sizeof(MSGQUEUE_JOY_t), NULL);   
 
    // 4. Creación del Hilo
    tid_ThJoy = osThreadNew(ThJoystick, NULL, NULL);
    if (tid_ThJoy == NULL) {
        return(-1);
    }
 
    return(0);
}

/**
  * @brief  Hilo Principal del Joystick (Máquina de Estados).
  * Espera eventos de Hardware (ISR) o de Software (Timer).
  */
void ThJoystick (void *argument) {
    uint32_t flags;
    
    while (1) {
        // Bloqueo del hilo esperando:
        // A) FLAG_CIC: Alguien pulsó una tecla (Flanco de subida).
        // B) FLAG_DEBO: El timer de 50ms ha vencido (muestreo).
        flags = osThreadFlagsWait(FLAG_CIC | FLAG_DEBO, osFlagsWaitAny, osWaitForever);
        
        // --- Caso 1: Detección inicial (Interrupción) ---
        if (flags & FLAG_CIC) {
            // Solo iniciamos el proceso si el timer NO está corriendo.
            // Esto evita reiniciar la cuenta si hay rebotes mecánicos rápidos al inicio.
            if (!osTimerIsRunning(timJoy_id)) {
                tiempo_pulsado_ms = 0;           
                osTimerStart(timJoy_id, 50U); // Arrancamos muestreo cada 50ms
            }
        }
        
        // --- Caso 2: Muestreo periódico (Timer) ---
        if (flags & FLAG_DEBO) {
            // En el primer tick (50ms), identificamos qué tecla es.
            // Esperar 50ms asegura que leemos la señal estable, evitando ruido.
            if (tiempo_pulsado_ms == 0) {
                IdentificarTecla();
            }
            
            // Gestionamos la lógica de mantener presionado vs soltar
            IdentificarPulsacion();
        }
    }
}

/**
  * @brief  Callback del Timer (ISR Software).
  * Simplemente notifica al hilo principal para que procese la lógica.
  */
static void Timer_Callback(void *arg){
    osThreadFlagsSet(tid_ThJoy, FLAG_DEBO);  
}

/**
  * @brief  Lectura física de los pines para determinar la tecla.
  * Mapeo según esquemático de la MBED Application Board.
  */
static void IdentificarTecla(void) {
    joy.gesto = 0; 

    // Nota: Lógica positiva (Active High) asumida por la configuración PULLDOWN/RISING
    if(HAL_GPIO_ReadPin (GPIOE, GPIO_PIN_15))      joy.gesto = CENTRO;
    else if(HAL_GPIO_ReadPin (GPIOB, GPIO_PIN_10)) joy.gesto = ARRIBA;
    else if(HAL_GPIO_ReadPin (GPIOB, GPIO_PIN_11)) joy.gesto = DERECHA;
    else if(HAL_GPIO_ReadPin (GPIOE, GPIO_PIN_12)) joy.gesto = ABAJO;
    else if(HAL_GPIO_ReadPin (GPIOE, GPIO_PIN_14)) joy.gesto = IZQUIERDA;
}

/**
  * @brief  Máquina de estados para determinar Corta vs Larga.
  * Se ejecuta cada 50ms mientras el timer está activo.
  */
static void IdentificarPulsacion(void){
    tiempo_pulsado_ms += 50; 
    uint8_t sigue_pulsado = 0;

    // Verificamos si el pin específico detectado sigue en alto
    switch (joy.gesto) {
        case CENTRO:    if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_15)) sigue_pulsado = 1; break;
        case ARRIBA:    if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_10)) sigue_pulsado = 1; break;
        case DERECHA:   if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11)) sigue_pulsado = 1; break;
        case ABAJO:     if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_12)) sigue_pulsado = 1; break;
        case IZQUIERDA: if(HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_14)) sigue_pulsado = 1; break;
        default: 
            osTimerStop(timJoy_id); // Gesto inválido o error de lectura
            return;
    }

    if (sigue_pulsado) {
        // --- Usuario mantiene el botón ---
        
        // Detectar pulsación larga (> 1000ms)
        // Enviamos el evento JUSTO cuando se cruza el umbral, una sola vez.
        if (tiempo_pulsado_ms == 1000) { 
            joy.pulso = PUL_LARGA;
            osMessageQueuePut(id_MsgQueue_Joystick, &joy, 0U, 0U);
        }
    } 
    else {
        // --- Usuario soltó el botón ---
        
        // Si soltó ANTES de 1000ms y después de 0ms, es Pulsación CORTA.
        // El filtro (>0) evita falsos positivos por ruido muy breve.
        if (tiempo_pulsado_ms < 1000 && tiempo_pulsado_ms > 0) {
            joy.pulso = PUL_CORTA;
            osMessageQueuePut(id_MsgQueue_Joystick, &joy, 0U, 0U);
        }
        
        // Reseteo del sistema para la próxima pulsación
        osTimerStop(timJoy_id); 
        tiempo_pulsado_ms = 0;  
    }
}

/* --- CONFIGURACIÓN E INTERRUPCIONES (CRÍTICO) --- */

/**
  * @brief  Configura los pines en modo Interrupción por Flanco de Subida.
  */
void init_GPIOS(void){
    GPIO_InitTypeDef GPIO_InitStruct = {0};
  
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
  
    // Configuración: Interrupción Flanco Subida + PullDown.
    // El PullDown asegura que si el botón no se pulsa, leamos '0'.
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  
    // PB10 (Arriba), PB11 (Derecha)
    GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // PE12 (Abajo), PE14 (Izquierda), PE15 (Centro)
    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    
    // Habilitar vector de interrupción en el NVIC (Controlador de Interrupciones)
    // Prioridad 5 (Baja) para no interferir con el Kernel del RTOS (Tick).
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0); 
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/**
  * @brief  Manejador de la Interrupción (ISR) para líneas 10 a 15.
  * Nota: Generalmente iría en stm32f4xx_it.c, pero se incluye aquí por modularidad.
  */
void EXTI15_10_IRQHandler(void) {
    // Delegamos en la HAL para limpiar flags y gestionar el origen
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_10);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_11);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_14);
    HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_15);
}

/**
  * @brief  Callback ejecutado tras detectarse el flanco.
  * Se ejecuta en contexto de ISR -> Debe ser muy breve.
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    // Verificamos si es uno de nuestros pines del Joystick
    if (GPIO_Pin == GPIO_PIN_10 || GPIO_Pin == GPIO_PIN_11 || 
        GPIO_Pin == GPIO_PIN_12 || GPIO_Pin == GPIO_PIN_14 || 
        GPIO_Pin == GPIO_PIN_15) {
            
        // Despertamos al hilo para iniciar la secuencia de debouncing
        osThreadFlagsSet(tid_ThJoy, FLAG_CIC);
    }
}
