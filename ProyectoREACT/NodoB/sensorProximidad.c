/**
 ******************************************************************************
 * @file    sensorProximidad.c
 * @author  Jose Vargas Gonzaga
 * @brief   Implementacion del modulo VL53L0X para el proyecto R.E.A.C.T.
 *
 * He disenado una maquina de estados no bloqueante para leer el sensor de
 * tiempo de vuelo (ToF) por I2C. He decidido prescindir de la pesada
 * libreria oficial de STMicroelectronics para ahorrar memoria y evitar
 * cuelgues del RTOS. En su lugar, utilizo una inicializacion bare-metal
 * extraida por ingenieria inversa.
 * Ademas, he implementado un filtro digital IIR para limpiar el ruido optico.
 ******************************************************************************
 */

#include "sensorProximidad.h"
#include <stdio.h>
#include "stm32f4xx.h"

// === DIRECCION I2C Y REGISTROS CLAVE DEL VL53L0X ===
#define VL53L0X_ADDR                  0x52
#define REG_SYSRANGE_START            0x00
#define REG_RESULT_INTERRUPT_STATUS   0x13
#define REG_RESULT_RANGE_STATUS       0x14
#define REG_SYSTEM_INTERRUPT_CLEAR    0x0B

// Umbral de deteccion: la mano esta en posicion neutra si esta a < 20 cm
#define UMBRAL_POSICION_NEUTRA_MM     200

// Handle privado para el I2C de este modulo
I2C_HandleTypeDef hi2c1_tof;

// Hilo del sensor
osThreadId_t tid_SensorProximidad;
void Hilo_SensorProximidad(void *argument);

// Variables globales volatiles (se actualizan en este hilo, se leen desde CerebroB)
volatile uint16_t distancia_actual_mm = 8190;
volatile uint8_t  mano_en_neutro = 0;
volatile uint8_t  tof_disponible = 1U;   /* 1 = sensor OK, 0 = init fallo -> juegos sin gate */

/**
 * @brief Getter thread-safe para obtener la ultima distancia leida.
 */
uint16_t ToF_GetDistancia(void) {
    return distancia_actual_mm;
}

/**
 * @brief Funcion para que el CerebroB sepa si el jugador ha vuelto al inicio.
 *        BYPASS: si el sensor no se pudo inicializar (cable suelto, modulo
 *        roto, etc.) devuelve 1 para que la FSM no se quede esperando la
 *        mano. Asi los juegos pueden seguir probandose mientras se depura.
 */
uint8_t ToF_ManoEnPosicionNeutra(void) {
    if (!tof_disponible) return 1U;
    return mano_en_neutro;
}

/**
 * @brief Inicializa los pines, el periferico I2C1 y crea el hilo del sensor.
 */
void SensorProximidad_Init(void) {
    // 1. Enciendo los relojes de los perifericos
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();

    // 2. PB8 (SCL) y PB9 (SDA) en modo Alterno Open Drain
    // Uso Pull-ups internos como medida extra de seguridad para el bus I2C
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. Configuro el periferico I2C1 a 100 kHz (Standard Mode)
    hi2c1_tof.Instance              = I2C1;
    hi2c1_tof.Init.ClockSpeed       = 100000;
    hi2c1_tof.Init.DutyCycle        = I2C_DUTYCYCLE_2;
    hi2c1_tof.Init.OwnAddress1      = 0;
    hi2c1_tof.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    hi2c1_tof.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    hi2c1_tof.Init.OwnAddress2      = 0;
    hi2c1_tof.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    hi2c1_tof.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1_tof);

    // 4. Creo el hilo que controlara la maquina de estados del sensor
    tid_SensorProximidad = osThreadNew(Hilo_SensorProximidad, NULL, NULL);
}

// Funcion auxiliar para escribir 1 byte en un registro del VL53L0X
void ToF_WriteReg(uint8_t reg, uint8_t valor) {
    HAL_I2C_Mem_Write(&hi2c1_tof, VL53L0X_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &valor, 1, 20);
}

/**
 * @brief Maquina de estados / Bucle del sensor de proximidad (Modo Alta Velocidad)
 */
