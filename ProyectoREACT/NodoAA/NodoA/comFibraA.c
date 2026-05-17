/**
 ******************************************************************************
 * @file    CerebroA.c
 * @author  Jose Vargas Gonzaga
 ******************************************************************************
 */

#include "CerebroA.h"
#include "comFibraA.h"
#include "cmsis_os2.h"
#include "lcd.h"
#include "joystick.h"
#include "rtc.h"       
#include "eeprom.h"    
#include <stdio.h>
#include <string.h>
#include <stdbool.h> // Necesario para los booleanos

extern char react_rx_trama[30];
extern char react_estado_sistema[60];
extern char react_nombre_jugador[16]; 
extern osMessageQueueId_t id_MsgQueue_Joystick;

uint16_t consumo_actual_mA = 0;

// === NUEVA VARIABLE PARA CONTROLAR EL SLEEP ===
static bool nodo_b_dormido = false;

typedef enum {
    ESTADO_REPOSO,      
    ESTADO_SEL_MODO,    
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
    
    if (EEPROM_Init()) {
        EEPROM_CargarRecords(tabla_records);
        if (tabla_records[0].puntuacion == 0xFFFFFFFF) {
            memset(tabla_records, 0, sizeof(RecordJuego_t) * MAX_RECORDS);
            EEPROM_GuardarRecords(tabla_records);
        }
    }
    Actualizar_LCD_Menu();

    while (1) {
        // 1. ESCUCHAR A LA WEB
        if (osMessageQueueGet(colaEventosCerebro, &mensajeWebRX, NULL, 0U) == osOK) {
            if (mensajeWebRX.tipo_comando == 1) { 
                modo_juego_actual = mensajeWebRX.datos[0];
                estado_actual = ESTADO_JUGANDO;
                tiempo_inicio_partida = osKernelGetTickCount(); 
                Actualizar_LCD_Menu();
                
                // Si estaba dormido, al mandar juego se despertará
                nodo_b_dormido = false; 
                FibraA_SendFrame(0x30, modo_juego_actual, 0x00, 0x00); 
            } 
            else if (mensajeWebRX.tipo_comando == 3) { 
                FibraA_SendFrame(0x40, 0x00, 0x00, 0x00);
            }
        } 
        
        // 2. ESCUCHAR AL JOYSTICK
        if (osMessageQueueGet(id_MsgQueue_Joystick, &mensajeJoyRX, NULL, 0U) == osOK) {
            if (mensajeJoyRX.pulso == PUL_CORTA) { 
                osDelay(50U); 
                switch(estado_actual) {
                    case ESTADO_REPOSO:
                        if (mensajeJoyRX.gesto == CENTRO) {
                            strncpy(react_nombre_jugador, "Invitado", 15);
                            estado_actual = ESTADO_SEL_MODO;
                            Actualizar_LCD_Menu();
                        }
                        break;
                    case ESTADO_SEL_MODO:
                        if (mensajeJoyRX.gesto == ARRIBA) modo_juego_actual = (modo_juego_actual % NUM_MODOS) + 1;
                        if (mensajeJoyRX.gesto == ABAJO) modo_juego_actual = (modo_juego_actual == 1) ? NUM_MODOS : (modo_juego_actual - 1);
                        if (mensajeJoyRX.gesto == CENTRO) {
                            estado_actual = ESTADO_JUGANDO;
                            tiempo_inicio_partida = osKernelGetTickCount(); 
                            Actualizar_LCD_Menu();
                            
                            nodo_b_dormido = false; 
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

        // 3. PING AL NODO B (Solo si NO está dormido)
        if (!nodo_b_dormido) {
            contador_ping++;
            if (contador_ping >= 5) {
                FibraA_SendFrame(0x10, 0x00, 0x00, 0x00); 
                contador_ping = 0;
            }
        }

        // 4. ESCUCHAR A LA FIBRA ÓPTICA
        estado_fibra = osMessageQueueGet(colaFibraRX, &mensajeFibraRX, NULL, 100U); 
        if (estado_fibra == osOK) {
            if (mensajeFibraRX.tipo_comando == 0x50) {
                uint8_t nivel = mensajeFibraRX.payload[1];
                uint8_t puntos = mensajeFibraRX.payload[2];
                char t_str[20], d_str[20];
                int i, j, pos_insertar = -1;
                MSGQUEUE_OBJ_LCD_t msg_lcd;
                
                snprintf(react_rx_trama, sizeof(react_rx_trama), "FIN: Niv %d | Pts %d", nivel, puntos);
                RTC_ObtenerHoraFecha(t_str, d_str);
                
                for (i = 0; i < MAX_RECORDS; i++) {
                    if (puntos >= tabla_records[i].puntuacion || tabla_records[i].puntuacion == 0) {
                        pos_insertar = i; break;
                    }
                }
                
                if (pos_insertar != -1) {
                    for (j = MAX_RECORDS - 1; j > pos_insertar; j--) {
                        tabla_records[j] = tabla_records[j - 1];
                    }
                    strncpy(tabla_records[pos_insertar].nombre_jugador, react_nombre_jugador, 15);
                    tabla_records[pos_insertar].nombre_jugador[15] = '\0';
                    tabla_records[pos_insertar].nivel = nivel;
                    tabla_records[pos_insertar].puntuacion = (uint32_t)puntos;
                    snprintf(tabla_records[pos_insertar].fecha_hora, sizeof(tabla_records[pos_insertar].fecha_hora), "%s %s", d_str, t_str);
                    EEPROM_GuardarRecords(tabla_records);
                }
                
                estado_actual = ESTADO_REPOSO; 
                memset(&msg_lcd, 0, sizeof(msg_lcd));
                snprintf(msg_lcd.Lin1, sizeof(msg_lcd.Lin1), "FIN! Lvl:%d Pts:%d", nivel, puntos);
                snprintf(msg_lcd.Lin2, sizeof(msg_lcd.Lin2), "   Pulse Centro   ");
                osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0U);
            }
            else if (mensajeFibraRX.tipo_comando == 0x20) {
                nodo_b_dormido = false; // Si responde al ping, es que está despierto
                consumo_actual_mA = (mensajeFibraRX.payload[1] << 8) | mensajeFibraRX.payload[2];
                snprintf(react_estado_sistema, sizeof(react_estado_sistema), 
                        "<span style='color:green;'>Activo / %d mA</span>", consumo_actual_mA);
            }
            else if (mensajeFibraRX.tipo_comando == 0x41) {
                nodo_b_dormido = true; // EL NODO B NOS CONFIRMA QUE SE HA DORMIDO
                consumo_actual_mA = 2; 
                snprintf(react_estado_sistema, sizeof(react_estado_sistema), 
                        "<span style='color:orange;'>Modo Sleep / 2mA</span>");
            }
        } 
        else if (estado_fibra == osErrorTimeout && contador_ping == 0 && !nodo_b_dormido) {
            snprintf(react_rx_trama, sizeof(react_rx_trama), "[ Enlace Caido ]");
            snprintf(react_estado_sistema, sizeof(react_estado_sistema), "<span style='color:red;'>Desconectado</span>");
            consumo_actual_mA = 0; 
        }
        
        if (estado_actual == ESTADO_REPOSO) {
            tick_reloj++;
            if (tick_reloj >= 10) { 
                tick_reloj = 0; Actualizar_LCD_Menu(); 
            }
        } else if (estado_actual == ESTADO_JUGANDO) {
            if ((osKernelGetTickCount() - tiempo_inicio_partida) > 6000U) {
                estado_actual = ESTADO_REPOSO; Actualizar_LCD_Menu();
            }
        }
    }
}