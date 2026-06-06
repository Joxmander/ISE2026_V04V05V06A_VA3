/**
 ******************************************************************************
 * @file    buzzer.c
 * @author  Equipo R.E.A.C.T.
 * @brief   Driver del buzzer pasivo (TIM3 CH1 / PA6).
 *
 * @details Mecánica:
 *          - TIM3 cuenta a 1 MHz (prescaler = 84-1 sobre clock 84 MHz).
 *          - Para emitir frecuencia F Hz: ARR = (1_000_000 / F) - 1.
 *          - Duty 50%: CCR1 = (ARR+1) / 2.
 *          - Un osTimer one-shot apaga el PWM tras la duración pedida.
 *
 *          Concurrencia: si se llama Buzzer_Tono() mientras suena un tono
 *          previo, se cancela el timer pendiente y se reprograma con la
 *          nueva duración. El usuario percibe esto como "el nuevo tono
 *          sustituye al anterior".
 ******************************************************************************
 */

#include "buzzer.h"
#include "cmsis_os2.h"
#include <stdbool.h>

TIM_HandleTypeDef htim3;

#define BUZZER_TIM_CLOCK_HZ   1000000U   /* Frecuencia del contador TIM3   */
#define BUZZER_FREQ_MIN_HZ    100U
#define BUZZER_FREQ_MAX_HZ    5000U
#define BUZZER_DUR_MIN_MS     10U
#define BUZZER_DUR_MAX_MS     5000U

static osTimerId_t tim_apagar = NULL;
static volatile bool tono_activo = false;

/* Callback del timer one-shot: apaga el PWM cuando expira la duración */
static void ApagarBuzzerCallback(void *arg) {
    (void)arg;
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    tono_activo = false;
}

int Buzzer_Init(void) {
    /* La configuración de TIM3 (instancia, prescaler base, channel) se
     * hace por CubeMX. Aquí solo:
     *   1) Forzamos prescaler a 84-1 para tener 1 MHz de cuenta.
     *   2) Creamos el timer RTOS de apagado.
     *   3) Dejamos el PWM parado.
     */
    __HAL_TIM_SET_PRESCALER(&htim3, 84 - 1);

    tim_apagar = osTimerNew(ApagarBuzzerCallback, osTimerOnce, NULL, NULL);
    if (tim_apagar == NULL) return -1;

    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    tono_activo = false;
    return 0;
}

void Buzzer_Tono(uint16_t frecuencia_hz, uint16_t duracion_ms) {
    /* Sanitización de entradas */
    if (frecuencia_hz < BUZZER_FREQ_MIN_HZ || frecuencia_hz > BUZZER_FREQ_MAX_HZ) return;
    if (duracion_ms   < BUZZER_DUR_MIN_MS  || duracion_ms   > BUZZER_DUR_MAX_MS)  return;

    /* Si había un tono sonando, cancelo su apagado pendiente */
    if (tono_activo) {
        osTimerStop(tim_apagar);
        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    }

    uint32_t arr = (BUZZER_TIM_CLOCK_HZ / frecuencia_hz) - 1U;
    uint32_t ccr = (arr + 1U) / 2U;     /* duty 50% */

    __HAL_TIM_SET_AUTORELOAD(&htim3, arr);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, ccr);
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    tono_activo = true;

    osTimerStart(tim_apagar, duracion_ms);
}

void Buzzer_Stop(void) {
    if (tono_activo) {
        osTimerStop(tim_apagar);
        HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
        tono_activo = false;
    }
}