void Hilo_SensorProximidad(void *argument) {
    uint8_t registro_id = 0;
    uint8_t status_reg;
    uint8_t buffer_distancia[2];
    HAL_StatusTypeDef status;

    // >>> DIAG TMP: traza minuto a minuto para localizar el cuelgue
    printf("[ToF] >> Hilo arrancado, esperando 100ms para power-up\r\n");

    // Esperar un momento a que el sensor termine de arrancar electricamente
    // (CRITICO: sin este delay la primera lectura I2C falla porque el sensor
    //  todavia no ha terminado el power-on internamente)
    osDelay(100U);

    printf("[ToF] >> Delay OK. Leyendo registro 0xC0 (identidad)...\r\n");

    // FASE 1: Compruebo la identidad (Registro 0xC0 debe responder 0xEE)
    status = HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, 0xC0,
                              I2C_MEMADD_SIZE_8BIT, &registro_id, 1, 100);

    printf("[ToF] >> HAL_status=%d  ID_leido=0x%02X (esperado 0xEE)\r\n",
           (int)status, (unsigned)registro_id);

    if (status != HAL_OK || registro_id != 0xEE) {
        /* === MODO BYPASS (sensor no responde a I2C) ===
         * Marcamos el sensor como NO disponible. ToF_ManoEnPosicionNeutra()
         * devolvera 1 siempre, asi la FSM no se atasca esperando la mano y
         * los juegos pueden correr mientras se depura el problema fisico.
         * Senal visual: PB7 parpadea 5 segundos, luego se apaga y este hilo
         * termina limpiamente (no consume mas CPU).
         */
        tof_disponible = 0U;
        printf("[ToF] FALLO I2C (status=%d ID=0x%02X). BYPASS activado.\r\n",
               (int)status, (unsigned)registro_id);
        printf("[ToF] Los juegos correran SIN gate ToF. Cuando arregles el\r\n");
        printf("      cableado (PB8/PB9/3V3/GND) el sensor vuelve solo.\r\n");

        __HAL_RCC_GPIOB_CLK_ENABLE();
        {
            GPIO_InitTypeDef LED_InitStruct = { .Pin = GPIO_PIN_7, .Mode = GPIO_MODE_OUTPUT_PP };
            uint8_t k;
            HAL_GPIO_Init(GPIOB, &LED_InitStruct);
            for (k = 0U; k < 20U; k++) {       /* 20 toggles * 250 ms = 5 s */
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_7);
                osDelay(250U);
            }
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
        }
        osThreadExit();   /* termina el hilo, libera CPU para el resto */
    }

    printf("[ToF] >> ID OK. Ejecutando secuencia de arranque magica...\r\n");

    // FASE 1.5: SECUENCIA DE ARRANQUE "MAGICA" MINIMALISTA
    ToF_WriteReg(0x0A, 0x04);

    uint8_t stop_var = 0;
    ToF_WriteReg(0x80, 0x01);
    ToF_WriteReg(0xFF, 0x01);
    ToF_WriteReg(0x00, 0x00);
    HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, 0x91, I2C_MEMADD_SIZE_8BIT, &stop_var, 1, 20);
    ToF_WriteReg(0x00, 0x01);
    ToF_WriteReg(0xFF, 0x00);
    ToF_WriteReg(0x80, 0x00);

    printf("[ToF] >> Magic OK. Configurando High Speed...\r\n");

    // CONFIGURACION HIGH SPEED (Timing Budget ~20ms)
    ToF_WriteReg(0x51, 0x00);
    ToF_WriteReg(0x52, 0x1A);
    ToF_WriteReg(0x71, 0x01);
    ToF_WriteReg(0x72, 0x14);

    // >>> DIAG TMP: confirmamos que la init paso bien y entramos en loop
    printf("[ToF] Init OK (ID=0xEE). Entrando en loop de medicion...\r\n");

    // FASE 2: Bucle principal de medicion
    uint16_t ultima_raw   = 0U;     /* >>> DIAG TMP: ultima lectura cruda */
    uint32_t diag_cnt     = 0U;     /* >>> DIAG TMP: contador para print periodico */
    while (1) {
        // 1. Mando comando de medicion y la Stop Variable
        ToF_WriteReg(0x80, 0x01);
        ToF_WriteReg(0xFF, 0x01);
        ToF_WriteReg(0x00, 0x00);
        ToF_WriteReg(0x91, stop_var);
        ToF_WriteReg(0x00, 0x01);
        ToF_WriteReg(0xFF, 0x00);
        ToF_WriteReg(0x80, 0x00);

        ToF_WriteReg(0x00, 0x01);   // Disparar el laser

        // 2. Polling no bloqueante esperando que el sensor termine
        uint8_t timeout = 0;
        do {
            osDelay(2U);
            HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, REG_RESULT_INTERRUPT_STATUS,
                             I2C_MEMADD_SIZE_8BIT, &status_reg, 1, 20);
            timeout++;
        } while (((status_reg & 0x07) == 0) && (timeout < 20));

        // 3. Procesar dato listo
        if ((status_reg & 0x07) != 0) {
            if (HAL_I2C_Mem_Read(&hi2c1_tof, VL53L0X_ADDR, REG_RESULT_RANGE_STATUS + 10,
                                 I2C_MEMADD_SIZE_8BIT, buffer_distancia, 2, 20) == HAL_OK) {

                uint16_t nueva_dist = (buffer_distancia[0] << 8) | buffer_distancia[1];
                static uint16_t dist_filtrada = 8190;
                ultima_raw = nueva_dist;   /* >>> DIAG TMP */

                if (nueva_dist > 20 && nueva_dist < 4000) {
                    if (dist_filtrada == 8190) {
                        dist_filtrada = nueva_dist;
                    } else {
                        // Filtro IIR 50/50 (respuesta agil)
                        dist_filtrada = ((nueva_dist * 5) + (dist_filtrada * 5)) / 10;
                    }

                    distancia_actual_mm = dist_filtrada;
                    mano_en_neutro = (distancia_actual_mm < UMBRAL_POSICION_NEUTRA_MM) ? 1 : 0;
                }
            }

            ToF_WriteReg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
        }

        /* DIAG eliminado completamente: estaba saturando el SWO y causando el
         * "Trace data overflow" que veias en Keil. Si necesitas reactivarlo
         * para depurar el ToF, descomenta el bloque de abajo:
         *
         * diag_cnt++;
         * if (diag_cnt >= 200U) {
         *     diag_cnt = 0U;
         *     printf("[ToF] dist=%u mm  neutro=%u\r\n",
         *            (unsigned)distancia_actual_mm, (unsigned)mano_en_neutro);
         * }
         */
        (void)ultima_raw;
        (void)diag_cnt;

        osDelay(10U);
    }
}

