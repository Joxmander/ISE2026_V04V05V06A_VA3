/**
 ******************************************************************************
 * @file    HTTP_Server_CGI.c
 * @author  Jose Vargas Gonzaga
 * @brief   Módulo CGI para el servidor web del NODO B (Esclavo).
 * En esta placa, el servidor web se mantiene para cumplir con los requisitos
 * de las prácticas (configuración de red, RTC, SNTP), pero he eliminado
 * la lógica del panel REACT, ya que la Estación Base (Nodo A) es la única
 * encargada de gestionar la interfaz de usuario del proyecto final.
 ******************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cmsis_os2.h"
#include "rl_net.h"
#include "rtc.h"          // Necesario para leer la hora
#include "lcd.h"          // Necesario para enviar mensajes al LCD
#include "Board_LED.h"    // ::Board Support:LED


#if      defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
#pragma  clang diagnostic push
#pragma  clang diagnostic ignored "-Wformat-nonliteral"
#endif

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
static uint8_t P2;      // Variable local para guardar el estado de los 8 LEDs de la placa
static uint8_t ip_addr[NET_ADDR_IP6_LEN];
static char    ip_string[40];

// Estructura de variable de estado CGI.
typedef struct {
  uint8_t idx;
  uint8_t unused[3];
} MY_BUF;
#define MYBUF(p)        ((MY_BUF *)p)

/*----------------------------------------------------------------------------
  1. netCGI_ProcessQuery: Procesa datos enviados por la URL (GET)
 *---------------------------------------------------------------------------*/
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
        if (var[1] == '4') { opt = netIF_OptionIP6_DefaultGateway; } 
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

/*----------------------------------------------------------------------------
  2. netCGI_ProcessData: Procesa datos enviados por formularios (POST)
 *---------------------------------------------------------------------------*/
void netCGI_ProcessData (uint8_t code, const char *data, uint32_t len) {
  char var[40];               
  char passw[12];             
  MSGQUEUE_OBJ_LCD_t msg_lcd; 
  bool update_lcd = false;    

  if (code != 0) {
    return; 
  }

  P2 = 0;           
  LEDrun = true;    

  if (len == 0) {
    LED_SetOut (P2);
    return;
  }

  passw[0] = 1;
  uint8_t alarma_en_formulario = 0;

  do {
    data = netCGI_GetEnvVar (data, var, sizeof (var));
    
    if (var[0] != 0) {
      /* --- GESTIÓN DE LEDS --- */
      if      (strcmp (var, "led0=on") == 0) P2 |= 0x01;
      else if (strcmp (var, "led1=on") == 0) P2 |= 0x02;
      else if (strcmp (var, "led2=on") == 0) P2 |= 0x04;
      else if (strcmp (var, "led3=on") == 0) P2 |= 0x08;
      else if (strcmp (var, "led4=on") == 0) P2 |= 0x10;
      else if (strcmp (var, "led5=on") == 0) P2 |= 0x20;
      else if (strcmp (var, "led6=on") == 0) P2 |= 0x40;
      else if (strcmp (var, "led7=on") == 0) P2 |= 0x80;
      else if (strcmp (var, "ctrl=Browser") == 0) LEDrun = false;

      /* --- GESTIÓN DE PASSWORD --- */
      else if ((strncmp (var, "pw0=", 4) == 0) || (strncmp (var, "pw2=", 4) == 0)) {
        if (netHTTPs_LoginActive()) {
          if (passw[0] == 1) strcpy (passw, var+4);
          else if (strcmp (passw, var+4) == 0) netHTTPs_SetPassword (passw);
        }
      }

      /* --- GESTIÓN DE MI PANTALLA LCD --- */
      else if (strncmp (var, "lcd1=", 5) == 0) {
        strcpy (lcd_text[0], var+5); 
        update_lcd = true;
      }
      else if (strncmp (var, "lcd2=", 5) == 0) {
        strcpy (lcd_text[1], var+5); 
        update_lcd = true;
      }

      /* --- GESTIÓN APARTADO 5 (SNTP Y ALARMA RTC) --- */
      else if (strncmp(var, "sntp=", 5) == 0) {
        sntp_server_index = (uint8_t)atoi(&var[5]);
      }
      else if (strcmp(var, "alm_en=on") == 0) {
        alarma_en_formulario = 1;
      }
      else if (strncmp(var, "periodo=", 8) == 0) {
        periodo_seleccionado = (RTC_PeriodoAlarma_t)atoi(&var[8]);
        RTC_ConfigurarAlarma(periodo_seleccionado); 
      }
    }
  } while (data); 

  alarma_habilitada_web = alarma_en_formulario;
  LED_SetOut (P2);

  if (update_lcd) {
    memset(&msg_lcd, 0, sizeof(MSGQUEUE_OBJ_LCD_t)); 
    strncpy(msg_lcd.Lin1, lcd_text[0], sizeof(msg_lcd.Lin1) - 1);
    strncpy(msg_lcd.Lin2, lcd_text[1], sizeof(msg_lcd.Lin2) - 1);
    msg_lcd.barra = 0;
    msg_lcd.amplitud = 0;
    osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0);
  }
}

