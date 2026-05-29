/**
 ******************************************************************************
 * @file    fsm_modos.c
 * @author  Equipo R.E.A.C.T.
 ******************************************************************************
 */

#include "fsm_modos.h"
#include "comFibraB.h"
#include "RGB.h"
#include "buzzer.h"
#include "sensorProximidad.h"
#include <stdlib.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include "LedsRGB.h"

#ifndef COLOR_AMARILLO
#define COLOR_AMARILLO ((rgb_color_t){255, 255, 0})
#endif

/* === MODO MEMORIA === */
#define MEM_NIVELES_TOTAL          3U
#define MEM_LEN_NIVEL_1            4U
#define MEM_LEN_NIVEL_2_Y_3        5U
#define MEM_LED_ON_MS              500U
#define MEM_LED_OFF_MS             200U
#define MEM_FLASH_TURNO_MS         200U
#define MEM_PAUSA_PRE_SECUENCIA_MS 500U
#define MEM_TIMEOUT_INPUT_MS       3000U
#define MEM_FEEDBACK_FALLO_MS      1500U
#define MEM_FEEDBACK_VICTORIA_MS   2000U
#define MEM_TOF_DEBOUNCE_MS        500U
#define MEM_TOF_TIMEOUT_MS         10000U

/* === MODO FUERZA === */
#define FUERZA_NUM_RONDAS          3U
#define FUERZA_FLASH_PAD_MS        200U
#define FUERZA_TIMEOUT_GOLPE_MS    4000U
#define FUERZA_FEEDBACK_MS         500U
#define FUERZA_PAUSA_ENTRE_MS      500U
#define FUERZA_MARGEN              15U
#define FUERZA_OBJETIVO_PASO       10U
#define FUERZA_TOF_DEBOUNCE_MS     500U
#define FUERZA_TOF_TIMEOUT_MS      10000U

/* === MODO RITMO === */
#define RIT_NUM_NIVELES                3U
#define RIT_GOLPES_TOTALES             6U
#define RIT_INTERVALOS_MEDIDOS         5U
#define RIT_FLASH_INICIAL_MS           200U
#define RIT_TIMEOUT_PRIMER_GOLPE_MS    4000U
#define RIT_NIVEL_COMPLETADO_MS        1000U
#define RIT_FEEDBACK_FALLO_MS          1500U
#define RIT_FEEDBACK_VICTORIA_MS       2000U
#define RIT_BEEP_FREQ_HZ               880U
#define RIT_BEEP_DURATION_MS           100U
#define RIT_TIMEOUT_TOTAL_EXTRA_MS     1000U

/* === MODO INHIBICIÓN === */
#define INH_NUM_NIVELES                3U
#define INH_DURACION_NIVEL_MS          20000U
#define INH_DUR_ESTIMULO_NIVEL_1_MS    1000U
#define INH_DUR_ESTIMULO_NIVEL_2_MS    750U
#define INH_DUR_ESTIMULO_NIVEL_3_MS    500U
#define INH_PROB_ROJO_PCT              30U
#define INH_PROB_AGUDO_PCT             55U
#define INH_FRECUENCIA_GRAVE_HZ        440U
#define INH_FRECUENCIA_AGUDO_HZ        1760U
#define INH_DURACION_BEEP_MS           100U
#define INH_FALLOS_MAX                 3U
#define INH_PAUSA_ENTRE_NIVELES_MS     2000U
#define INH_FEEDBACK_FALLO_MS          1500U
#define INH_FEEDBACK_VICTORIA_MS       2000U
#define INH_NUM_COLORES_NO_ROJO        3U     

#define REPORTE_OK                 0x00
#define REPORTE_FALLO              0x01
#define REPORTE_ABORTADO_TOF       0xFF
#define MARCA_FIN_RONDA            0x80U

static FSM_Context_t ctx;

static void     ResetearContextoPartida(uint8_t id_modo);
static void     TransitarAModo(uint8_t id_modo);

static void     EnviarReporteFinalMemoria(uint8_t p2_nivel, uint8_t p3_codigo);
static void     EnviarReporteFinalFuerza(uint8_t aciertos, uint8_t fuerza_media);
static void     EnviarReporteFinalRitmo(uint8_t nivel_alcanzado, uint8_t codigo);
static void     EnviarReporteFinalInhibicion(uint8_t nivel_alcanzado, uint8_t fallos);
static void     EnviarEventoNivelMemoria(uint8_t nivel);
static void     EnviarEventoInicioRondaFuerza(uint8_t ronda, uint8_t objetivo);
static void     EnviarEventoFinRondaFuerza  (uint8_t ronda, uint8_t fuerza);
static void     EnviarEventoNivelRitmo(uint8_t nivel);
static void     EnviarEventoNivelInhibicion(uint8_t nivel);

