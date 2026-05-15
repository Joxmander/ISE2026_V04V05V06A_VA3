#ifndef __EEPROM_H
#define __EEPROM_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define EEPROM_TAMANO           1024    
#define EEPROM_BANK_SIZE        256     
#define EEPROM_PAGE_SIZE        16      
#define EEPROM_INIT_MAGIC       0x42    

#define ADDR_EEPROM_INIT_FLAG   0x0000
#define ADDR_CONFIG_SNTP        0x0001
#define ADDR_RECORD_NOMBRE      0x0050

#define MAX_RECORDS 10  /* Limite del Histórico */

typedef struct {
    uint8_t  sntp_index;
    uint8_t  alarma_habilitada;
    uint8_t  ultimo_modo_juego;
} ConfigSistema_t;

typedef struct {
    char     nombre_jugador[16];
    uint32_t puntuacion;
    uint8_t  nivel;
    char     fecha_hora[20]; 
} RecordJuego_t;

/* Variable global compartida (evita el error L6200E) */
extern RecordJuego_t tabla_records[MAX_RECORDS];

bool EEPROM_Init(void);
bool EEPROM_WriteBytes(uint16_t direccion, const uint8_t *datos, uint16_t tamano);
bool EEPROM_ReadBytes(uint16_t direccion, uint8_t *destino, uint16_t tamano);
bool EEPROM_WriteByte(uint16_t direccion, uint8_t dato);
bool EEPROM_ReadByte(uint16_t direccion, uint8_t *dato);

bool EEPROM_GuardarConfiguracion(ConfigSistema_t *config);
bool EEPROM_CargarConfiguracion(ConfigSistema_t *config);

/* Nuevas firmas para guardar la tabla entera */
bool EEPROM_GuardarRecords(RecordJuego_t *tabla);
bool EEPROM_CargarRecords(RecordJuego_t *tabla);

#endif /* __EEPROM_H */
