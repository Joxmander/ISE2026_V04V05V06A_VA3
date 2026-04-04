#include "cmsis_os2.h"
#include "comFibraA.h"
#include "CerebroA.h"
#include <string.h>
#include <stdio.h> 

extern osMessageQueueId_t colaEventosCerebro;
extern char react_rx_trama[30]; 

typedef struct {
    uint8_t origen;         
    uint8_t tipo_comando;   
    uint8_t datos[6];       
} MensajeCerebro_t;

void Hilo_Orquestador_Cerebro(void *argument) {
    MensajeCerebro_t msgWeb;
    TramaFibra_t tramaTX;

    // Arrancamos el hardware en modo "Solo Transmisión"
    Init_ComFibraA(); 

    while(1) {
        // Escuchamos a la Web
        if (osMessageQueueGet(colaEventosCerebro, &msgWeb, NULL, 10U) == osOK) {
            
            if (msgWeb.tipo_comando == 1) {
                tramaTX.tipo_modo = DIR_A_TO_B | msgWeb.datos[0]; 
                tramaTX.payload1  = 0x00; 
                tramaTX.payload2  = 0x00;
                tramaTX.payload3  = 0x00;
                ComFibraA_EnviarTrama(&tramaTX); // Disparamos por el cable
                
                // Actualizamos la web para saber que ha funcionado
                sprintf(react_rx_trama, "[TX] Comando %d Enviado", msgWeb.datos[0]);
            }
            else if (msgWeb.tipo_comando == 2) {
                tramaTX.tipo_modo = DIR_A_TO_B | MODO_TELEMETRIA; 
                tramaTX.payload1  = 0xDE; 
                tramaTX.payload2  = 0xAD; 
                tramaTX.payload3  = 0xBE; 
                ComFibraA_EnviarTrama(&tramaTX); // Disparamos por el cable
                
                // Actualizamos la web para saber que ha funcionado
                sprintf(react_rx_trama, "[TX] Trama Manual Enviada");
            }
        }
        
        // Dejamos respirar al RTOS un poquito para asegurar el Ping
        osDelay(10); 
    }
}