static void     IniciarBajoConsumo(void);
static uint32_t ObtenerSemilla(void);

static void Handler_ModoMemoria   (FSM_Event_t evento, void* param_local);
static void Handler_ModoFuerza    (FSM_Event_t evento, void* param_local);
static void Handler_ModoRitmo     (FSM_Event_t evento, void* param_local);
static void Handler_ModoInhibicion(FSM_Event_t evento, void* param_local);

static uint8_t     GenerarObjetivoFuerza(void);
static void        ConfigurarParametrosRitmo(uint8_t nivel);
static rgb_color_t ElegirColorNoRojo(void);

void FSM_Init(void) {
    srand(ObtenerSemilla());
    memset(&ctx, 0, sizeof(ctx));
    ctx.estado_actual = STATE_IDLE;
    rgb_apagar_todos();
}

FSM_State_t FSM_GetEstadoActual(void) {
    return ctx.estado_actual;
}

void FSM_ProcesarEvento(FSM_Event_t evento, const TramaFibra_t* trama_in, void* param_local) {
    if (evento == EV_RX_COMANDO && trama_in != NULL) {
        if (trama_in->tipo_comando == CMD_GOTO_LOW_POWER) {
            IniciarBajoConsumo(); return;
        }
        if (trama_in->tipo_comando == CMD_START_JUEGO && ctx.estado_actual == STATE_IDLE) {
            uint8_t modo = trama_in->payload[0];
            if (modo >= MODO_MEMORIA && modo <= MODO_RITMO) {
                TransitarAModo(modo);
                FibraB_SendFrame(ACK_START_JUEGO, modo, 0, 0);
            }
            return;
        }
    }

    if (evento == EV_TICK_MS && ctx.timer_ms > 0) ctx.timer_ms--;

    switch (ctx.estado_actual) {
        case STATE_IDLE: break;
        case STATE_LOW_POWER:
            if (evento == EV_RX_COMANDO && trama_in != NULL && trama_in->tipo_comando != CMD_GOTO_LOW_POWER) {
                ctx.estado_actual = STATE_IDLE; rgb_apagar_todos();
            }
            break;
        case STATE_MODO_MEMORIA:    Handler_ModoMemoria(evento, param_local); break;
        case STATE_MODO_FUERZA:     Handler_ModoFuerza(evento, param_local); break;
        case STATE_MODO_RITMO:      Handler_ModoRitmo(evento, param_local); break;
        case STATE_MODO_INHIBICION: Handler_ModoInhibicion(evento, param_local); break;
        case STATE_SEND_RESULTS:    rgb_apagar_todos(); ctx.estado_actual = STATE_IDLE; break;
        default: ctx.estado_actual = STATE_IDLE; rgb_apagar_todos(); break;
    }
}

