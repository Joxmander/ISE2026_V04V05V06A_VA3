#include "eeprom.h"
#include "cmsis_os2.h"
#include <string.h>

static I2C_HandleTypeDef hi2c1;

/* AQUI se declara la memoria real para los 10 jugadores */
RecordJuego_t tabla_records[MAX_RECORDS]; 

#define EEPROM_BASE_ADDR        0xA0
#define EEPROM_WRITE_TIME_MS    6U      
#define I2C_TIMEOUT_MS          100U

void HAL_I2C_MspInit(I2C_HandleTypeDef* hi2c) {
    GPIO_InitTypeDef GPIO_InitStruct;
    if(hi2c->Instance == I2C1) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_I2C1_CLK_ENABLE();
        GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
        GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pull      = GPIO_PULLUP;
        GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
}

static uint8_t EEPROM_GetSlaveAddr(uint16_t mem_address) {
    uint8_t bank = (uint8_t)((mem_address >> 8) & 0x03);
    return (uint8_t)(EEPROM_BASE_ADDR | (bank << 1));
}

bool EEPROM_Init(void) {
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK) return false;
    if (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_BASE_ADDR, 3, I2C_TIMEOUT_MS) != HAL_OK) {
        return false;
    }
    return true;
}

bool EEPROM_WriteBytes(uint16_t direccion, const uint8_t *datos, uint16_t tamano) {
    uint16_t bytes_restantes = tamano;
    uint16_t dir_actual = direccion;
    const uint8_t *p_datos = datos;
    uint16_t chunk;

    if ((direccion + tamano) > EEPROM_TAMANO) return false;

    while (bytes_restantes > 0) {
        uint16_t espacio_pagina = EEPROM_PAGE_SIZE - (dir_actual % EEPROM_PAGE_SIZE);
        uint16_t espacio_banco  = EEPROM_BANK_SIZE - (dir_actual % EEPROM_BANK_SIZE);
        
        chunk = bytes_restantes;
        if (chunk > espacio_pagina) chunk = espacio_pagina;
        if (chunk > espacio_banco)  chunk = espacio_banco;

        if (HAL_I2C_Mem_Write(&hi2c1, EEPROM_GetSlaveAddr(dir_actual), (uint16_t)(dir_actual & 0xFF), 
                              I2C_MEMADD_SIZE_8BIT, (uint8_t *)p_datos, chunk, I2C_TIMEOUT_MS) != HAL_OK) {
            return false;
        }
        osDelay(EEPROM_WRITE_TIME_MS);
        dir_actual += chunk;
        p_datos += chunk;
        bytes_restantes -= chunk;
    }
    return true;
}

bool EEPROM_ReadBytes(uint16_t direccion, uint8_t *destino, uint16_t tamano) {
    uint16_t bytes_restantes = tamano;
    uint16_t dir_actual = direccion;
    uint8_t *p_destino = destino;
    uint16_t chunk;

    if ((direccion + tamano) > EEPROM_TAMANO) return false;

    while (bytes_restantes > 0) {
        uint16_t espacio_banco = EEPROM_BANK_SIZE - (dir_actual % EEPROM_BANK_SIZE);
        chunk = bytes_restantes;
        if (chunk > espacio_banco) chunk = espacio_banco;

        if (HAL_I2C_Mem_Read(&hi2c1, EEPROM_GetSlaveAddr(dir_actual), (uint16_t)(dir_actual & 0xFF), 
                             I2C_MEMADD_SIZE_8BIT, p_destino, chunk, I2C_TIMEOUT_MS) != HAL_OK) {
            return false;
        }
        dir_actual += chunk;
        p_destino += chunk;
        bytes_restantes -= chunk;
    }
    return true;
}

bool EEPROM_WriteByte(uint16_t direccion, uint8_t dato) {
    return EEPROM_WriteBytes(direccion, &dato, 1);
}

bool EEPROM_ReadByte(uint16_t direccion, uint8_t *dato) {
    if (dato == NULL) return false;
    return EEPROM_ReadBytes(direccion, dato, 1);
}

bool EEPROM_GuardarConfiguracion(ConfigSistema_t *config) {
    if (!EEPROM_WriteBytes(ADDR_CONFIG_SNTP, (const uint8_t *)config, sizeof(ConfigSistema_t))) return false;
    uint8_t magic = EEPROM_INIT_MAGIC;
    return EEPROM_WriteBytes(ADDR_EEPROM_INIT_FLAG, &magic, 1);
}

bool EEPROM_CargarConfiguracion(ConfigSistema_t *config) {
    uint8_t magic = 0;
    if (!EEPROM_ReadBytes(ADDR_EEPROM_INIT_FLAG, &magic, 1) || magic != EEPROM_INIT_MAGIC) return false;
    return EEPROM_ReadBytes(ADDR_CONFIG_SNTP, (uint8_t *)config, sizeof(ConfigSistema_t));
}

bool EEPROM_GuardarRecords(RecordJuego_t *tabla) {
    return EEPROM_WriteBytes(ADDR_RECORD_NOMBRE, (const uint8_t *)tabla, sizeof(RecordJuego_t) * MAX_RECORDS);
}

bool EEPROM_CargarRecords(RecordJuego_t *tabla) {
    return EEPROM_ReadBytes(ADDR_RECORD_NOMBRE, (uint8_t *)tabla, sizeof(RecordJuego_t) * MAX_RECORDS);
}
