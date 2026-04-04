/**
 ******************************************************************************
 * @file    comFibraA.h
 * @author  Jose Vargas Gonzaga
 * @brief   Módulo de comunicación por Fibra Óptica - NODO A (R.E.A.C.T.)
 ******************************************************************************
 */

#ifndef COM_FIBRA_A_H
#define COM_FIBRA_A_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"                          
#include "Driver_USART.h"
#include <stdint.h>
#include <stdbool.h>

/* --- Definiciones del Hardware --- */
#define FIBRA_SPEED       115200 
#define MAX_MENSAJES_Q    10      

/* --- Definiciones del Protocolo REACT --- */
#define TRAMA_SOF         0xAA    // Start Of Frame

// Máscaras para el Byte 1 (TIPO_MODO)
#define DIR_B_TO_A        0x80    // Bit 7 a 1: Dato del Nodo B al A
#define DIR_A_TO_B        0x00    // Bit 7 a 0: Comando del Nodo A al B
#define MASK_MODO         0x7F    // Máscara para extraer el Modo (Bits 0-6)

// Catálogo de Modos de Juego
#define MODO_TELEMETRIA   0x00
#define MODO_MEMORIA      0x01
#define MODO_FUERZA       0x02
#define MODO_INHIBICION   0x03
#define MODO_RITMO        0x04

/* --- ESTRUCTURA DE LA TRAMA (6 bytes) --- */
#pragma pack(push, 1) 
typedef struct {
    uint8_t sof;          // Siempre 0xAA
    uint8_t tipo_modo;    // Bit 7: Dirección | Bits 0-6: ID Modo
    uint8_t payload1;     // MSB Tiempo / Bandera
    uint8_t payload2;     // LSB Tiempo
    uint8_t payload3;     // Fuerza ADC / Batería
    uint8_t checksum;     // XOR de los 4 bytes centrales
} TramaFibra_t;
#pragma pack(pop)

/* --- Funciones Públicas --- */
int Init_ComFibraA(void);
int ComFibraA_EnviarTrama(TramaFibra_t* trama);
int ComFibraA_RecibirTrama(TramaFibra_t* out_trama, uint32_t timeout_ms);

#endif /* COM_FIBRA_A_H */
