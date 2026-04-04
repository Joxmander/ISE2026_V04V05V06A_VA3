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
    // TramaFibra_t tramaRX; 

    Init_ComFibraA(); 

    while(1) {
        if (osMessageQueueGet(colaEventosCerebro, &msgWeb, NULL, 10U) == osOK) {
            
            if (msgWeb.tipo_comando == 1) {
                tramaTX.tipo_modo = DIR_A_TO_B | msgWeb.datos[0]; 
                tramaTX.payload1  = 0x00; 
                tramaTX.payload2  = 0x00;
                tramaTX.payload3  = 0x00;
                ComFibraA_EnviarTrama(&tramaTX); 
                sprintf(react_rx_trama, "[TX] Comando %d -> Nodo B", msgWeb.datos[0]);
            }
            else if (msgWeb.tipo_comando == 2) {
                tramaTX.tipo_modo = DIR_A_TO_B | MODO_TELEMETRIA; 
                tramaTX.payload1  = 0xDE; 
                tramaTX.payload2  = 0xAD; 
                tramaTX.payload3  = 0xBE; 
                ComFibraA_EnviarTrama(&tramaTX); 
                sprintf(react_rx_trama, "[TX] Trama Hexadecimal Enviada");
            }
        }
        
        // APAGADO HASTA FASE 3 (Conexión real con Nodo B)
        /*
        if (ComFibraA_RecibirTrama(&tramaRX, 10U) == 0) {
            sprintf(react_rx_trama, "[RX] %02X %02X %02X %02X %02X %02X",
                    tramaRX.sof, tramaRX.tipo_modo, tramaRX.payload1,
                    tramaRX.payload2, tramaRX.payload3, tramaRX.checksum);
        }
        */
        
        osDelay(10); 
    }
}