#include "CerebroB.h"
#include "comFibraB.h"
#include "power.h"
#include "monitorConsumo.h"
#include "fsm_modos.h"
#include "spz.h"
#include "rgb.h"
#include "buzzer.h"
#include "cmsis_os2.h"

// Importamos la cola donde el hilo de los piezos/botones deja los golpes
extern osMessageQueueId_t id_cola_eventos;

void Hilo_Orquestador_CerebroB(void *argument) {
    TramaFibra_t mensajeRX;
    EventoGolpe_t nuevo_golpe;

    // 1. INICIALIZAMOS TODO EL SISTEMA FÍSICO Y LÓGICO
    FibraB_Init();
    Monitor_Init(); 
    Init_Th_piezo();  // Botones simulando piezos
    Init_Th_rgb();    // LEDs rojos simulando anillos
    Buzzer_Init();    // El zumbador
    FSM_Init();       // La máquina de estados de tu compañero

    uint32_t tick_anterior = osKernelGetTickCount();

    while (1) {
        // 2. ESCUCHAR COMANDOS DEL NODO A (Fibra Óptica / UART)
        // No bloqueamos indefinidamente, esperamos máximo 1 tick para no frenar la FSM
        if (osMessageQueueGet(colaFibraRX_B, &mensajeRX, NULL, 0) == osOK) {
            
            if (mensajeRX.tipo_comando == CMD_PING) { // 0x10 PING
                // El Nodo A pregunta si estamos vivos, mandamos el consumo
                uint16_t mA = Monitor_GetConsumo_mA();
                FibraB_SendFrame(0x20, 0x00, (mA >> 8), (mA & 0xFF));
            }
            else if (mensajeRX.tipo_comando == CMD_GOTO_LOW_POWER) { // 0x40 SLEEP
                Monitor_SetEstado(MODO_PWR_SLEEP);
                FSM_ProcesarEvento(EV_RX_COMANDO, &mensajeRX, NULL);
                osDelay(10); 
                Sistema_EntrarEnSleep(); // El micro se duerme aquí
                Monitor_SetEstado(MODO_PWR_IDLE); // Al despertar, vuelve a Idle
            }
            else { 
                // Cualquier otro comando (START_JUEGO 0x30, etc) se lo pasamos a la máquina de estados
                FSM_ProcesarEvento(EV_RX_COMANDO, &mensajeRX, NULL);
            }
        }

        // 3. ESCUCHAR LOS GOLPES (Piezos/Botones)
        // Si alguien pulsó un botón, sacamos el evento de la cola y se lo damos a la FSM
        if (osMessageQueueGet(id_cola_eventos, &nuevo_golpe, NULL, 0) == osOK) {
            FSM_ProcesarEvento(EV_SENSOR_HIT, NULL, (void*)&nuevo_golpe);
        }

        // 4. ALIMENTAR EL RELOJ DE LA FSM (1 milisegundo)
        uint32_t tick_actual = osKernelGetTickCount();
        if (tick_actual - tick_anterior >= 1U) {
            FSM_ProcesarEvento(EV_TICK_MS, NULL, NULL);
            tick_anterior = tick_actual;
        }

        // Pequeño respiro al RTOS
        osThreadYield(); 
    }
}
