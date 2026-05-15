/**
  ******************************************************************************
  * @file    joystick.h
  * @author  Michelle Conto y Jose Vargas
  * @brief   Cabecera del controlador del Joystick (Mbed Application Board).
  * Define los códigos de eventos (gestos), tipos de pulsación y la estructura
  * de mensajes para la comunicación con el módulo Principal.
  ******************************************************************************
  */

#ifndef __JOYSTICK_H
#define __JOYSTICK_H

#include <stdint.h>
#include "cmsis_os2.h"

/* --- Definiciones de Teclas (Bitmask) --- */
/* Se utilizan máscaras de bits para facilitar la identificación.
   Aunque en este diseño se envían individualmente, esto permitiría combinaciones. */
#define ARRIBA      (1U << 1)
#define ABAJO       (1U << 3)
#define DERECHA     (1U << 2)
#define IZQUIERDA   (1U << 4)
#define CENTRO      (1U << 5)

/* --- Definiciones de Tipo de Pulsación --- */
#define PUL_CORTA   (1U << 0) /*!< Pulsación breve (< 1s) al soltar la tecla */
#define PUL_LARGA   (1U << 1) /*!< Pulsación mantenida (>= 1s) */

/* --- Flags internos de Sincronización --- */
#define FLAG_CIC    (1U << 2) /*!< Flag activado por la ISR (Interrupción Hardware) */
#define FLAG_DEBO   (1U << 4) /*!< Flag activado por el Timer (Debounce/Conteo) */

/* --- Estructura del Mensaje --- */
/* Definida en el .h para que sea visible por el receptor (Módulo Principal) */
typedef struct{
    uint8_t  gesto; /*!< Identificador de la tecla (ARRIBA, ABAJO...) */
    uint8_t  pulso; /*!< Tipo de evento (PUL_CORTA o PUL_LARGA) */
} MSGQUEUE_JOY_t;

/* --- Variables Externas --- */
extern osMessageQueueId_t id_MsgQueue_Joystick; /*!< Cola de salida de eventos */
extern osThreadId_t tid_ThJoy;                  /*!< ID del hilo para recibir señales de ISR */

/* --- Funciones --- */
/**
  * @brief  Inicializa los GPIOs, Interrupciones, Timer y Hilo del Joystick.
  * @return 0 si éxito, -1 si error.
  */
int Init_ThJoystick(void);

#endif /* __JOYSTICK_H */
