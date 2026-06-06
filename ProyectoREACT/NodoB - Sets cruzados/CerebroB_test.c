#include "Pads.h"
#include "LedsRGB.h"
#include "comFibraB.h"
#include "fsm_modos.h"  
#include "buzzer.h" // Asegúrate de tener esta cabecera
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

// Umbral ajustado: El Pad 2 tiene mucho ruido (650), subimos un poco el margen
#define UMBRAL_SEGURO_GOLPE  800 

extern void     SensorProximidad_Init(void);
extern uint16_t ToF_GetDistancia(void);
extern osMessageQueueId_t colaFibraRX_B;

void Hilo_CerebroB(void *argument) {
    printf("\r\n[SISTEMA] Arrancando Nodo B...\r\n");
    
    Pads_Init();
    LedsRGB_Init();
    SensorProximidad_Init();
    FibraB_Init(); 
    Buzzer_Init();  // <--- IMPORTANTE: Si no, no suena nada
    FSM_Init();

    printf("[SISTEMA] Perifericos OK. Umbral Golpe: %d\r\n", UMBRAL_SEGURO_GOLPE);

    uint32_t tick = osKernelGetTickCount();
    uint32_t t_telemetria = 0;

    while (1) {
        osDelayUntil(tick += 1); 

        FSM_ProcesarEvento(EV_TICK_MS, NULL, NULL);

        // 1. COMANDOS FIBRA
        TramaFibra_t tramaRX;
        if (osMessageQueueGet(colaFibraRX_B, &tramaRX, NULL, 0) == osOK) {
            printf("[FIBRA] Comando Recibido: 0x%02X | Payload: %d\n", tramaRX.tipo_comando, tramaRX.payload[0]);
            FSM_ProcesarEvento(EV_RX_COMANDO, &tramaRX, NULL);
        }

        // 2. SENSORES PADS (Con log de proximidad al umbral)
        Pads_Poll();
        for (uint8_t i = 0; i < 4; i++) {
            if (Pads_HayGolpe(i)) {
                uint16_t val = Pads_RawValue(i);
                if (val > UMBRAL_SEGURO_GOLPE) {
                    printf("[PAD] GOLPE DETECTADO en PAD %d (Fuerza: %u)\n", i, val);
                    EventoGolpe_t g = { .sensor_id = i, .fuerza = val };
                    FSM_ProcesarEvento(EV_SENSOR_HIT, NULL, &g);
                } else if (val > 500) {
                    // Log silencioso para debug de pads flojos
                    // printf("[DEBUG] Intento en Pad %d con fuerza %u (Umbral es %d)\n", i, val, UMBRAL_SEGURO_GOLPE);
                }
            }
        }

        // 3. TELEMETRÍA (Cada 2 segundos para no saturar)
        if (++t_telemetria >= 2000) {
            t_telemetria = 0;
            FSM_State_t est = FSM_GetEstadoActual();
            if (est == STATE_IDLE) {
                printf("[B-IDLE] Esperando juego... ToF: %d mm\n", ToF_GetDistancia());
            }
        }
    }
}
