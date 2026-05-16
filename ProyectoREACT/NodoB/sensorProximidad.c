/**
 ******************************************************************************
 * @file    sensorProximidad.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementación del módulo VL53L0X para el proyecto R.E.A.C.T.
 ******************************************************************************
 */

#include "sensorProximidad.h"
#include <stdio.h>
#include "stm32f4xx.h"

#define VL53L0X_ADDR                  0x52  
#define REG_SYSRANGE_START            0x00
#define REG_RESULT_INTERRUPT_STATUS   0x13
#define REG_RESULT_RANGE_STATUS       0x14
#define REG_SYSTEM_INTERRUPT_CLEAR    0x0B

#define UMBRAL_POSICION_NEUTRA_MM     200  

I2C_HandleTypeDef hi2c1_tof;
osThreadId_t tid_SensorProximidad;
void Hilo_SensorProximidad(void *argument);

volatile uint16_t distancia_actual_mm = 8190; 
volatile uint8_t  mano_en_neutro = 0; 

uint16_t ToF_GetDistancia(void) {
    return distancia_actual_mm;
}

uint8_t ToF_ManoEnPosicionNeutra(void) {
    return mano_en_neutro;
}

void SensorProximidad_Init(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;      
    GPIO_InitStruct.Pull = GPIO_PULLUP;          
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;   
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    hi2c1_tof.Instance = I2C1;
    hi2c1_tof.Init.ClockSpeed = 100000;
    hi2c1_tof.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1_tof.Init.OwnAddress1 = 0;
    hi2c1_tof.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1_tof.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1_tof.Init.OwnAddress2 = 0;
    hi2c1_tof.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1_tof.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    
    HAL_I2C_Init(&hi2c1_tof);

    tid_SensorProximidad = osThreadNew(Hilo_SensorProximidad, NULL, NULL);
}

void ToF_WriteReg(uint8_t reg, uint8_t valor) {
    HAL_I2C_Mem_Write(&hi2c1_tof, VL53L0X_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &valor, 1, 20);
}

void Hilo_SensorProximidad(void *argument) {
    uint8_t registro_id = 0;
    uint8_t status_reg;
    uint8_t buffer_distancia[2];
    HAL_StatusTypeDef status;

    // Esperar un momento a que el sensor termine de arrancar eléctricamente
    osDelay(100U);

    status = HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, 0xC0, I2C_MEMADD_SIZE_8BIT, &registro_id, 1, 100);    
    if (status != HAL_OK || registro_id != 0xEE) {
        // Si entra aquí, parpadeamos el LED azul de la placa nucleo para avisar de fallo I2C
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitTypeDef LED_InitStruct = { .Pin = GPIO_PIN_7, .Mode = GPIO_MODE_OUTPUT_PP };
        HAL_GPIO_Init(GPIOB, &LED_InitStruct);
        while(1) { 
            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_7);
            osDelay(250U); 
        } 
    }

    ToF_WriteReg(0x0A, 0x04); 
    uint8_t stop_var = 0;
    ToF_WriteReg(0x80, 0x01);
    ToF_WriteReg(0xFF, 0x01);
    ToF_WriteReg(0x00, 0x00);
    HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, 0x91, I2C_MEMADD_SIZE_8BIT, &stop_var, 1, 20); 
    ToF_WriteReg(0x00, 0x01);
    ToF_WriteReg(0xFF, 0x00);
    ToF_WriteReg(0x80, 0x00);
    
    ToF_WriteReg(0x51, 0x00);
    ToF_WriteReg(0x52, 0x1A);
    ToF_WriteReg(0x71, 0x01);
    ToF_WriteReg(0x72, 0x14);

    while (1) {
        ToF_WriteReg(0x80, 0x01);
        ToF_WriteReg(0xFF, 0x01);
        ToF_WriteReg(0x00, 0x00);
        ToF_WriteReg(0x91, stop_var); 
        ToF_WriteReg(0x00, 0x01);
        ToF_WriteReg(0xFF, 0x00);
        ToF_WriteReg(0x80, 0x00);
        ToF_WriteReg(0x00, 0x01);

        uint8_t timeout = 0;
        do {
            osDelay(2U); 
            HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, REG_RESULT_INTERRUPT_STATUS, I2C_MEMADD_SIZE_8BIT, &status_reg, 1, 20);
            timeout++;
        } while (((status_reg & 0x07) == 0) && (timeout < 20)); 

        if ((status_reg & 0x07) != 0) { 
            if (HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, REG_RESULT_RANGE_STATUS + 10, I2C_MEMADD_SIZE_8BIT, buffer_distancia, 2, 20) == HAL_OK) {
                uint16_t nueva_dist = (buffer_distancia[0] << 8) | buffer_distancia[1];
                static uint16_t dist_filtrada = 8190;
                
                if(nueva_dist > 20 && nueva_dist < 4000) { 
                    if (dist_filtrada == 8190) dist_filtrada = nueva_dist;
                    else dist_filtrada = ((nueva_dist * 5) + (dist_filtrada * 5)) / 10;
                    
                    distancia_actual_mm = dist_filtrada;
                    mano_en_neutro = (distancia_actual_mm < UMBRAL_POSICION_NEUTRA_MM) ? 1 : 0;
                }
            }
            ToF_WriteReg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
        }
        osDelay(10U); 
    }
}
