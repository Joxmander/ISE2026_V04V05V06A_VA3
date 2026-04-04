#include "cmsis_os2.h"
#include "comFibraA.h"
#include "CerebroA.h"
#include <string.h>
#include <stdio.h>

// Conectamos con el buzón y la variable de texto de la página Web
extern osMessageQueueId_t colaEventosCerebro;
extern char react_rx_trama[30]; 

// La misma estructura que usamos en el CGI
typedef struct {
    uint8_t origen;         
    uint8_t tipo_comando;   
    uint8_t datos[6];       
} MensajeCerebro_t;

void Hilo_Orquestador_Cerebro(void *argument) {
    MensajeCerebro_t msgWeb;
    TramaFibra_t tramaTX;
    TramaFibra_t tramaRX;

    // 1. Arrancamos el hardware de la fibra óptica
    Init_ComFibraA(); 

    while(1) {
        // 2. ¿HA PULSADO EL USUARIO UN BOTÓN EN LA WEB?
        // Miramos la cola de la web (esperamos máx 10ms para no bloquear)
        if (osMessageQueueGet(colaEventosCerebro, &msgWeb, NULL, 10U) == osOK) {
            
            if (msgWeb.tipo_comando == 1) {
                tramaTX.tipo_modo = DIR_A_TO_B | msgWeb.datos[0]; // Comando + Modo
                tramaTX.payload1  = 0x00; 
                tramaTX.payload2  = 0x00;
                tramaTX.payload3  = 0x00;
                ComFibraA_EnviarTrama(&tramaTX);
            }
            else if (msgWeb.tipo_comando == 2) {
                tramaTX.tipo_modo = DIR_A_TO_B | MODO_TELEMETRIA; 
                tramaTX.payload1  = 0xDE; // DE
                tramaTX.payload2  = 0xAD; // AD
                tramaTX.payload3  = 0xBE; // BE
                ComFibraA_EnviarTrama(&tramaTX);
            }
        }

        // 3. ¿HA ENTRADO UN MENSAJE FÍSICO POR EL CABLE RX?
        // Miramos la cola de la fibra óptica
        if (ComFibraA_RecibirTrama(&tramaRX, 10U) == 0) {
            // Escribimos los bytes recibidos directamente en la variable que lee la Web
            sprintf(react_rx_trama, "[RX] %02X %02X %02X %02X %02X %02X",
                    tramaRX.sof, tramaRX.tipo_modo, tramaRX.payload1,
                    tramaRX.payload2, tramaRX.payload3, tramaRX.checksum);
        }
				osDelay(50); 
    }
}
