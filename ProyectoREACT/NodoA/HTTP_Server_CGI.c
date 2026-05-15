/**
 ******************************************************************************
 * @file    HTTP_Server_CGI.c
 * @author  Jose Vargas Gonzaga
 * @brief   Módulo puente entre la Web y el Hardware (CGI).
 * Aquí se procesan los formularios enviados por el usuario y se
 * preparan los datos dinámicos (Hora, Voltaje, Estado REACT) para mostrar en web.
 ******************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cmsis_os2.h"
#include "rl_net.h"
#include "rtc.h"          
#include "lcd.h"          
#include "Board_LED.h"    
#include "CerebroA.h"     
#include "eeprom.h"       

#if      defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#pragma  clang diagnostic push
#pragma  clang diagnostic ignored "-Wformat-nonliteral"
#endif

// === VARIABLES GLOBALES PARA LA WEB REACT ===
char react_rx_trama[30] = "[A la espera de Loopback]";
char react_estado_sistema[60] = "<span style='color:green;'>Activo / 45mA</span>";
char react_nombre_jugador[16] = "Invitado"; 

// === VARIABLES EXTRACCION NODO B, EEPROM Y JOYSTICK ===
extern uint16_t consumo_actual_mA;  
extern RecordJuego_t tabla_records[MAX_RECORDS]; // Para el Top 10
extern uint8_t react_web_nav_trigger;            // Para el salto web con joystick
extern uint8_t modo_juego_actual;                // Modo seleccionado en la placa

// ============================================
// Variables externas
extern uint16_t AD_in (uint32_t ch);
extern bool LEDrun;
extern char lcd_text[2][20+1];
extern uint8_t  get_button (void);

/* --- VARIABLES EXTERNAS DEL APARTADO 5 --- */
extern uint8_t sntp_server_index;
extern RTC_PeriodoAlarma_t periodo_seleccionado;
extern uint8_t alarma_habilitada_web;
extern const char* sntp_servers[];

// Variables Locales.
static uint8_t P2;      
static uint8_t ip_addr[NET_ADDR_IP6_LEN];
static char    ip_string[40];

// My structure of CGI status variable.
typedef struct {
  uint8_t idx;
  uint8_t unused[3];
} MY_BUF;
#define MYBUF(p)        ((MY_BUF *)p)

void netCGI_ProcessQuery (const char *qstr) {
  netIF_Option opt = netIF_OptionMAC_Address;
  int16_t      typ = 0;        
  char var[40];        

  do {
    qstr = netCGI_GetEnvVar (qstr, var, sizeof (var));

    switch (var[0]) {
      case 'i': 
        if (var[1] == '4') { opt = netIF_OptionIP4_Address;       }
        else               { opt = netIF_OptionIP6_StaticAddress; }
        break;
      case 'm': 
        if (var[1] == '4') { opt = netIF_OptionIP4_SubnetMask; }
        break;
      case 'g': 
        if (var[1] == '4') { opt = netIF_OptionIP4_DefaultGateway; } 
        else               { opt = netIF_OptionIP6_DefaultGateway; }
        break;
      case 'p': 
        if (var[1] == '4') { opt = netIF_OptionIP4_PrimaryDNS; }
        else               { opt = netIF_OptionIP6_PrimaryDNS; }
        break;
      case 's': 
        if (var[1] == '4') { opt = netIF_OptionIP4_SecondaryDNS; }
        else               { opt = netIF_OptionIP6_SecondaryDNS; }
        break;
      default: var[0] = '\0'; break;
    }

    switch (var[1]) {
      case '4': typ = NET_ADDR_IP4; break; 
      case '6': typ = NET_ADDR_IP6; break; 
      default: var[0] = '\0'; break;
    }

    if ((var[0] != '\0') && (var[2] == '=')) {
      netIP_aton (&var[3], typ, ip_addr);
      netIF_SetOption (NET_IF_CLASS_ETH, opt, ip_addr, sizeof(ip_addr));
    }
  } while (qstr); 
}

