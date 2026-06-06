#include "power.h"
#include "stm32f4xx_hal.h"

volatile uint8_t is_sleeping = 0; 

void Apagar_Perifericos_Temporalmente(void) {
    __HAL_RCC_ADC1_CLK_DISABLE();
    __HAL_RCC_TIM7_CLK_DISABLE();
    __HAL_RCC_SPI1_CLK_DISABLE();
}

void Restaurar_Perifericos_Temporalmente(void) {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_TIM7_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();
}

void Sistema_EntrarEnSleep(void) {
    is_sleeping = 1;

    // 1. Indicador visual (si tienes LED en la placa B)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);

    // 2. Apagamos periféricos temporales para bajar el consumo
    Apagar_Perifericos_Temporalmente();
    
    // 3. Suspendemos el Tick del RTOS. Si no lo hacemos, el RTOS
    // nos despertará cada 1 milisegundo arruinando el modo Sleep.
    HAL_SuspendTick();

    // 4. ENTRAMOS A DORMIR. 
    // El procesador se queda congelado en esta línea. 
    // Solo despertará cuando reciba una interrupción (Ej: Entra un byte por la UART).
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

    // 5. ¡HEMOS DESPERTADO! (Porque ha llegado un Ping u orden por Fibra)
    // Reanudamos el corazón del RTOS inmediatamente.
    HAL_ResumeTick();
        
    // 6. Restauramos los periféricos
    Restaurar_Perifericos_Temporalmente();

    // 7. Apagamos indicador visual
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
    
    is_sleeping = 0;
}
