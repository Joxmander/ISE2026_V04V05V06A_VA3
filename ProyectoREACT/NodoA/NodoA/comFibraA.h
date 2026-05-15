/**
 ******************************************************************************
 * @file    comFibraA.h
 * @author  Jose Vargas Gonzaga
 * @brief   Módulo de comunicación por Fibra Óptica - NODO A (R.E.A.C.T.)
 * En esta cabecera defino la estructura de mi protocolo de comunicación
 * personalizado. He optado por una trama fija de 6 bytes para mantener
 * la predictibilidad y facilitar el trabajo de la máquina de estados.
 ******************************************************************************
 */
 
#ifndef COMFIBRA_A_H
#define COMFIBRA_A_H

#include <stdint.h>
#include <stdbool.h>
#include "cmsis_os2.h"

// Defino mi caracter de inicio de trama (Start of Frame) para sincronización.
// Uso 0xAA (10101010 en binario) porque es un patrón excelente para estabilizar
// las líneas de transmisión óptica.
#define SOF_BYTE 0xAA
#define FRAME_SIZE 6

// Estructura de la trama ya parseada. 
// Una vez que mi ISR valida el Checksum, empaqueto los datos útiles aquí
// para mandarlos limpios a la cola del RTOS.
typedef struct {
    uint8_t tipo_comando;
    uint8_t payload[3];
} TramaFibra_t;

// Expongo el identificador de mi cola de recepción para que el CerebroA pueda leerla.
extern osMessageQueueId_t colaFibraRX;

// Mis prototipos de funciones públicas
void FibraA_Init(void);
void FibraA_SendFrame(uint8_t tipo, uint8_t p1, uint8_t p2, uint8_t p3);

#endif /* COMFIBRA_A_H */
