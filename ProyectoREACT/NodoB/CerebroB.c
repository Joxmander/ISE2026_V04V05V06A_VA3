#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>

// Tus drivers locales
#include "Pads.h"
#include "LedsRGB.h"
#include "sensorProximidad.h"
#include "comFibraB.h"

// Variables para emular el estado del sistema
static uint8_t modo_juego_actual = 2; // Por defecto arrancamos en Modo 2 (Control Fuerza)
static uint32_t ultimo_tiempo_golpe = 0;

// Prototipos externos
extern osMessageQueueId_t colaFibraRX_B;
extern void SensorProximidad_Init(void);

// Funci?n para convertir valor ADC a Voltaje estimado (Referencia 3.3V, 12 bits = 4095)
static float CalcularVoltaje(uint16_t valor_adc) {
    return ((float)valor_adc * 3.3f) / 4095.0f;
}

void Hilo_CerebroB(void *argument) {
    printf("\r\n======================================\r\n");
    printf("[NODO B] Arrancando Banco de Pruebas...\r\n");
    printf("======================================\r\n");

    // 1. Inicializaci?n de Hardware
    Pads_Init();
    LedsRGB_Init();
    SensorProximidad_Init();
    FibraB_Init(); // El hilo de RX de fibra ya est? escuchando
	
		// Ejecuta la calibracion interactiva con el jugador
    Pads_CalibrarSensibilidad();

    printf("[NODO B] Hardware OK. Modo por defecto: 2 (Fuerza)\r\n");
    uint32_t tick_espera = osKernelGetTickCount();

    while (1) {
        // 1. PROCESAR COMANDOS DE FIBRA (Si Nodo A estuviera conectado)
        TramaFibra_t tramaRX;
        if (osMessageQueueGet(colaFibraRX_B, &tramaRX, NULL, 0) == osOK) {
            // Nodo A nos dice que cambiemos de juego (Comando 0x30)
            if (tramaRX.tipo_comando == 0x30) {
                modo_juego_actual = tramaRX.payload[0];
                printf("\n[FIBRA] -> Nodo A solicita Modo de Juego: %d\n", modo_juego_actual);
                
                // Confirmamos recepci?n
                FibraB_SendFrame(0x31, modo_juego_actual, 0, 0);
            }
            // Nodo A nos hace un Ping (Comando 0x10)
            else if (tramaRX.tipo_comando == 0x10) {
                // Respondemos al ping con telemetr?a b?sica (ej. ToF)
                uint16_t dist = ToF_GetDistancia();
                FibraB_SendFrame(0x11, (dist >> 8), (dist & 0xFF), 0);
            }
        }

        // 2. POLLING DE SENSORES Y AN?LISIS DE HARDWARE
        Pads_Poll();

        for (uint8_t i = 0; i < PADS_NUM_CANALES; i++) {
            if (Pads_HayGolpe(i)) {
                // Extraer datos del golpe
                uint8_t fuerza_pct = Pads_GetFuerza(i);
                uint16_t raw_adc   = Pads_GetRawDelta(i);
                float voltaje      = CalcularVoltaje(raw_adc);
                uint32_t ahora     = osKernelGetTickCount();
                uint32_t ms_desde_ultimo = ahora - ultimo_tiempo_golpe;
                ultimo_tiempo_golpe = ahora;

                // Destello visual de confirmaci?n
                LedsRGB_FillAnillo(i, 0, 255, 0); 
                LedsRGB_Show();
                osDelay(50); 
                LedsRGB_Clear();
                LedsRGB_Show();

                // 3. LOG DIAGN?STICO SEG?N MODO DE JUEGO
                printf("\n--- IMPACTO PAD %d ---\n", i);
                printf("Se?al ADC : %u cuentas (%.3f V)\n", raw_adc, voltaje);
                printf("Fuerza    : %d %%\n", fuerza_pct);

                switch (modo_juego_actual) {
                    case 1: // Memoria de Trabajo (Retenci?n Secuencial)
                        printf("[TEST M1] Pad %d detectado limpiamente. Listo para secuencias.\n", i);
                        break;
                    
case 2: // Control de Fuerza (Propiocepción)

    printf("[TEST M2] Objetivo: 50%%. Tu impacto: %d%%.\n", fuerza_pct);

    if (fuerza_pct <= 20) {
        printf("          -> ¡GOLPE MUY SUAVE!\n");
    } else if (fuerza_pct <= 45) {
        printf("          -> ¡EXCELENTE CONTROL!\n");
    } else {
        printf("          -> ¡TE HAS PASADO DE FUERZA!\n");
    }
    break;
                    
                    case 3: // Inhibici?n Motora (Tiempo de reacci?n)
                        printf("[TEST M3] Tiempo desde el ?ltimo golpe: %u ms\n", ms_desde_ultimo);
                        if (raw_adc > 2000) {
                            printf("          -> AVISO: El golpe fue muy violento (posible p?nico del jugador).\n");
                        }
                        break;
                    
                    case 4: // Ritmo Constante
                        {
                            uint32_t bpm_calculado = (ms_desde_ultimo > 0) ? (60000 / ms_desde_ultimo) : 0;
                            printf("[TEST M4] Cadencia actual: ~%u BPM (Intervalo: %u ms)\n", bpm_calculado, ms_desde_ultimo);
                        }
                        break;
                    
                    default:
                        printf("[TEST] Modo no reconocido.\n");
                        break;
                }
                
                // Simular env?o de evento al Nodo A 
                FibraB_SendFrame(0x50, i, fuerza_pct, 0);
            }
        }

        // Bucle estable a 100Hz (10ms)
        tick_espera += 10U;
        osDelayUntil(tick_espera);
    }
}