static void Handler_ModoMemoria(FSM_Event_t evento, void* param_local) {
    if (ctx.partida_recien_iniciada) {
        ctx.partida_recien_iniciada = false;
        ctx.mem.sub = MEM_ESPERANDO_MANO;
        ctx.mem.debounce_tof_ms = 0;
        ctx.mem.longitud_secuencia = 0;
        ctx.mem.indice_mostrar = 0;
        ctx.mem.indice_input = 0;
        ctx.timer_ms = MEM_TOF_TIMEOUT_MS;
        rgb_encender_todos(COLOR_BLANCO);
    }

    switch (ctx.mem.sub) {
        case MEM_ESPERANDO_MANO:
            if (evento == EV_TICK_MS) {
                if (ToF_ManoEnPosicionNeutra()) {
                    ctx.mem.debounce_tof_ms++;
                    if (ctx.mem.debounce_tof_ms >= MEM_TOF_DEBOUNCE_MS) {
                        ctx.mem.sub = MEM_GENERAR_NIVEL; ctx.nivel_actual = 1; rgb_apagar_todos();
                    }
                } else { ctx.mem.debounce_tof_ms = 0; }
                if (ctx.timer_ms == 0) {
                    rgb_apagar_todos(); EnviarReporteFinalMemoria(0, REPORTE_ABORTADO_TOF);
                    ctx.estado_actual = STATE_SEND_RESULTS;
                }
            }
            break;

        case MEM_GENERAR_NIVEL: 
            ctx.mem.longitud_secuencia = (ctx.nivel_actual == 1) ? MEM_LEN_NIVEL_1 : MEM_LEN_NIVEL_2_Y_3;
            for (uint8_t i = 0; i < ctx.mem.longitud_secuencia; i++) ctx.mem.secuencia[i] = (uint8_t)(rand() % 4);
            ctx.mem.indice_mostrar = 0; ctx.mem.indice_input = 0;
            EnviarEventoNivelMemoria(ctx.nivel_actual);
            ctx.timer_ms = MEM_PAUSA_PRE_SECUENCIA_MS; ctx.mem.sub = MEM_PAUSA_PRE_SECUENCIA;
            break;

        case MEM_PAUSA_PRE_SECUENCIA:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_encender_pad(ctx.mem.secuencia[ctx.mem.indice_mostrar], COLOR_BLANCO);
                ctx.timer_ms = MEM_LED_ON_MS; ctx.mem.sub = MEM_MOSTRAR_LED_ON;
            }
            break;

        case MEM_MOSTRAR_LED_ON:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_apagar_pad(ctx.mem.secuencia[ctx.mem.indice_mostrar]);
                ctx.timer_ms = MEM_LED_OFF_MS; ctx.mem.sub = MEM_MOSTRAR_LED_OFF;
            }
            break;

        case MEM_MOSTRAR_LED_OFF:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                ctx.mem.indice_mostrar++;
                if (ctx.mem.indice_mostrar >= ctx.mem.longitud_secuencia) {
                    rgb_encender_todos(COLOR_AZUL);
                    ctx.timer_ms = MEM_FLASH_TURNO_MS; ctx.mem.sub = MEM_FLASH_TURNO;
                } else {
                    rgb_encender_pad(ctx.mem.secuencia[ctx.mem.indice_mostrar], COLOR_BLANCO);
                    ctx.timer_ms = MEM_LED_ON_MS; ctx.mem.sub = MEM_MOSTRAR_LED_ON;
                }
            }
            break;

        case MEM_FLASH_TURNO:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_apagar_todos();
                ctx.mem.indice_input = 0;
                ctx.timer_ms = MEM_TIMEOUT_INPUT_MS; ctx.mem.sub = MEM_ESPERANDO_INPUT;
            }
            break;

        case MEM_ESPERANDO_INPUT:
            if (evento == EV_SENSOR_HIT && param_local != NULL) {
                EventoGolpe_t* g = (EventoGolpe_t*)param_local;
                if (g->sensor_id == ctx.mem.secuencia[ctx.mem.indice_input]) {
                    ctx.puntuacion++; ctx.mem.indice_input++;
                    if (ctx.mem.indice_input >= ctx.mem.longitud_secuencia) {
                        ctx.nivel_alcanzado = ctx.nivel_actual;
                        if (ctx.nivel_actual >= MEM_NIVELES_TOTAL) {
                            rgb_encender_todos(COLOR_VERDE);
                            ctx.timer_ms = MEM_FEEDBACK_VICTORIA_MS; ctx.mem.sub = MEM_FEEDBACK_VICTORIA;
                        } else {
                            ctx.nivel_actual++; ctx.mem.sub = MEM_GENERAR_NIVEL;
                        }
                    } else { ctx.timer_ms = MEM_TIMEOUT_INPUT_MS; }
                } else {
                    ctx.errores = 1; rgb_encender_todos(COLOR_ROJO);
                    ctx.timer_ms = MEM_FEEDBACK_FALLO_MS; ctx.mem.sub = MEM_FEEDBACK_FALLO;
                }
            } else if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                ctx.errores = 1; rgb_encender_todos(COLOR_ROJO);
                ctx.timer_ms = MEM_FEEDBACK_FALLO_MS; ctx.mem.sub = MEM_FEEDBACK_FALLO;
            }
            break;

        case MEM_FEEDBACK_FALLO:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                EnviarReporteFinalMemoria(ctx.nivel_alcanzado, REPORTE_FALLO);
                ctx.estado_actual = STATE_SEND_RESULTS; ctx.mem.sub = MEM_INACTIVO;
            }
            break;

        case MEM_FEEDBACK_VICTORIA:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                EnviarReporteFinalMemoria(ctx.nivel_alcanzado, REPORTE_OK);
                ctx.estado_actual = STATE_SEND_RESULTS; ctx.mem.sub = MEM_INACTIVO;
            }
            break;
            
        default: ctx.estado_actual = STATE_IDLE; rgb_apagar_todos(); break;
    }
}

