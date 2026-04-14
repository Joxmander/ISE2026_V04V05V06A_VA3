/**
 ******************************************************************************
 * @file    CerebroA.c
 * @author  Jose Vargas Gonzaga
 * @brief   FASE 4.2: Gestión de perfiles y pasarela inteligente.
 ******************************************************************************
 */

#include "CerebroA.h"
#include "comFibraA.h"
#include "cmsis_os2.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>

extern char react_rx_trama[30];
extern char react_estado_sistema[60];

// ID de usuario actual (0 = Invitado, >0 = Perfil Web)
static uint16_t id_usuario_actual = 0; 

void Hilo_Orquestador_Cerebro(void *argument) {
    TramaFibra_t mensajeFibraRX;
    MensajeCerebro_t mensajeWebRX;
    MSGQUEUE_OBJ_LCD_t msg_lcd;
    osStatus_t estado_fibra;

    FibraA_Init();

    while (1) {
        // 1. ESCUCHAR A LA WEB
        if (osMessageQueueGet(colaEventosCerebro, &mensajeWebRX, NULL, 0U) == osOK) {
            
            if (mensajeWebRX.tipo_comando == 1) { // INICIAR JUEGO
                id_usuario_actual = mensajeWebRX.datos[1]; // Supongamos que el byte 1 es el ID
                uint8_t modo = mensajeWebRX.datos[0];
                
                // Si el ID es 0, en el LCD pondremos "Invitado"
                FibraA_SendFrame(0x30, modo, 0x00, 0x00); 
            } 
            else if (mensajeWebRX.tipo_comando == 3) { // BAJO CONSUMO
                FibraA_SendFrame(0x40, 0x00, 0x00, 0x00);
            }
        } 
        else {
            FibraA_SendFrame(0x10, 0x00, 0x00, 0x00); // PING
        }

        // 2. ESCUCHAR A LA FIBRA
        estado_fibra = osMessageQueueGet(colaFibraRX, &mensajeFibraRX, NULL, 500U);

        if (estado_fibra == osOK) {
            
            // Caso: Reporte Final de Partida (Trama 0x50 personalizada)
            if (mensajeFibraRX.tipo_comando == 0x50) {
                uint8_t nivel = mensajeFibraRX.payload[1];
                uint8_t puntos = mensajeFibraRX.payload[2];
                
                // ACTUALIZAR WEB
                snprintf(react_rx_trama, sizeof(react_rx_trama), "FIN: Niv %d | Pts %d", nivel, puntos);
                
                // ACTUALIZAR LCD
                memset(&msg_lcd, 0, sizeof(msg_lcd));
                snprintf(msg_lcd.Lin1, sizeof(msg_lcd.Lin1), "FIN DE PARTIDA");
                snprintf(msg_lcd.Lin2, sizeof(msg_lcd.Lin2), "U:%s Lvl:%d", (id_usuario_actual==0)?"INV":"WEB", nivel);
                osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0U);
                
                // TODO: Aquí llamaríamos a la función de la EEPROM:
                // EEPROM_GuardarResultado(id_usuario_actual, nivel, puntos);
            }
            
            // Caso: Respuesta de Telemetría (Ping) con Consumo Real
            else if (mensajeFibraRX.tipo_comando == 0x20) {
                // Reconstruimos el valor de mA (2 bytes)
                uint16_t consumo_ma = (mensajeFibraRX.payload[1] << 8) | mensajeFibraRX.payload[2];
                
                snprintf(react_estado_sistema, sizeof(react_estado_sistema), 
                        "<span style='color:green;'>Activo / %d mA</span>", consumo_ma);
            }

            // ... (resto de casos de ACKs de modo y sleep)
            
        } else if (estado_fibra == osErrorTimeout) {
            snprintf(react_rx_trama, sizeof(react_rx_trama), "[ Enlace Caido ]");
            snprintf(react_estado_sistema, sizeof(react_estado_sistema), "<span style='color:red;'>Desconectado</span>");
        }
    }
}
