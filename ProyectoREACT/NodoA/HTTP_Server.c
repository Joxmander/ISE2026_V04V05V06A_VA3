/**
 ******************************************************************************
 * @file    HTTP_Server.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del servidor HTTP
 * Contiene la lógica principal para atender las peticiones de la
 * red y servir las páginas estáticas y dinámicas. Este módulo
 * coordina las llamadas a CGI, gestión de sockets y mantenimiento
 * de la conexión con el hardware (ADC, RTC, etc.).
 ******************************************************************************
 */
    
#include <stdio.h>
#include "string.h"
#include <stdlib.h>

#include "rl_net.h"                     // Keil.MDK-Pro::Network:CORE
#include "stm32f4xx_hal.h"              // Keil::Device:STM32Cube HAL:Common
#include "Board_LED.h"                  // ::Board Support:LED
#include "main.h"

// Cabeceras de mis librerias
#include "lcd.h"
#include "adc.h"
#include "rtc.h"
#include "CerebroA.h"

/* --- CONFIGURACIÓN DEL HILO PRINCIPAL (app_main) --- */
#define APP_MAIN_STK_SZ (2048U)        
uint64_t app_main_stk[APP_MAIN_STK_SZ / 8];
const osThreadAttr_t app_main_attr = {
  .stack_mem  = &app_main_stk[0],
  .stack_size = sizeof(app_main_stk)
};

/* --- VARIABLES GLOBALES COMPARTIDAS CON LA WEB --- */
bool LEDrun;                                
char lcd_text[2][20+1];                     
uint8_t iniciar_parpadeo_sntp = 0;          

/* --- VARIABLES PARA EL APARTADO 5 (Configuración Web) --- */
uint8_t sntp_server_index = 0;              
const char* sntp_servers[] = {"Google NTP (216.239.35.0)", "Cloudflare NTP (162.159.200.1)"}; 
const char* sntp_ips[] = {"216.239.35.0", "162.159.200.1"}; 

RTC_PeriodoAlarma_t periodo_seleccionado = ALARMA_CADA_1_MIN; 
uint8_t alarma_habilitada_web = 1;          

/* --- RECURSOS DE PERIFÉRICOS Y RED --- */
ADC_HandleTypeDef hadc1;                    
static NET_ADDR server_addr;                

/* --- MIS RECURSOS DE TIMERS DEL RTOS --- */
osTimerId_t timer_led_rojo;
osTimerId_t timer_led_verde;

// Variable Global para comunicar Web y CerebroA (Declarada extern en CerebroA.h)
osMessageQueueId_t colaEventosCerebro = NULL;

extern void ResetPulsosRojo(void);
extern void ResetPulsosVerde(void);
extern void TimerRojo_Callback (void *argument);
extern void TimerVerde_Callback (void *argument);

void Time_Thread (void *argument);
void Sincronizacion_SNTP_Completada(uint32_t segundos_unix, uint32_t fraccion);
__NO_RETURN void app_main (void *arg);

uint16_t AD_in (uint32_t ch) {
  ADC_ChannelConfTypeDef sConfig = {0};
  
  sConfig.Channel = ADC_CHANNEL_10; 
  sConfig.Rank = 1;    
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;     
  
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
  HAL_ADC_Start(&hadc1); 
  
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) {
    return (uint16_t)HAL_ADC_GetValue(&hadc1);
  }
  return 0; 
}

uint8_t get_button (void) { return 0; }
void netDHCP_Notify (uint32_t if_num, uint8_t option, const uint8_t *val, uint32_t len) { }

void Time_Thread (void *argument) {
  MSGQUEUE_OBJ_LCD_t msg_lcd;      
  char t_buffer[20], d_buffer[20]; 
  
  uint32_t contador_sntp_segundos = 0; 
  uint8_t  divisor_100ms = 0;          

  RTC_Init();
  RTC_ConfigurarAlarma(periodo_seleccionado); 

  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_14;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  while (1) {
    osDelay(100); 

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) {
        RTC_Reset_A_2000();         
        contador_sntp_segundos = 0; 
    }

    if (alarma_activada == 1) {
        alarma_activada = 0; 
        if (alarma_habilitada_web == 1) {
            osTimerStop(timer_led_verde); 
            ResetPulsosVerde();            
            osTimerStart(timer_led_verde, 200U); 
        }
    }

    if (iniciar_parpadeo_sntp == 1) {
        iniciar_parpadeo_sntp = 0; 
        osTimerStop(timer_led_rojo); 
        ResetPulsosRojo();         
        osTimerStart(timer_led_rojo, 100U); 
    }

    divisor_100ms++;
    if (divisor_100ms >= 10) {
        divisor_100ms = 0; 
        
        RTC_ObtenerHoraFecha(t_buffer, d_buffer);
        memset(&msg_lcd, 0, sizeof(msg_lcd));
        strcpy(msg_lcd.Lin1, t_buffer); 
        strcpy(msg_lcd.Lin2, d_buffer); 
        osMessageQueuePut(mid_messageQueueLCD, &msg_lcd, 0, 0); 
        
        contador_sntp_segundos++;
        if (contador_sntp_segundos == 5 || (contador_sntp_segundos > 5 && (contador_sntp_segundos % 180 == 0))) {
            server_addr.addr_type = NET_ADDR_IP4;
            server_addr.port = 0; 
            netIP_aton(sntp_ips[sntp_server_index], NET_ADDR_IP4, server_addr.addr);
            netSNTPc_GetTime(&server_addr, Sincronizacion_SNTP_Completada);
        }
    }
  }
}

void Sincronizacion_SNTP_Completada(uint32_t segundos_unix, uint32_t fraccion) {
    if (segundos_unix > 0) {
        RTC_ActualizarDesdeUnix(segundos_unix);
        iniciar_parpadeo_sntp = 1; 
    }
}

__NO_RETURN void app_main (void *arg) {
  (void)arg;

  LED_Initialize();
  Init_ThLCD(); 
    
  ADC1_pins_F429ZI_config(); 
  ADC_Init_Single_Conversion(&hadc1, ADC1); 
    
  netInitialize (); 

  timer_led_rojo = osTimerNew(TimerRojo_Callback, osTimerPeriodic, NULL, NULL);
  timer_led_verde = osTimerNew(TimerVerde_Callback, osTimerPeriodic, NULL, NULL);

  // === CÓDIGO REACT ===
  colaEventosCerebro = osMessageQueueNew(10, sizeof(MensajeCerebro_t), NULL);
  osThreadNew(Hilo_Orquestador_Cerebro, NULL, NULL);
  
  osThreadNew (Time_Thread, NULL, NULL); 
  osThreadExit();
}