static void Handler_ModoFuerza(FSM_Event_t evento, void* param_local) {
    if (ctx.partida_recien_iniciada) {
        ctx.partida_recien_iniciada = false;
        ctx.fuerza.sub = FZA_ESPERANDO_MANO;
        ctx.fuerza.ronda_actual = 0;
        ctx.fuerza.aciertos_total = 0;
        ctx.fuerza.suma_fuerzas = 0;
        ctx.fuerza.debounce_tof_ms = 0;
        ctx.timer_ms = FUERZA_TOF_TIMEOUT_MS;
        rgb_encender_todos(COLOR_BLANCO);
    }

    switch (ctx.fuerza.sub) {
        case FZA_ESPERANDO_MANO:
            if (evento == EV_TICK_MS) {
                if (ToF_ManoEnPosicionNeutra()) {
                    ctx.fuerza.debounce_tof_ms++;
                    if (ctx.fuerza.debounce_tof_ms >= FUERZA_TOF_DEBOUNCE_MS) {
                        rgb_apagar_todos(); ctx.fuerza.ronda_actual = 1; ctx.fuerza.sub = FZA_INICIANDO_RONDA;
                    }
                } else { ctx.fuerza.debounce_tof_ms = 0; }
                if (ctx.timer_ms == 0) {
                    rgb_apagar_todos(); FibraB_SendFrame(REPORTE_FINAL, MODO_FUERZA, 0xFF, 0);
                    ctx.estado_actual = STATE_SEND_RESULTS;
                }
            }
            break;

        case FZA_INICIANDO_RONDA: 
            ctx.fuerza.pad_objetivo = (uint8_t)(rand() % 4);
            ctx.fuerza.fuerza_objetivo = GenerarObjetivoFuerza();
            ctx.fuerza.fuerza_conseguida = 0;
            
            rgb_color_t color_obj;
            if (ctx.fuerza.fuerza_objetivo < 40) color_obj = COLOR_AZUL;
            else if (ctx.fuerza.fuerza_objetivo < 70) color_obj = COLOR_AMARILLO;
            else color_obj = COLOR_BLANCO;

            EnviarEventoInicioRondaFuerza(ctx.fuerza.ronda_actual, ctx.fuerza.fuerza_objetivo);
            rgb_encender_pad(ctx.fuerza.pad_objetivo, color_obj);
            ctx.timer_ms = FUERZA_FLASH_PAD_MS; ctx.fuerza.sub = FZA_FLASH_PAD;
            break;

        case FZA_FLASH_PAD:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_apagar_pad(ctx.fuerza.pad_objetivo);
                ctx.timer_ms = FUERZA_TIMEOUT_GOLPE_MS; ctx.fuerza.sub = FZA_ESPERANDO_GOLPE;
            }
            break;

        case FZA_ESPERANDO_GOLPE:
            if (evento == EV_SENSOR_HIT && param_local != NULL) {
                EventoGolpe_t* g = (EventoGolpe_t*)param_local;
                if (g->sensor_id == ctx.fuerza.pad_objetivo) {
                    
                    // AHORA g->fuerza YA VIENE COMO PORCENTAJE DESDE CerebroB.c
                    ctx.fuerza.fuerza_conseguida = (uint8_t)g->fuerza; 
                    ctx.fuerza.suma_fuerzas += ctx.fuerza.fuerza_conseguida;
                    
                    int diff = (int)ctx.fuerza.fuerza_conseguida - (int)ctx.fuerza.fuerza_objetivo;
                    if (diff < 0) diff = -diff;
                    
                    if (diff <= (int)FUERZA_MARGEN) {
                        ctx.fuerza.aciertos_total++; rgb_encender_pad(ctx.fuerza.pad_objetivo, COLOR_VERDE);
                    } else {
                        rgb_encender_pad(ctx.fuerza.pad_objetivo, COLOR_ROJO);
                    }
                    ctx.timer_ms = FUERZA_FEEDBACK_MS; ctx.fuerza.sub = FZA_FEEDBACK_RONDA;
                }
            }
            else if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                ctx.fuerza.fuerza_conseguida = 0; rgb_encender_pad(ctx.fuerza.pad_objetivo, COLOR_ROJO);
                ctx.timer_ms = FUERZA_FEEDBACK_MS; ctx.fuerza.sub = FZA_FEEDBACK_RONDA;
            }
            break;

        case FZA_FEEDBACK_RONDA:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_apagar_pad(ctx.fuerza.pad_objetivo);
                EnviarEventoFinRondaFuerza(ctx.fuerza.ronda_actual, ctx.fuerza.fuerza_conseguida);
                ctx.timer_ms = FUERZA_PAUSA_ENTRE_MS; ctx.fuerza.sub = FZA_PAUSA_ENTRE_RONDAS;
            }
            break;

        case FZA_PAUSA_ENTRE_RONDAS:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                if (ctx.fuerza.ronda_actual >= FUERZA_NUM_RONDAS) ctx.fuerza.sub = FZA_CIERRE_PARTIDA;
                else { ctx.fuerza.ronda_actual++; ctx.fuerza.sub = FZA_INICIANDO_RONDA; }
            }
            break;

        case FZA_CIERRE_PARTIDA: {
            uint8_t media = (uint8_t)(ctx.fuerza.suma_fuerzas / FUERZA_NUM_RONDAS);
            ctx.nivel_alcanzado = ctx.fuerza.aciertos_total; ctx.puntuacion = media;
            EnviarReporteFinalFuerza(ctx.fuerza.aciertos_total, media);
            ctx.estado_actual = STATE_SEND_RESULTS; ctx.fuerza.sub = FZA_INACTIVO;
            break;
        }

        default: ctx.estado_actual = STATE_IDLE; rgb_apagar_todos(); break;
    }
}

