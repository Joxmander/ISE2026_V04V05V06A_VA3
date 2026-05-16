#include "CerebroB.h"
#include "sensorProximidad.h"
#include "rgb.h"
#include "spz.h"         // Añadido para poder leer los piezos
#include "cmsis_os2.h"
#include <stdio.h>       // Necesario para el printf

//// Truco mágico para que el printf salga por la ventana "Debug (printf) Viewer" de Keil
//int fputc(int ch, FILE *f) {
//    ITM_SendChar(ch);
//    return ch;
//}

void Hilo_Orquestador_CerebroB(void *argument) {
// --- 1. ABRIMOS LA PUERTA PARA EL LED 0 (Pin PB6) ---
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; // Función Alternativa para PWM
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF2_TIM4; // Conectamos el pin al Timer 4
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Init_pin_tim();  <-- La dejamos comentada
    Init_Th_rgb();          
    SensorProximidad_Init(); 
    Init_Th_piezo();

    // 2. Colores y variables de estado
    rgb_color_t color_on = {0, 40, 0};  // Verde con brillo bajo para no tumbar el USB
  //  rgb_color_t color_off = {0, 0, 0};  // Apagado
    
    uint8_t estado_led_0 = 0;
    uint8_t estado_led_1 = 0;

    printf("\r\n--- INICIANDO BANCO DE PRUEBAS NODO B ---\r\n");

    while (1) {
        // --- A. LECTURA DEL SENSOR DE PROXIMIDAD ---
        uint16_t distancia = ToF_GetDistancia();
        
        // --- B. LECTURA DE FUERZA CONTINUA (Para calibrar en el printf) ---
        FuerzaPiezos_t fuerzas;
        // Vaciamos la cola para quedarnos solo con la lectura más reciente
        while (osMessageQueueGet(id_cola_fuerza, &fuerzas, NULL, 0) == osOK) {}
        
        printf("Distancia: %4d mm | Tension -> Pad 0: %4d | Pad 1: %4d\r\n", 
                distancia, fuerzas.fuerza[0], fuerzas.fuerza[1]);

        // --- C. PROCESAMIENTO DE GOLPES (Eventos) ---
        EventoGolpe_t ev;
        // Leemos todos los golpes detectados
        while (osMessageQueueGet(id_cola_eventos, &ev, NULL, 0) == osOK) {
            
            printf("\r\n>>> GOLPE DETECTADO EN PAD %d (Fuerza: %d) <<<\r\n\r\n", ev.sensor_id, ev.fuerza);

            // Alternamos el LED correspondiente
            if (ev.sensor_id == 0) {
                estado_led_0 = !estado_led_0; // Cambia de 0 a 1 o de 1 a 0
                if (estado_led_0) rgb_encender_pad(0, color_on);
                else rgb_apagar_pad(0);
            } 
            else if (ev.sensor_id == 1) {
                estado_led_1 = !estado_led_1;
                if (estado_led_1) rgb_encender_pad(1, color_on);
                else rgb_apagar_pad(1);
            }
        }

        osDelay(100U); // Retardo de 100ms para que el printf se pueda leer con los ojos
    }
}