void netCGI_ProcessData (uint8_t code, const char *data, uint32_t len) {
  char var[40];                
  char passw[12];              
  MSGQUEUE_OBJ_LCD_t msg_lcd; 
  bool update_lcd = false;    
  static MensajeCerebro_t msg_react; 

  if (code != 0) return; 

  P2 = 0;            
  LEDrun = true;    

  if (len == 0) { LED_SetOut (P2); return; }

  passw[0] = 1;
  uint8_t alarma_en_formulario = 0;

  do {
    data = netCGI_GetEnvVar (data, var, sizeof (var));
    
    if (var[0] != 0) {
      if      (strcmp (var, "led0=on") == 0) P2 |= 0x01;
      else if (strcmp (var, "led1=on") == 0) P2 |= 0x02;
      else if (strcmp (var, "led2=on") == 0) P2 |= 0x04;
      else if (strcmp (var, "led3=on") == 0) P2 |= 0x08;
      else if (strcmp (var, "led4=on") == 0) P2 |= 0x10;
      else if (strcmp (var, "led5=on") == 0) P2 |= 0x20;
      else if (strcmp (var, "led6=on") == 0) P2 |= 0x40;
      else if (strcmp (var, "led7=on") == 0) P2 |= 0x80;
      else if (strcmp (var, "ctrl=Browser") == 0) LEDrun = false;

      else if (strcmp (var, "ctrl=sleep") == 0) {
          msg_react.origen = 0; 
          msg_react.tipo_comando = 3; 
          if (colaEventosCerebro != NULL) osMessageQueuePut(colaEventosCerebro, &msg_react, 0, 0);
      }
      
      /* --- BORRADO DEL TOP 10 --- */
      else if (strcmp (var, "ctrl=wipe_records") == 0) {
          memset(tabla_records, 0, sizeof(RecordJuego_t) * MAX_RECORDS);
          EEPROM_GuardarRecords(tabla_records);
      }

      else if ((strncmp (var, "pw0=", 4) == 0) || (strncmp (var, "pw2=", 4) == 0)) {
        if (netHTTPs_LoginActive()) {
          if (passw[0] == 1) strcpy (passw, var+4);
          else if (strcmp (passw, var+4) == 0) netHTTPs_SetPassword (passw);
        }
      }
      else if (strncmp (var, "lcd1=", 5) == 0) { strcpy (lcd_text[0], var+5); update_lcd = true; }
      else if (strncmp (var, "lcd2=", 5) == 0) { strcpy (lcd_text[1], var+5); update_lcd = true; }
      else if (strncmp(var, "sntp=", 5) == 0) sntp_server_index = (uint8_t)atoi(&var[5]);
      else if (strcmp(var, "alm_en=on") == 0) alarma_en_formulario = 1;
      else if (strncmp(var, "periodo=", 8) == 0) {
        periodo_seleccionado = (RTC_PeriodoAlarma_t)atoi(&var[8]);
        RTC_ConfigurarAlarma(periodo_seleccionado);
      }
      else if (strncmp(var, "jugador=", 8) == 0) {
          strncpy(react_nombre_jugador, &var[8], 15);
          react_nombre_jugador[15] = '\0'; 
      }
      else if (strncmp(var, "modo=", 5) == 0) {
          msg_react.origen = 0; msg_react.tipo_comando = 1;
          msg_react.datos[0] = (uint8_t)atoi(&var[5]);
          if (colaEventosCerebro != NULL) osMessageQueuePut(colaEventosCerebro, &msg_react, 0, 0);
      }
      else if (strncmp(var, "trama_tx=", 9) == 0) {
          msg_react.origen = 0; msg_react.tipo_comando = 2;
          if (colaEventosCerebro != NULL) osMessageQueuePut(colaEventosCerebro, &msg_react, 0, 0);
      }
    }
  } while (data); 

  alarma_habilitada_web = alarma_en_formulario;
  LED_SetOut (P2);

  if (update_lcd) {
    memset(&msg_lcd, 0, sizeof(MSGQUEUE_OBJ_LCD_t)); 
    strncpy(msg_lcd.Lin1, lcd_text[0], sizeof(msg_lcd.Lin1) - 1);
    strncpy(msg_lcd.Lin2, lcd_text[1], sizeof(msg_lcd.Lin2) - 1);
    msg_lcd.barra = 0; msg_lcd.amplitud = 0;
    osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0);
  }
}

