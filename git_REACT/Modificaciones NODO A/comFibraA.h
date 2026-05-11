/**
 ******************************************************************************
 * @file    comFibraA.h
 * @author  Jose Vargas Gonzaga (modificado por Equipo R.E.A.C.T.)
 * @brief   Módulo de comunicación por Fibra Óptica - NODO A (R.E.A.C.T.)
 *
 * @details SPRINT 1: Migración al protocolo RPC unificado.
 *          Se eliminan los antiguos defines MODO_xxx que codificaban el
 *          modo en el byte tipo_modo. Ahora tipo_modo lleva un COMANDO
 *          (PING/START/SLEEP/...) y el modo de juego va en payload1.
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

/* --- Definiciones del Protocolo REACT (RPC unificado) --- */
#define TRAMA_SOF         0xAA    /* Start Of Frame                 */

/* === CATÁLOGO DE COMANDOS RPC (byte tipo_modo) ====================
 * Se eliminan los bits de dirección DIR_A_TO_B / DIR_B_TO_A: el nodo
 * receptor ya sabe quién es y los comandos están separados por origen
 * (los 0x1x/0x3x/0x4x los emite A, los 0x2x/0x3x ACK/0x5x/0x6x los
 * emite B). No hay ambigüedad y nos ahorramos un bit.
 * ==================================================================*/

/* Comandos que emite el Nodo A */
#define CMD_PING               0x10    /* Petición de estado/telemetría    */
#define CMD_START_JUEGO        0x30    /* payload1 = ID del modo (1..4)    */
#define CMD_GOTO_LOW_POWER     0x40    /* Orden de entrar en bajo consumo  */

/* Comandos/respuestas que emite el Nodo B */
#define RESP_PING              0x20    /* Estado + consumo (mA en p1:p2)   */
#define ACK_START_JUEGO        0x31    /* payload1 = ID del modo iniciado  */
#define ACK_GOTO_LOW_POWER     0x41    /* "Voy a dormir"                   */
#define REPORTE_FINAL          0x50    /* p1=modo, p2=nivel, p3=fallos/pts */
#define EVENTO_TIEMPO_REAL     0x60    /* p1=pad, p2=fuerza/acierto, p3=tr */

/* IDs de Modo de Juego (van en payload1 del CMD_START_JUEGO).
 * Mantenemos los valores originales para no romper la web ni el CGI:
 *   MEMORIA=1, FUERZA=2, INHIBICION=3, RITMO=4
 */
#define MODO_MEMORIA      0x01
#define MODO_FUERZA       0x02
#define MODO_INHIBICION   0x03
#define MODO_RITMO        0x04

/* --- ESTRUCTURA DE LA TRAMA (6 bytes) ---
 * Mantenemos la struct y su tamaño para no romper el resto del código.
 * Lo que cambia es el SIGNIFICADO de tipo_modo (ahora es tipo_comando).
 * Conservamos el nombre del campo por compatibilidad con código existente.
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t sof;          /* Siempre 0xAA                                    */
    uint8_t tipo_modo;    /* Ahora: COMANDO RPC (CMD_xxx, RESP_xxx, ACK_xxx) */
    uint8_t payload1;     /* Significado depende del comando                 */
    uint8_t payload2;     /* Significado depende del comando                 */
    uint8_t payload3;     /* Significado depende del comando                 */
    uint8_t checksum;     /* XOR de los 4 bytes centrales                    */
} TramaFibra_t;
#pragma pack(pop)

/* --- Funciones Públicas --- */
int Init_ComFibraA(void);
int ComFibraA_EnviarTrama(TramaFibra_t* trama);
int ComFibraA_RecibirTrama(TramaFibra_t* out_trama, uint32_t timeout_ms);

#endif /* COM_FIBRA_A_H */
