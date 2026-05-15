/**
 ******************************************************************************
 * @file    sensorProximidad.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del módulo VL53L0X para el proyecto R.E.A.C.T.
 * * En este módulo he diseñado una máquina de estados no bloqueante para leer
 * el sensor de tiempo de vuelo (ToF) por I2C. He decidido prescindir de la 
 * pesada librería oficial de STMicroelectronics para ahorrar memoria y 
 * evitar cuelgues del RTOS. En su lugar, utilizo una inicialización bare-metal 
 * extraída por ingeniería inversa.
 * Además, he implementado un filtro digital IIR para limpiar el ruido óptico.
 ******************************************************************************
 */

#include "sensorProximidad.h"
#include <stdio.h>
#include "stm32f4xx.h"

// Dirección I2C y Registros clave del VL53L0X
#define VL53L0X_ADDR                  0x52  
#define REG_SYSRANGE_START            0x00
#define REG_RESULT_INTERRUPT_STATUS   0x13
#define REG_RESULT_RANGE_STATUS       0x14
#define REG_SYSTEM_INTERRUPT_CLEAR    0x0B

// Umbral de detección: Considero que la mano está en posición neutra si está a menos de 15 cm
#define UMBRAL_POSICION_NEUTRA_MM     200  

// Handle privado para el I2C de este módulo
I2C_HandleTypeDef hi2c1_tof;

// Hilo del sensor
osThreadId_t tid_SensorProximidad;
void Hilo_SensorProximidad(void *argument);

// Variables globales volátiles (se actualizan en este hilo, se leen desde CerebroB)
volatile uint16_t distancia_actual_mm = 8190; 
volatile uint8_t  mano_en_neutro = 0; // 1 = Mano detectada, 0 = Mano fuera

/**
 * @brief Getter thread-safe para obtener la última distancia leída.
 */
uint16_t ToF_GetDistancia(void) {
    return distancia_actual_mm;
}

/**
 * @brief Función para que el CerebroB sepa si el jugador ha vuelto a la posición de inicio.
 */
uint8_t ToF_ManoEnPosicionNeutra(void) {
    return mano_en_neutro;
}

/**
 * @brief Inicializa los pines, el periférico I2C1 y crea el hilo del sensor.
 */
void SensorProximidad_Init(void) {
    // 1. Enciendo los relojes de los periféricos que voy a usar
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    // 2. Configuro los pines PB8 (SCL) y PB9 (SDA) en modo Alterno Open Drain
    // Uso Pull-ups internos como medida extra de seguridad para el bus I2C
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;      
    GPIO_InitStruct.Pull = GPIO_PULLUP;          
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;   
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. Configuro el periférico I2C1 a 100kHz (Standard Mode)
    hi2c1_tof.Instance = I2C1;
    hi2c1_tof.Init.ClockSpeed = 100000;
    hi2c1_tof.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1_tof.Init.OwnAddress1 = 0;
    hi2c1_tof.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1_tof.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1_tof.Init.OwnAddress2 = 0;
    hi2c1_tof.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1_tof.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    
    if (HAL_I2C_Init(&hi2c1_tof) != HAL_OK) {
        printf("[ToF] Error de inicializacion hardware I2C1\r\n");
    }

    // 4. Creo el hilo que controlará la máquina de estados del sensor
    tid_SensorProximidad = osThreadNew(Hilo_SensorProximidad, NULL, NULL);
}

// Función auxiliar para escribir en I2C de forma limpia y mantener el código legible
void ToF_WriteReg(uint8_t reg, uint8_t valor) {
    HAL_I2C_Mem_Write(&hi2c1_tof, VL53L0X_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &valor, 1, 20);
}

/**
 * @brief Máquina de estados / Bucle del sensor de proximidad (Modo Alta Velocidad)
 */