uint32_t netCGI_Script (const char *env, char *buf, uint32_t buflen, uint32_t *pcgi) {
  const char *lang;
  uint32_t len = 0U;            
  uint8_t id;
  static uint32_t adv;          
  netIF_Option opt = netIF_OptionMAC_Address;
  int16_t      typ = 0;
  char t_str[20], d_str[20];    
  
  char id_val = (env[1] == ' ') ? env[2] : env[1];

  switch (env[0]) {
    case 'r':
      if (id_val == '1') len = (uint32_t)sprintf(buf, "%s", react_estado_sistema);
      else if (id_val == '2') len = (uint32_t)sprintf(buf, "%s", react_rx_trama);
      else if (id_val == '3') len = (uint32_t)sprintf(buf, "%u", consumo_actual_mA);
      else if (id_val == '4') {
          // Si el joystick lo pide, avisamos a la web para que navegue
          if (react_web_nav_trigger) {
              len = (uint32_t)sprintf(buf, "NAV:%d", modo_juego_actual);
              react_web_nav_trigger = 0; 
          } else {
              len = (uint32_t)sprintf(buf, "WAIT");
          }
      }
      break;

    case 'h': 
      RTC_ObtenerHoraFecha(t_str, d_str);
      if (id_val == '1') len = (uint32_t)sprintf(buf, "%s", t_str);
      else if (id_val == '2') len = (uint32_t)sprintf(buf, "%s", d_str);
      break;

    // === EXTRACCION TOP 10 DESDE EEPROM ===
    case 'k':
    {
      uint8_t idx = 0;
      char campo = '1';
      if (env[1] == ' ') { idx = env[2] - '0'; campo = env[4]; } 
      else { idx = env[1] - '0'; campo = env[3]; }

      if (idx < MAX_RECORDS) {
        switch (campo) {
          case '1': len = (uint32_t)sprintf(buf, "%s", tabla_records[idx].nombre_jugador[0] ? tabla_records[idx].nombre_jugador : "--"); break;
          case '2': len = (uint32_t)sprintf(buf, "%u", tabla_records[idx].nivel); break;
          case '3': len = (uint32_t)sprintf(buf, "%u", tabla_records[idx].puntuacion); break;
          case '4': len = (uint32_t)sprintf(buf, "%s", tabla_records[idx].fecha_hora[0] ? tabla_records[idx].fecha_hora : "--/--/---- --:--"); break;
        }
      }
      break;
    }

    case 'a' :
      switch (env[3]) {
        case '4': typ = NET_ADDR_IP4; break;
        case '6': typ = NET_ADDR_IP6; break;
        default: return (0);
      }
      switch (env[2]) {
        case 'l': if (env[3] == '4') return (0); else opt = netIF_OptionIP6_LinkLocalAddress; break;
        case 'i': if (env[3] == '4') opt = netIF_OptionIP4_Address; else opt = netIF_OptionIP6_StaticAddress; break;
        case 'm': if (env[3] == '4') opt = netIF_OptionIP4_SubnetMask; else return (0); break;
        case 'g': if (env[3] == '4') opt = netIF_OptionIP4_DefaultGateway; else opt = netIF_OptionIP6_DefaultGateway; break;
        case 'p': if (env[3] == '4') opt = netIF_OptionIP4_PrimaryDNS; else opt = netIF_OptionIP6_PrimaryDNS; break;
        case 's': if (env[3] == '4') opt = netIF_OptionIP4_SecondaryDNS; else opt = netIF_OptionIP6_SecondaryDNS; break;
      }
      netIF_GetOption (NET_IF_CLASS_ETH, opt, ip_addr, sizeof(ip_addr));
      netIP_ntoa (typ, ip_addr, ip_string, sizeof(ip_string));
      len = (uint32_t)sprintf (buf, &env[5], ip_string);
      break;

    case 'b': 
      if (id_val == 'c') {
        len = (uint32_t)sprintf (buf, &env[4], LEDrun ?     ""     : "selected",
                                               LEDrun ? "selected" :    ""     );
        break;
      }
      id = id_val - '0';
      if (id > 7) id = 0;
      id = (uint8_t)(1U << id);
      len = (uint32_t)sprintf (buf, &env[4], (P2 & id) ? "checked" : "");
      break;

    case 'c': 
      break;

    case 'd': 
      switch (id_val) {
        case '1': len = (uint32_t)sprintf (buf, &env[4], netHTTPs_LoginActive() ? "Enabled" : "Disabled"); break;
        case '2': len = (uint32_t)sprintf (buf, &env[4], netHTTPs_GetPassword()); break;
      }
      break;

    case 'e': 
      lang = netHTTPs_GetLanguage();
      if      (strncmp (lang, "en", 2) == 0) lang = "English";
      else if (strncmp (lang, "de", 2) == 0) lang = "German";
      else if (strncmp (lang, "fr", 2) == 0) lang = "French";
      else if (strncmp (lang, "sl", 2) == 0) lang = "Slovene";
      else                                   lang = "Unknown";
      len = (uint32_t)sprintf (buf, &env[2], lang, netHTTPs_GetLanguage());
      break;

    case 'g': 
      switch (id_val) {
        case '1': 
          adv = AD_in (0); 
          len = (uint32_t)sprintf (buf, &env[4], adv);
          break;
        case '2': 
          len = (uint32_t)sprintf (buf, &env[4], (double)((float)adv*3.3f)/4096);
          break;
        case '3': 
          adv = (adv * 100) / 4096;
          len = (uint32_t)sprintf (buf, &env[4], adv);
          break;
      }
      break;
        
    case 's':
      switch (id_val) {
        case '1': 
          if ((env[2] - '0') == sntp_server_index) {
            len = (uint32_t)sprintf(buf, "%s", "checked"); 
          } else {
            len = (uint32_t)sprintf(buf, "%s", "");
          }
          break;
        case '2': 
          if (alarma_habilitada_web) {
            len = (uint32_t)sprintf(buf, "%s", "checked");
          } else {
            len = (uint32_t)sprintf(buf, "%s", "");
          }
          break;
        case '3': 
          if ((env[2] - '0') == (int)periodo_seleccionado) {
            len = (uint32_t)sprintf(buf, "%s", "selected");
          } else {
            len = (uint32_t)sprintf(buf, "%s", "");
          }
          break;
        case '4': 
          len = (uint32_t)sprintf(buf, "%s", sntp_servers[sntp_server_index]);
          break;
      }
      break;

    case 'x':
      adv = AD_in (0);
      len = (uint32_t)sprintf (buf, &env[1], adv);
      break;

    case 'y':
      len = (uint32_t)sprintf (buf, "<checkbox><id>button%c</id><on>%s</on></checkbox>",
                               env[1], (get_button () & (1 << (env[1]-'0'))) ? "true" : "false");
      break;
  }
  return (len);
}

#if      defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#pragma  clang diagnostic pop
#endif