static void Handler_ModoRitmo(FSM_Event_t evento, void* param_local) {
    if (ctx.partida_recien_iniciada) {
        ctx.partida_recien_iniciada = false;
        ctx.ritmo.sub = RIT_INICIANDO_NIVEL;
        ctx.nivel_actual = 1; ctx.nivel_alcanzado = 0;
    }

    switch (ctx.ritmo.sub) {
        case RIT_INICIANDO_NIVEL: 
            ConfigurarParametrosRitmo(ctx.nivel_actual);
            ctx.ritmo.pad_objetivo = (uint8_t)(rand() % 4);
            ctx.ritmo.golpes_dados = 0; ctx.ritmo.beeps_pendientes = 0; ctx.ritmo.golpe_anterior_ms = 0;
            EnviarEventoNivelRitmo(ctx.nivel_actual);
            rgb_encender_pad(ctx.ritmo.pad_objetivo, COLOR_AZUL);
            ctx.timer_ms = RIT_FLASH_INICIAL_MS; ctx.ritmo.sub = RIT_FLASH_INICIAL;
            break;

        case RIT_FLASH_INICIAL:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_apagar_pad(ctx.ritmo.pad_objetivo); Buzzer_Tono(RIT_BEEP_FREQ_HZ, RIT_BEEP_DURATION_MS);
                ctx.ritmo.beeps_pendientes = 2U; ctx.timer_ms = ctx.ritmo.periodo_ms; ctx.ritmo.sub = RIT_BEEPS_AVISO;
            }
            break;

        case RIT_BEEPS_AVISO:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                if (ctx.ritmo.beeps_pendientes > 0) {
                    Buzzer_Tono(RIT_BEEP_FREQ_HZ, RIT_BEEP_DURATION_MS);
                    ctx.ritmo.beeps_pendientes--; ctx.timer_ms = ctx.ritmo.periodo_ms;
                } else {
                    ctx.timer_ms = RIT_TIMEOUT_PRIMER_GOLPE_MS; ctx.ritmo.sub = RIT_ESPERANDO_PRIMER_GOLPE;
                }
            }
            break;

        case RIT_ESPERANDO_PRIMER_GOLPE:
            if (evento == EV_SENSOR_HIT && param_local != NULL) {
                EventoGolpe_t* g = (EventoGolpe_t*)param_local;
                if (g->sensor_id == ctx.ritmo.pad_objetivo) {
                    ctx.ritmo.golpe_anterior_ms = osKernelGetTickCount(); ctx.ritmo.golpes_dados = 1U;
                    ctx.timer_ms = (uint32_t)ctx.ritmo.periodo_ms * RIT_GOLPES_TOTALES + RIT_TIMEOUT_TOTAL_EXTRA_MS;
                    ctx.ritmo.sub = RIT_MIDIENDO_INTERVALOS;
                }
            } else if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_encender_todos(COLOR_ROJO); ctx.timer_ms = RIT_FEEDBACK_FALLO_MS; ctx.ritmo.sub = RIT_FEEDBACK_FALLO;
            }
            break;

        case RIT_MIDIENDO_INTERVALOS:
            if (evento == EV_SENSOR_HIT && param_local != NULL) {
                EventoGolpe_t* g = (EventoGolpe_t*)param_local;
                if (g->sensor_id == ctx.ritmo.pad_objetivo) {
                    uint32_t ahora = osKernelGetTickCount();
                    uint32_t intervalo = ahora - ctx.ritmo.golpe_anterior_ms;
                    int32_t diff = (int32_t)intervalo - (int32_t)ctx.ritmo.periodo_ms;
                    if (diff < 0) diff = -diff;

                    if (diff > (int32_t)ctx.ritmo.tolerancia_ms) {
                        rgb_encender_todos(COLOR_ROJO); ctx.timer_ms = RIT_FEEDBACK_FALLO_MS; ctx.ritmo.sub = RIT_FEEDBACK_FALLO;
                    } else {
                        ctx.ritmo.golpes_dados++; ctx.ritmo.golpe_anterior_ms = ahora;
                        if (ctx.ritmo.golpes_dados >= RIT_GOLPES_TOTALES) {
                            ctx.nivel_alcanzado = ctx.nivel_actual;
                            if (ctx.nivel_actual >= RIT_NUM_NIVELES) {
                                rgb_encender_todos(COLOR_VERDE); ctx.timer_ms = RIT_FEEDBACK_VICTORIA_MS; ctx.ritmo.sub = RIT_FEEDBACK_VICTORIA;
                            } else {
                                rgb_encender_todos(COLOR_VERDE); ctx.timer_ms = RIT_NIVEL_COMPLETADO_MS; ctx.ritmo.sub = RIT_NIVEL_COMPLETADO;
                            }
                        }
                    }
                }
            } else if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_encender_todos(COLOR_ROJO); ctx.timer_ms = RIT_FEEDBACK_FALLO_MS; ctx.ritmo.sub = RIT_FEEDBACK_FALLO;
            }
            break;

        case RIT_NIVEL_COMPLETADO:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_apagar_todos(); ctx.nivel_actual++; ctx.ritmo.sub = RIT_INICIANDO_NIVEL;
            }
            break;

        case RIT_FEEDBACK_FALLO:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                EnviarReporteFinalRitmo(ctx.nivel_alcanzado, REPORTE_FALLO); ctx.estado_actual = STATE_SEND_RESULTS; ctx.ritmo.sub = RIT_INACTIVO;
            }
            break;

        case RIT_FEEDBACK_VICTORIA:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                EnviarReporteFinalRitmo(ctx.nivel_alcanzado, REPORTE_OK); ctx.estado_actual = STATE_SEND_RESULTS; ctx.ritmo.sub = RIT_INACTIVO;
            }
            break;

        default: ctx.estado_actual = STATE_IDLE; rgb_apagar_todos(); break;
    }
}