/*----------------------------------------------------------------------------
  3. netCGI_Script: El "Generador" de contenido dinámico.
 *---------------------------------------------------------------------------*/
uint32_t netCGI_Script (const char *env, char *buf, uint32_t buflen, uint32_t *pcgi) {
  int32_t socket;
  netTCP_State state;
  NET_ADDR r_client;
  const char *lang;
  uint32_t len = 0U;            
  uint8_t id;
  static uint32_t adv;          
  netIF_Option opt = netIF_OptionMAC_Address;
  int16_t      typ = 0;
    
  char t_str[20], d_str[20];    

  switch (env[0]) {
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
      if (env[2] == 'c') {
        len = (uint32_t)sprintf (buf, &env[4], LEDrun ?     ""     : "selected",
                                               LEDrun ? "selected" :    ""     );
        break;
      }
      id = env[2] - '0';
      if (id > 7) id = 0;
      id = (uint8_t)(1U << id);
      len = (uint32_t)sprintf (buf, &env[4], (P2 & id) ? "checked" : "");
      break;

    case 'c': 
      while ((uint32_t)(len + 150) < buflen) {
        socket = ++MYBUF(pcgi)->idx;
        state  = netTCP_GetState (socket);
        if (state == netTCP_StateINVALID) return ((uint32_t)len);
        
        len += (uint32_t)sprintf (buf+len,   "<tr align=\"center\">");
        if (state <= netTCP_StateCLOSED) {
          len += (uint32_t)sprintf (buf+len, "<td>%d</td><td>%d</td><td>-</td><td>-</td><td>-</td><td>-</td></tr>\r\n", socket, netTCP_StateCLOSED);
        } else if (state == netTCP_StateLISTEN) {
          len += (uint32_t)sprintf (buf+len, "<td>%d</td><td>%d</td><td>%d</td><td>-</td><td>-</td><td>-</td></tr>\r\n", socket, netTCP_StateLISTEN, netTCP_GetLocalPort(socket));
        } else {
          netTCP_GetPeer (socket, &r_client, sizeof(r_client));
          netIP_ntoa (r_client.addr_type, r_client.addr, ip_string, sizeof (ip_string));
          len += (uint32_t)sprintf (buf+len, "<td>%d</td><td>%d</td><td>%d</td><td>%d</td><td>%s</td><td>%d</td></tr>\r\n", socket, netTCP_StateLISTEN, netTCP_GetLocalPort(socket), netTCP_GetTimer(socket), ip_string, r_client.port);
        }
      }
      len |= (1u << 31); 
      break;

    case 'd': 
      switch (env[2]) {
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
      switch (env[2]) {
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

    case 'h': 
      RTC_ObtenerHoraFecha(t_str, d_str);
      if (env[1] == '1') {
        len = (uint32_t)sprintf(buf, "%s", t_str);
      }
      else if (env[1] == '2') {
        len = (uint32_t)sprintf(buf, "%s", d_str);
      }
      break;
        
    case 's':
      switch (env[1]) {
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
