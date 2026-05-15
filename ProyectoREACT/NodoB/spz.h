#ifndef SENS_PIEZO
#define SENS_PIEZO

#include "cmsis_os2.h"                          // CMSIS RTOS header file
#include "stm32f4xx_hal.h"
//#include "stm32f4xx_hal_adc.h"


typedef struct {
    uint16_t fuerza[4];  // fuerza filtrada de cada piezo
} FuerzaPiezos_t;


typedef struct {
    uint8_t modo;          // Modo actual (memoria, ritmo, velocidad, inhibici?n?)
    uint8_t nivel;             // Nivel actual del juego
    uint8_t secuencia[32];     // Secuencia generada (modo memoria)
    uint8_t longitud;          // Longitud de la secuencia
    uint8_t indice_actual;     // ?ndice que debe pulsar el jugador
    uint8_t esperando_input;   // 1 = aceptar golpes, 0 = ignorar
    uint8_t proximidad_activa; // 1 = mano retirada, 0 = mano presente
} Juego_t;

//Evento generado por los piezos
//Cada vez que se detecta un golpe, generara un evento
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

extern   Juego_t juego;



#endif