static void Handler_ModoInhibicion(FSM_Event_t evento, void* param_local) {
    if (ctx.partida_recien_iniciada) {
        ctx.partida_recien_iniciada = false;
        ctx.inhib.sub = INH_INICIANDO_NIVEL;
        ctx.inhib.fallos_acumulados = 0; ctx.nivel_actual = 1; ctx.nivel_alcanzado = 0;
    }

    if (evento == EV_TICK_MS && ctx.inhib.tiempo_restante_nivel_ms > 0 && ctx.inhib.sub == INH_ESTIMULO_ACTIVO) {
        ctx.inhib.tiempo_restante_nivel_ms--;
    }

    switch (ctx.inhib.sub) {
        case INH_INICIANDO_NIVEL:
            if (ctx.nivel_actual == 1) ctx.inhib.duracion_estimulo_ms = INH_DUR_ESTIMULO_NIVEL_1_MS;
            else if (ctx.nivel_actual == 2) ctx.inhib.duracion_estimulo_ms = INH_DUR_ESTIMULO_NIVEL_2_MS;
            else ctx.inhib.duracion_estimulo_ms = INH_DUR_ESTIMULO_NIVEL_3_MS;
            
            ctx.inhib.tiempo_restante_nivel_ms = INH_DURACION_NIVEL_MS;
            EnviarEventoNivelInhibicion(ctx.nivel_actual);
            ctx.inhib.sub = INH_GENERAR_ESTIMULO;
            break;

        case INH_GENERAR_ESTIMULO: {
            ctx.inhib.pad_actual = (uint8_t)(rand() % 4);
            ctx.inhib.color_es_rojo = ((rand() % 100) < (int)INH_PROB_ROJO_PCT);
            ctx.inhib.sonido_es_agudo = ((rand() % 100) < (int)INH_PROB_AGUDO_PCT);
            ctx.inhib.debe_pulsar = !ctx.inhib.color_es_rojo && ctx.inhib.sonido_es_agudo;
            ctx.inhib.jugador_pulso_alguno = false; ctx.inhib.jugador_pulso_correcto = false;

            rgb_encender_pad(ctx.inhib.pad_actual, ctx.inhib.color_es_rojo ? COLOR_ROJO : ElegirColorNoRojo());
            Buzzer_Tono(ctx.inhib.sonido_es_agudo ? INH_FRECUENCIA_AGUDO_HZ : INH_FRECUENCIA_GRAVE_HZ, INH_DURACION_BEEP_MS);

            ctx.timer_ms = ctx.inhib.duracion_estimulo_ms; ctx.inhib.sub = INH_ESTIMULO_ACTIVO;
            break;
        }

        case INH_ESTIMULO_ACTIVO:
            if (evento == EV_SENSOR_HIT && param_local != NULL && !ctx.inhib.jugador_pulso_alguno) {
                EventoGolpe_t* g = (EventoGolpe_t*)param_local;
                ctx.inhib.jugador_pulso_alguno = true;
                if (g->sensor_id == ctx.inhib.pad_actual) ctx.inhib.jugador_pulso_correcto = true;
            }

            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                if (ctx.inhib.debe_pulsar && !ctx.inhib.jugador_pulso_correcto) ctx.inhib.fallos_acumulados++;
                else if (!ctx.inhib.debe_pulsar && ctx.inhib.jugador_pulso_alguno) ctx.inhib.fallos_acumulados++;

                rgb_apagar_pad(ctx.inhib.pad_actual);

                if (ctx.inhib.fallos_acumulados >= INH_FALLOS_MAX) {
                    rgb_encender_todos(COLOR_ROJO); ctx.timer_ms = INH_FEEDBACK_FALLO_MS; ctx.inhib.sub = INH_FEEDBACK_FALLO; break;
                }
                if (ctx.inhib.tiempo_restante_nivel_ms == 0) {
                    ctx.nivel_alcanzado = ctx.nivel_actual;
                    if (ctx.nivel_actual >= INH_NUM_NIVELES) {
                        rgb_encender_todos(COLOR_VERDE); ctx.timer_ms = INH_FEEDBACK_VICTORIA_MS; ctx.inhib.sub = INH_FEEDBACK_VICTORIA;
                    } else {
                        rgb_encender_todos(COLOR_VERDE); ctx.timer_ms = INH_PAUSA_ENTRE_NIVELES_MS; ctx.inhib.sub = INH_PAUSA_ENTRE_NIVELES;
                    }
                    break;
                }
                ctx.inhib.sub = INH_GENERAR_ESTIMULO;
            }
            break;

        case INH_PAUSA_ENTRE_NIVELES:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                rgb_apagar_todos(); ctx.nivel_actual++; ctx.inhib.sub = INH_INICIANDO_NIVEL;
            }
            break;

        case INH_FEEDBACK_FALLO:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                EnviarReporteFinalInhibicion(ctx.nivel_alcanzado, ctx.inhib.fallos_acumulados); ctx.estado_actual = STATE_SEND_RESULTS; ctx.inhib.sub = INH_INACTIVO;
            }
            break;

        case INH_FEEDBACK_VICTORIA:
            if (evento == EV_TICK_MS && ctx.timer_ms == 0) {
                EnviarReporteFinalInhibicion(INH_NUM_NIVELES, ctx.inhib.fallos_acumulados); ctx.estado_actual = STATE_SEND_RESULTS; ctx.inhib.sub = INH_INACTIVO;
            }
            break;

        default: ctx.estado_actual = STATE_IDLE; rgb_apagar_todos(); break;
    }
}

