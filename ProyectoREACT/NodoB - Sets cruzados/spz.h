#ifndef SENS_PIEZO
#define SENS_PIEZO

#include "cmsis_os2.h"                          
#include "stm32f4xx_hal.h"

typedef struct {
    uint16_t fuerza[4];  // fuerza filtrada de cada piezo
} FuerzaPiezos_t;

typedef struct {
    uint8_t modo;          
    uint8_t nivel;             
    uint8_t secuencia[32];     
    uint8_t longitud;          
    uint8_t indice_actual;     
    uint8_t esperando_input;   
    uint8_t proximidad_activa; 
} Juego_t;

typedef struct{
  uint8_t sensor_id;
  uint16_t fuerza;
} EventoGolpe_t;

typedef enum {
    ESTADO_ESPERANDO_MANO,
    ESTADO_LISTO,
    ESTADO_MOSTRANDO_SECUENCIA,
    ESTADO_ESPERANDO_INPUT,
    ESTADO_FALLO,
    ESTADO_EXITO
} EstadoJuego_t;

int Init_Th_piezo (void);
int Init_Th_test_spz(void);

extern Juego_t juego;

// --- AÑADIDO PARA QUE EL CEREBRO B VEA LAS COLAS ---
extern osMessageQueueId_t id_cola_eventos;
extern osMessageQueueId_t id_cola_fuerza;
extern uint16_t piezo_raw[4];
extern uint16_t piezo_filtered[4];

#endif