void Hilo_SensorProximidad(void *argument) {
    uint8_t registro_id = 0;
    uint8_t status_reg;
    uint8_t buffer_distancia[2];
    HAL_StatusTypeDef status;

    // FASE 1: Compruebo la identidad (Registro 0xC0)
    status = HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, 0xC0, I2C_MEMADD_SIZE_8BIT, &registro_id, 1, 100);    
    if (status != HAL_OK || registro_id != 0xEE) {
        printf("[ToF] ERROR CRITICO: Sensor no responde.\r\n");
        while(1) { osDelay(1000U); } 
    }

    // --- FASE 1.5: SECUENCIA DE ARRANQUE "MÁGICA" MINIMALISTA ---
    ToF_WriteReg(0x0A, 0x04); 

    uint8_t stop_var = 0;
    ToF_WriteReg(0x80, 0x01);
    ToF_WriteReg(0xFF, 0x01);
    ToF_WriteReg(0x00, 0x00);
    HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, 0x91, I2C_MEMADD_SIZE_8BIT, &stop_var, 1, 20); 
    ToF_WriteReg(0x00, 0x01);
    ToF_WriteReg(0xFF, 0x00);
    ToF_WriteReg(0x80, 0x00);
    
    // --- NUEVO: CONFIGURACIÓN HIGH SPEED (Bajar Timing Budget a ~20ms) ---
    // Ajuste de los pre-range y final-range timeouts por hardware
    ToF_WriteReg(0x51, 0x00);
    ToF_WriteReg(0x52, 0x1A);
    ToF_WriteReg(0x71, 0x01);
    ToF_WriteReg(0x72, 0x14);
    // -------------------------------------------------------------

    // FASE 2: Bucle principal 
    while (1) {
        // 1. Mando comando de medición y la Stop Variable
        ToF_WriteReg(0x80, 0x01);
        ToF_WriteReg(0xFF, 0x01);
        ToF_WriteReg(0x00, 0x00);
        ToF_WriteReg(0x91, stop_var); 
        ToF_WriteReg(0x00, 0x01);
        ToF_WriteReg(0xFF, 0x00);
        ToF_WriteReg(0x80, 0x00);
        
        ToF_WriteReg(0x00, 0x01); // Orden de disparar el láser

        // 2. Polling no bloqueante 
        uint8_t timeout = 0;
        do {
            osDelay(2U); // REDUCIDO DE 5 a 2 ms
            HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, REG_RESULT_INTERRUPT_STATUS, I2C_MEMADD_SIZE_8BIT, &status_reg, 1, 20);
            timeout++;
        } while (((status_reg & 0x07) == 0) && (timeout < 20)); 

        // 3. Procesar dato listo
        if ((status_reg & 0x07) != 0) { 
            
            if (HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, REG_RESULT_RANGE_STATUS + 10, I2C_MEMADD_SIZE_8BIT, buffer_distancia, 2, 20) == HAL_OK) {
                
                uint16_t nueva_dist = (buffer_distancia[0] << 8) | buffer_distancia[1];
                static uint16_t dist_filtrada = 8190;
                
                if(nueva_dist > 20 && nueva_dist < 4000) { 
                    
                    if (dist_filtrada == 8190) {
                        dist_filtrada = nueva_dist;
                    } else {
                        // Al ir más rápido, damos más peso a la lectura nueva (50/50) 
                        // para que la respuesta sea más ágil
                        dist_filtrada = ((nueva_dist * 5) + (dist_filtrada * 5)) / 10;
                    }
                    
                    distancia_actual_mm = dist_filtrada;
                    
                    if (distancia_actual_mm < UMBRAL_POSICION_NEUTRA_MM) {
                        mano_en_neutro = 1;
                    } else {
                        mano_en_neutro = 0;
                    }
                }
            }

            ToF_WriteReg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
        }

        // REDUCIDO DE 40 a 2 ms: Máxima velocidad manteniendo el sistema vivo
        osDelay(2U); 
    }
}