static void ResetearContextoPartida(uint8_t id_modo) {
    FSM_State_t estado_backup = ctx.estado_actual;
    memset(&ctx, 0, sizeof(ctx));
    ctx.estado_actual = estado_backup;
    ctx.id_modo_actual = id_modo;
    ctx.nivel_actual = 1;
    ctx.partida_recien_iniciada = true;
}

static void TransitarAModo(uint8_t id_modo) {
    ResetearContextoPartida(id_modo);
    switch (id_modo) {
        case MODO_MEMORIA:    ctx.estado_actual = STATE_MODO_MEMORIA;    break;
        case MODO_FUERZA:     ctx.estado_actual = STATE_MODO_FUERZA;     break;
        case MODO_RITMO:      ctx.estado_actual = STATE_MODO_RITMO;      break;
        case MODO_INHIBICION: ctx.estado_actual = STATE_MODO_INHIBICION; break;
        default:              ctx.estado_actual = STATE_IDLE;            break;
    }
}

static void EnviarReporteFinalMemoria(uint8_t p2_nivel, uint8_t p3_codigo) { FibraB_SendFrame(REPORTE_FINAL, MODO_MEMORIA, p2_nivel, p3_codigo); }
static void EnviarReporteFinalFuerza(uint8_t aciertos, uint8_t fza_media) { FibraB_SendFrame(REPORTE_FINAL, MODO_FUERZA, aciertos, fza_media); }
static void EnviarReporteFinalRitmo(uint8_t nivel_alcanzado, uint8_t codigo) { FibraB_SendFrame(REPORTE_FINAL, MODO_RITMO, nivel_alcanzado, codigo); }
static void EnviarReporteFinalInhibicion(uint8_t nivel_alcanzado, uint8_t fallos) { FibraB_SendFrame(REPORTE_FINAL, MODO_INHIBICION, nivel_alcanzado, fallos); }
static void EnviarEventoNivelMemoria(uint8_t nivel) { FibraB_SendFrame(EVENTO_TIEMPO_REAL, MODO_MEMORIA, nivel, (uint8_t)ctx.puntuacion); }
static void EnviarEventoInicioRondaFuerza(uint8_t ronda, uint8_t obj) { FibraB_SendFrame(EVENTO_TIEMPO_REAL, MODO_FUERZA, ronda, obj); }
static void EnviarEventoFinRondaFuerza(uint8_t ronda, uint8_t fza) { FibraB_SendFrame(EVENTO_TIEMPO_REAL, MODO_FUERZA, (uint8_t)(ronda | MARCA_FIN_RONDA), fza); }
static void EnviarEventoNivelRitmo(uint8_t nivel) { FibraB_SendFrame(EVENTO_TIEMPO_REAL, MODO_RITMO, nivel, 0); }
static void EnviarEventoNivelInhibicion(uint8_t nivel) { FibraB_SendFrame(EVENTO_TIEMPO_REAL, MODO_INHIBICION, nivel, 0); }

