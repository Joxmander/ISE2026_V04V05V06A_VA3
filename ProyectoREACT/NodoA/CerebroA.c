/**
 ******************************************************************************
 * @file    CerebroA.c
 * @author  Jose Vargas Gonzaga
 * @brief   FASE 4.6: Nombres Libres Web y Menú Acelerado
 ******************************************************************************
 */

#include "CerebroA.h"
#include "comFibraA.h"
#include "cmsis_os2.h"
#include "lcd.h"
#include "joystick.h"
#include "rtc.h"       
#include <stdio.h>
#include <string.h>

extern char react_rx_trama[30];
extern char react_estado_sistema[60];
extern char react_nombre_jugador[16]; // Importamos el nombre libre de la web
extern osMessageQueueId_t id_MsgQueue_Joystick;

typedef enum {
    ESTADO_REPOSO,      
    ESTADO_SEL_MODO,    // Eliminado el ESTADO_SEL_PERFIL para acelerar el Joystick
    ESTADO_JUGANDO      
} EstadoMenu_t;

static EstadoMenu_t estado_actual = ESTADO_REPOSO;
static uint8_t modo_juego_actual = 1; 

#define NUM_MODOS 5
const char* nombres_modos[NUM_MODOS] = {
    "Memoria Trab.", 
    "Ctrl Fuerza", 
    "Inhib. Motora", 
    "Ritmo Const.", 
    "Discriminacion"
};

static uint32_t tiempo_inicio_partida = 0;

void Actualizar_LCD_Menu() {
    MSGQUEUE_OBJ_LCD_t msg_lcd;
    memset(&msg_lcd, 0, sizeof(msg_lcd));
    msg_lcd.barra = 0;
    msg_lcd.amplitud = 0;
    char t_str[20], d_str[20];

    switch(estado_actual) {
        case ESTADO_REPOSO:
            RTC_ObtenerHoraFecha(t_str, d_str);
            snprintf(msg_lcd.Lin1, sizeof(msg_lcd.Lin1), "      %s      ", t_str);
            snprintf(msg_lcd.Lin2, sizeof(msg_lcd.Lin2), "   Pulse Centro   ");
            break;
            
        case ESTADO_SEL_MODO:
            snprintf(msg_lcd.Lin1, sizeof(msg_lcd.Lin1), "MODO:   (Arr/Aba)   ");
            snprintf(msg_lcd.Lin2, sizeof(msg_lcd.Lin2), "-> %-17s", nombres_modos[modo_juego_actual - 1]);
            break;
            
        case ESTADO_JUGANDO:
            snprintf(msg_lcd.Lin1, sizeof(msg_lcd.Lin1), "   PARTIDA ACTIVA   ");
            // Imprimimos el nombre libre (Si vino por joystick, pondrá Invitado)
            snprintf(msg_lcd.Lin2, sizeof(msg_lcd.Lin2), " U:%-16s", react_nombre_jugador);
            break;
    }
    osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0U);
}

