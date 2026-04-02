/**
 ******************************************************************************
 * @file    comFibraB.h
 * @author  Jose Vargas Gonzaga
 * @brief   Módulo de comunicación por Fibra Óptica (Protocolo Binario) - NODO B
 ******************************************************************************
 */

#ifndef COM_FIBRA_B_H
#define COM_FIBRA_B_H

#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"                          
#include "Driver_USART.h"
#include <stdint.h>
#include <stdbool.h>

/* --- Definiciones del Protocolo Binario (Idénticas al Nodo A) --- */
#define TRAMA_START_BYTE  0xAA    
#define TRAMA_END_BYTE    0x55    

#define TIPO_COMANDO      0x01    // Del Nodo A al Nodo B (Órdenes)
#define TIPO_EVENTO       0x02    // Del Nodo B al Nodo A (Sensores)

/* Catálogo de Órdenes */
#define CMD_START_JUEGO   0x10
#define CMD_STOP_JUEGO    0x11
#define CMD_SLEEP_NODO    0x12

/* Catálogo de Eventos */
#define EVT_HIT_SENSOR    0x20
#define EVT_MISS_SENSOR   0x21
#define EVT_FIN_TIEMPO    0x22
#define EVT_BATERIA_MAH   0x23

/* Estructura de Trama (6 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t start_byte;
    uint8_t tipo;      
    uint8_t id_accion; 
    uint8_t param1;    
    uint8_t param2;    
    uint8_t end_byte;  
} TramaFibra_t;
#pragma pack(pop)

/* --- Funciones Públicas --- */
int Init_ComFibraB(void);
int ComFibraB_EnviarTrama(const TramaFibra_t* trama);
int ComFibraB_RecibirTrama(TramaFibra_t* out_trama, uint32_t timeout_ms);

#endif /* COM_FIBRA_B_H */