static void IniciarBajoConsumo(void) {
    rgb_apagar_todos(); Buzzer_Stop(); FibraB_SendFrame(ACK_GOTO_LOW_POWER, 0, 0, 0); ctx.estado_actual = STATE_LOW_POWER;
}

static uint32_t ObtenerSemilla(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    return (uint32_t)DWT->CYCCNT ^ (uint32_t)HAL_GetTick();
}

static uint8_t GenerarObjetivoFuerza(void) { return (uint8_t)(((rand() % 10) + 1) * 10U); }

static void ConfigurarParametrosRitmo(uint8_t nivel) {
    if (nivel == 1) { ctx.ritmo.periodo_ms = 500U; ctx.ritmo.tolerancia_ms = 100U; }
    else if (nivel == 2) { ctx.ritmo.periodo_ms = 1000U; ctx.ritmo.tolerancia_ms = 150U; }
    else { ctx.ritmo.periodo_ms = 2000U; ctx.ritmo.tolerancia_ms = 300U; }
}

static rgb_color_t ElegirColorNoRojo(void) {
    uint8_t pick = (uint8_t)(rand() % 3);
    if (pick == 0) return COLOR_VERDE;
    if (pick == 1) return COLOR_AZUL;
    return COLOR_BLANCO;
}

void rgb_encender_pad(uint8_t pad, rgb_color_t color) { LedsRGB_FillAnillo(pad, color.r, color.g, color.b); LedsRGB_Show(); }
void rgb_apagar_pad(uint8_t pad) { LedsRGB_FillAnillo(pad, 0U, 0U, 0U); LedsRGB_Show(); }
void rgb_encender_todos(rgb_color_t color) { for(int i=0; i<4; i++) LedsRGB_FillAnillo(i, color.r, color.g, color.b); LedsRGB_Show(); }
void rgb_apagar_todos(void) { LedsRGB_Clear(); LedsRGB_Show(); }
