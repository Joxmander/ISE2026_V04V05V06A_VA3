/**
 ******************************************************************************
 * @file    CerebroA.c
 * @author  Jose Vargas Gonzaga (modificado por Equipo R.E.A.C.T.)
 * @brief   Orquestador del Nodo A. Traduce eventos de la Web a tramas RPC.
 *
 * @details SPRINT 1: Migración al protocolo RPC unificado.
 *          - Ahora tipo_modo = CMD_START_JUEGO (0x30)
 *          - El modo de juego seleccionado se envía en payload1
 *          - Se elimina el bit de dirección DIR_A_TO_B (innecesario)
 ******************************************************************************
 */

#include "cmsis_os2.h"
#include "comFibraA.h"
#include "CerebroA.h"
#include <string.h>
#include <stdio.h>

/* Conectamos con el buzón de la Web y la variable de texto para mostrar RX */
extern osMessageQueueId_t colaEventosCerebro;
extern char react_rx_trama[30];

/* Misma estructura que usa el CGI para meter eventos en la cola */
typedef struct {
    uint8_t origen;         /* 0 = Web, 1 = Joystick, 2 = FibraRX */
    uint8_t tipo_comando;   /* 1 = Modo Juego, 2 = Trama Debug Manual */
    uint8_t datos[6];       /* Datos crudos del evento */
} MensajeCerebro_t;

void Hilo_Orquestador_Cerebro(void *argument) {
    MensajeCerebro_t msgWeb;
    TramaFibra_t tramaTX;
    TramaFibra_t tramaRX;

    /* Arrancamos el hardware de la fibra óptica */
    Init_ComFibraA();

    while (1) {
        /* === 1. EVENTOS DE LA WEB (selección de modo, debug, etc.) === */
        if (osMessageQueueGet(colaEventosCerebro, &msgWeb, NULL, 10U) == osOK) {

            if (msgWeb.tipo_comando == 1) {
                /* La web ha pedido iniciar un modo de juego.
                 * msgWeb.datos[0] contiene el ID del modo (1..4).
                 * Lo enviamos como CMD_START_JUEGO con el modo en payload1.
                 */
                tramaTX.tipo_modo = CMD_START_JUEGO;       /* 0x30 */
                tramaTX.payload1  = msgWeb.datos[0];       /* ID modo: 1..4 */
                tramaTX.payload2  = 0x00;
                tramaTX.payload3  = 0x00;
                ComFibraA_EnviarTrama(&tramaTX);
            }
            else if (msgWeb.tipo_comando == 2) {
                /* Debug: pedimos un PING al Nodo B para verificar enlace */
                tramaTX.tipo_modo = CMD_PING;              /* 0x10 */
                tramaTX.payload1  = 0x00;
                tramaTX.payload2  = 0x00;
                tramaTX.payload3  = 0x00;
                ComFibraA_EnviarTrama(&tramaTX);
            }
        }

        /* === 2. MENSAJES RECIBIDOS DEL NODO B POR LA FIBRA === */
        if (ComFibraA_RecibirTrama(&tramaRX, 10U) == 0) {

            /* Por ahora volcamos la trama cruda al string que lee la web.
             * En sprints siguientes se interpretará según el comando:
             *   - RESP_PING       (0x20): actualizar telemetría (consumo)
             *   - ACK_START_JUEGO (0x31): confirmar inicio en pantalla
             *   - ACK_GOTO_LOW_POWER (0x41): cambiar estado a "dormido"
             *   - REPORTE_FINAL   (0x50): registrar partida en histórico
             *   - EVENTO_TIEMPO_REAL (0x60): refrescar HUD de la web
             */
            sprintf(react_rx_trama, "[RX] %02X %02X %02X %02X %02X %02X",
                    tramaRX.sof, tramaRX.tipo_modo, tramaRX.payload1,
                    tramaRX.payload2, tramaRX.payload3, tramaRX.checksum);
        }

        osDelay(50);
    }
}