void Hilo_Orquestador_Cerebro(void *argument) {
    TramaFibra_t mensajeFibraRX;
    MensajeCerebro_t mensajeWebRX;
    MSGQUEUE_JOY_t mensajeJoyRX;
    osStatus_t estado_fibra;
    
    uint8_t tick_reloj = 0; 
    uint8_t contador_ping = 0; 

    FibraA_Init();
    Actualizar_LCD_Menu();

    while (1) {
        // =========================================================
        // 1. ESCUCHAR A LA WEB
        // =========================================================
        if (osMessageQueueGet(colaEventosCerebro, &mensajeWebRX, NULL, 0U) == osOK) {
            if (mensajeWebRX.tipo_comando == 1) { 
                modo_juego_actual = mensajeWebRX.datos[0];
                
                estado_actual = ESTADO_JUGANDO;
                tiempo_inicio_partida = osKernelGetTickCount(); 
                Actualizar_LCD_Menu();
                FibraA_SendFrame(0x30, modo_juego_actual, 0x00, 0x00); 
            } 
            else if (mensajeWebRX.tipo_comando == 3) { 
                FibraA_SendFrame(0x40, 0x00, 0x00, 0x00);
            }
        } 
        
        // =========================================================
        // 2. ESCUCHAR AL JOYSTICK
        // =========================================================
        if (osMessageQueueGet(id_MsgQueue_Joystick, &mensajeJoyRX, NULL, 0U) == osOK) {
            if (mensajeJoyRX.pulso == PUL_CORTA) { 
                osDelay(50U); 
                
                switch(estado_actual) {
                    case ESTADO_REPOSO:
                        if (mensajeJoyRX.gesto == CENTRO) {
                            // Al usar Joystick, forzamos la variable a Invitado
                            strncpy(react_nombre_jugador, "Invitado", 15);
                            estado_actual = ESTADO_SEL_MODO;
                            Actualizar_LCD_Menu();
                        }
                        break;
                        
                    case ESTADO_SEL_MODO:
                        if (mensajeJoyRX.gesto == ARRIBA) {
                            modo_juego_actual = (modo_juego_actual % NUM_MODOS) + 1;
                        }
                        if (mensajeJoyRX.gesto == ABAJO) {
                            modo_juego_actual = (modo_juego_actual == 1) ? NUM_MODOS : (modo_juego_actual - 1);
                        }
                        if (mensajeJoyRX.gesto == CENTRO) {
                            estado_actual = ESTADO_JUGANDO;
                            tiempo_inicio_partida = osKernelGetTickCount(); 
                            Actualizar_LCD_Menu();
                            FibraA_SendFrame(0x30, modo_juego_actual, 0x00, 0x00);
                        }
                        Actualizar_LCD_Menu();
                        break;
                        
                    case ESTADO_JUGANDO:
                        if (mensajeJoyRX.gesto == IZQUIERDA) { 
                            estado_actual = ESTADO_REPOSO;
                            Actualizar_LCD_Menu();
                        }
                        break;
                }
            }
        }

        // =========================================================
        // 3. PING AL NODO B
        // =========================================================
        contador_ping++;
        if (contador_ping >= 5) {
            FibraA_SendFrame(0x10, 0x00, 0x00, 0x00); 
            contador_ping = 0;
        }

        // =========================================================
        // 4. ESCUCHAR A LA FIBRA ÓPTICA
        // =========================================================
        estado_fibra = osMessageQueueGet(colaFibraRX, &mensajeFibraRX, NULL, 100U); 

        if (estado_fibra == osOK) {
            if (mensajeFibraRX.tipo_comando == 0x50) {
                uint8_t nivel = mensajeFibraRX.payload[1];
                uint8_t puntos = mensajeFibraRX.payload[2];
                snprintf(react_rx_trama, sizeof(react_rx_trama), "FIN: Niv %d | Pts %d", nivel, puntos);
                
                estado_actual = ESTADO_REPOSO; 
                MSGQUEUE_OBJ_LCD_t msg_lcd;
                memset(&msg_lcd, 0, sizeof(msg_lcd));
                snprintf(msg_lcd.Lin1, sizeof(msg_lcd.Lin1), "FIN! Lvl:%d Pts:%d", nivel, puntos);
                snprintf(msg_lcd.Lin2, sizeof(msg_lcd.Lin2), "   Pulse Centro   ");
                osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0U);
            }
            else if (mensajeFibraRX.tipo_comando == 0x20) {
                uint16_t consumo_ma = (mensajeFibraRX.payload[1] << 8) | mensajeFibraRX.payload[2];
                snprintf(react_estado_sistema, sizeof(react_estado_sistema), 
                        "<span style='color:green;'>Activo / %d mA</span>", consumo_ma);
            }
            else if (mensajeFibraRX.tipo_comando == 0x41) {
                snprintf(react_estado_sistema, sizeof(react_estado_sistema), 
                        "<span style='color:orange;'>Modo Sleep / 2mA</span>");
            }
            
        } 
        else if (estado_fibra == osErrorTimeout && contador_ping == 0) {
            snprintf(react_rx_trama, sizeof(react_rx_trama), "[ Enlace Caido ]");
            snprintf(react_estado_sistema, sizeof(react_estado_sistema), "<span style='color:red;'>Desconectado</span>");
        }
        
        // =========================================================
        // 5. ACTUALIZAR RELOJ Y WATCHDOG
        // =========================================================
        if (estado_actual == ESTADO_REPOSO) {
            tick_reloj++;
            if (tick_reloj >= 10) { 
                tick_reloj = 0;
                Actualizar_LCD_Menu(); 
            }
        } 
        else if (estado_actual == ESTADO_JUGANDO) {
            if ((osKernelGetTickCount() - tiempo_inicio_partida) > 6000U) {
                estado_actual = ESTADO_REPOSO;
                Actualizar_LCD_Menu();
            }
        }
    }
}
