#include "CerebroA.h"
#include "comFibraA.h"
#include "cmsis_os2.h"
#include "lcd.h"
#include "joystick.h"
#include "rtc.h"       
#include "eeprom.h"    
#include <stdio.h>
#include <string.h>

extern char react_rx_trama[30];
extern char react_estado_sistema[60];
extern char react_nombre_jugador[16]; 
extern osMessageQueueId_t id_MsgQueue_Joystick;

uint16_t consumo_actual_mA = 45;
uint8_t react_web_nav_trigger = 0; // NUEVO: Avisa a la Web que cambie de pagina

typedef enum { ESTADO_REPOSO, ESTADO_SEL_MODO, ESTADO_JUGANDO } EstadoMenu_t;
static EstadoMenu_t estado_actual = ESTADO_REPOSO;
uint8_t modo_juego_actual = 1; 

void Actualizar_LCD_Menu() {
    MSGQUEUE_OBJ_LCD_t msg_lcd;
    memset(&msg_lcd, 0, sizeof(msg_lcd));
    char t_str[20], d_str[20];

    switch(estado_actual) {
        case ESTADO_REPOSO:
            RTC_ObtenerHoraFecha(t_str, d_str);
            snprintf(msg_lcd.Lin1, 20, "      %s      ", t_str);
            snprintf(msg_lcd.Lin2, 20, "   Pulse Centro   ");
            break;
        case ESTADO_SEL_MODO:
            snprintf(msg_lcd.Lin1, 20, "MODO:   (Arr/Aba)   ");
            snprintf(msg_lcd.Lin2, 20, "-> Modo %d        ", modo_juego_actual);
            break;
        case ESTADO_JUGANDO:
            snprintf(msg_lcd.Lin1, 20, "   PARTIDA ACTIVA   ");
            snprintf(msg_lcd.Lin2, 20, " U:%-16s", react_nombre_jugador);
            break;
    }
    osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0U);
}

void Hilo_Orquestador_Cerebro(void *argument) {
    TramaFibra_t mensajeFibraRX;
    MensajeCerebro_t mensajeWebRX;
    MSGQUEUE_JOY_t mensajeJoyRX;
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
        if (osMessageQueueGet(colaEventosCerebro, &mensajeWebRX, NULL, 0U) == osOK) {
            if (mensajeWebRX.tipo_comando == 1) { 
                modo_juego_actual = mensajeWebRX.datos[0];
                estado_actual = ESTADO_JUGANDO;
                FibraA_SendFrame(0x30, modo_juego_actual, 0x00, 0x00);
                Actualizar_LCD_Menu();
            }
        } 
        
        if (osMessageQueueGet(id_MsgQueue_Joystick, &mensajeJoyRX, NULL, 0U) == osOK) {
            if (mensajeJoyRX.pulso == PUL_CORTA) { 
                osDelay(50U); 
                switch(estado_actual) {
                    case ESTADO_REPOSO:
                        if (mensajeJoyRX.gesto == CENTRO) {
                            estado_actual = ESTADO_SEL_MODO;
                        }
                        break;
                    case ESTADO_SEL_MODO:
                        if (mensajeJoyRX.gesto == ARRIBA) modo_juego_actual = (modo_juego_actual % 4) + 1;
                        if (mensajeJoyRX.gesto == ABAJO)  modo_juego_actual = (modo_juego_actual == 1) ? 4 : modo_juego_actual - 1;
                        if (mensajeJoyRX.gesto == CENTRO) {
                            estado_actual = ESTADO_JUGANDO;
                            react_web_nav_trigger = 1; // <--- DISPARAMOS NAVEGACION WEB
                            FibraA_SendFrame(0x30, modo_juego_actual, 0x00, 0x00);
                        }
                        break;
                    case ESTADO_JUGANDO:
                        if (mensajeJoyRX.gesto == IZQUIERDA) estado_actual = ESTADO_REPOSO;
                        break;
                }
                Actualizar_LCD_Menu();
            }
        }

        contador_ping++;
        if (contador_ping >= 5) {
            FibraA_SendFrame(0x10, 0x00, 0x00, 0x00); 
            contador_ping = 0;
        }

        if (osMessageQueueGet(colaFibraRX, &mensajeFibraRX, NULL, 100U) == osOK) {
            if (mensajeFibraRX.tipo_comando == 0x50) {
                uint8_t niv = mensajeFibraRX.payload[1], pts = mensajeFibraRX.payload[2];
                char t_s[20], d_s[20];
                int pos = -1;
                
                snprintf(react_rx_trama, 30, "FIN: Niv %d | Pts %d", niv, pts);
                RTC_ObtenerHoraFecha(t_s, d_s);
                
                for (int i = 0; i < MAX_RECORDS; i++) {
                    if (pts >= tabla_records[i].puntuacion || tabla_records[i].puntuacion == 0) {
                        pos = i; break;
                    }
                }
                if (pos != -1) {
                    for (int j = MAX_RECORDS - 1; j > pos; j--) tabla_records[j] = tabla_records[j - 1];
                    strncpy(tabla_records[pos].nombre_jugador, react_nombre_jugador, 15);
                    tabla_records[pos].nombre_jugador[15] = '\0';
                    tabla_records[pos].nivel = niv; tabla_records[pos].puntuacion = pts;
                    snprintf(tabla_records[pos].fecha_hora, 20, "%s %s", d_s, t_s);
                    EEPROM_GuardarRecords(tabla_records);
                }
                
                estado_actual = ESTADO_REPOSO; 
                MSGQUEUE_OBJ_LCD_t m; memset(&m, 0, sizeof(m));
                snprintf(m.Lin1, 20, "RESULTADO FINAL ");
                snprintf(m.Lin2, 20, "Niv:%d  Pts:%d", niv, pts);
                osMessageQueuePut(mid_messageQueueLCD, &m, 0, 0U);
            }
        } 
    }
}
