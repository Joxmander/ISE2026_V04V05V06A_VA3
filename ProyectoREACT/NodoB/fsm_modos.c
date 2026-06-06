/**
 ******************************************************************************
 * @file    fsm_modos.c
 * @brief   Máquina de estados de los 4 modos de juego arcade — Nodo B.
 ******************************************************************************
 */

#include "fsm_modos.h"
#include "comFibraB.h"
#include "LedsRGB.h"
#include "stm32f4xx_hal.h"
#include "cmsis_os2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Colores LED -- */
#define C_BLANCO_R   160U
#define C_BLANCO_G   160U
#define C_BLANCO_B   160U
#define C_VERDE_R      0U
#define C_VERDE_G    200U
#define C_VERDE_B      0U
#define C_ROJO_R     200U
#define C_ROJO_G       0U
#define C_ROJO_B       0U
#define C_AZUL_R       0U
#define C_AZUL_G       0U
#define C_AZUL_B     200U
#define C_AMARILLO_R 200U
#define C_AMARILLO_G 140U
#define C_AMARILLO_B   0U
#define C_CYAN_R       0U
#define C_CYAN_G     160U
#define C_CYAN_B     220U

/* Paleta aleatoria: 0=Rojo, 1=Verde, 2=Azul, 3=Amarillo, 4=Magenta, 5=Cyan */
static const uint8_t PALETA_R[] = {200,   0,   0, 200, 200,   0};
static const uint8_t PALETA_G[] = {  0, 200,   0, 140,   0, 200};
static const uint8_t PALETA_B[] = {  0,   0, 200,   0, 200, 200};

/* -- Helpers LED -- */
static void Leds_PadOn(uint8_t pad, uint8_t r, uint8_t g, uint8_t b) { LedsRGB_FillAnillo(pad, r, g, b); LedsRGB_Show(); }
static void Leds_PadOff(uint8_t pad) { LedsRGB_FillAnillo(pad, 0U, 0U, 0U); LedsRGB_Show(); }
static void Leds_AllOn(uint8_t r, uint8_t g, uint8_t b) { for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) LedsRGB_FillAnillo(a, r, g, b); LedsRGB_Show(); }
static void Leds_AllOff(void) { LedsRGB_Clear(); LedsRGB_Show(); }

/* -- Estados Generales -- */
#define M_GENERAR         0U   
#define M_PRE_PAUSA       1U   
#define M_DEMO_ON         2U   
#define M_DEMO_OFF        3U   
#define M_TURNO_FLASH     4U   
#define M_INPUT           5U   
#define M_HIT_OK          6U   
#define M_NIVEL_OK        7U   
#define M_WIN_FLASH       8U   
#define M_FAIL_FLASH      9U   
#define M_ESTIMULO        10U

#define M2_INICIO         0U   
#define M2_GENERAR        1U
#define M2_ESPERA         2U   
#define M2_HIT_OK         3U
#define M2_NIVEL_OK       4U
#define M2_WIN_FLASH      5U   
#define M2_FAIL_FLASH     6U   

/* -- Parámetros Modo 1 -- */
#define M1_PRE_PAUSA_MS    500U
#define M1_DEMO_ON_MS      550U
#define M1_DEMO_OFF_MS     250U
#define M1_TURNO_MS        350U
#define M1_INPUT_TIMEOUT  4000U
#define M1_HIT_FLASH_MS    300U
#define M1_NIVEL_OK_MS    1000U
#define M1_WIN_MS         2000U
#define M1_FAIL_MS        1500U
static const uint8_t M1_LONGITUD[3U] = {4U, 5U, 6U};   

/* -- Parámetros Modo 2 (Whack-a-mole Suave) -- */
#define M2_PADS_N1        4U
#define M2_PADS_N2        6U
#define M2_PADS_N3        8U
#define M2_TIEMPO_N1      2000U
#define M2_TIEMPO_N2      1500U
#define M2_TIEMPO_N3      1000U

/* -- Parámetros Modo 3 -- */
#define M3_HIT_OK_MS       300U
#define M3_NIVEL_OK_MS    1000U
#define M3_WIN_MS         2000U
#define M3_FAIL_MS        1500U
static const uint32_t M3_VENTANA_MS[3U]  = {1500U, 1000U,  700U};
static const uint8_t  M3_NECESARIOS[3U]  = {  10U,   15U,   20U}; 
static const uint8_t  M3_PROB_ROJO_PCT   = 35U;   
static const uint8_t  M3_PROB_DOBLE_PCT  = 50U;   

/* -- Parámetros Modo 4 (Simon Infinito) -- */
#define M4_DEMO_ON_MS      400U
#define M4_DEMO_OFF_MS     200U
#define M4_INPUT_TIMEOUT  3000U

#pragma anon_unions
typedef struct {
    EstadoJuego_t  estado;
    uint8_t        modo_id;
    uint8_t        nivel;           
    uint8_t        nivel_alcanzado; 
    uint8_t        fallos;          
    uint16_t       aciertos;        
    uint8_t        sub;             
    uint32_t       deadline;        
    uint32_t       objetivo_ms;     

    union {
        struct {
            uint8_t secuencia[8U];
            uint8_t colores[8U]; 
            uint8_t lon;
            uint8_t idx_demo;       
            uint8_t idx_input;      
        } mem;
        struct {
            uint8_t pad_obj;
            uint8_t pads_necesarios;
            uint8_t pads_golpeados;
            uint32_t tiempo_pad;
        } fza;
        struct {
            uint8_t pad_verde;      
            uint8_t pad_rojo;
            uint8_t hay_doble;      
            uint8_t es_rojo;        
            uint8_t aciertos_nivel;
            uint8_t necesarios;
            uint8_t color_obj_idx;  
        } inh;
        struct {
            uint8_t secuencia[M4_MAX_SEQ];
            uint8_t colores[M4_MAX_SEQ]; 
            uint8_t seq_len;
            uint8_t idx_demo;       
            uint8_t idx_input;  
            uint16_t score_rondas;    
        } simon;
    };
} FSMCtx_t;

static FSMCtx_t ctx;

static const char *NombreModo(uint8_t id) {
    switch (id) {
    case MODO_MEMORIA:    return "Memoria de Trabajo";
    case MODO_FUERZA:     return "Control de Fuerza";
    case MODO_INHIBICION: return "Inhibicion Motora";
    case MODO_RITMO:      return "Simon Infinito";
    default:              return "Desconocido";
    }
}

static void IniciarModo(uint8_t modo_id, uint32_t ahora_ms);
static void FinPartida(uint8_t nivel, uint8_t fallos_pts, uint32_t ahora_ms);
static void Tick_M1(uint32_t ahora_ms);
static void Golpe_M1(const EventoGolpe_t *g, uint32_t ahora_ms);
static void Tick_M2(uint32_t ahora_ms);
static void Golpe_M2(const EventoGolpe_t *g, uint32_t ahora_ms);
static void Tick_M3(uint32_t ahora_ms);
static void Golpe_M3(const EventoGolpe_t *g, uint32_t ahora_ms);
static void Tick_M4(uint32_t ahora_ms);
static void Golpe_M4(const EventoGolpe_t *g, uint32_t ahora_ms);
static uint32_t ObtenerSemilla(void);

void FSM_Init(void) {
    memset(&ctx, 0U, sizeof(ctx));
    ctx.estado = JUEGO_REPOSO;
    srand(ObtenerSemilla());
    Leds_AllOff();
}

EstadoJuego_t FSM_GetEstado(void) { return ctx.estado; }

void FSM_ComandoFibra(uint8_t cmd, uint8_t p0, uint8_t p1, uint8_t p2, uint32_t ahora_ms) {
    (void)p1; (void)p2;
    switch (cmd) {
    case CMD_PING:
        FibraB_SendFrame(CMD_PING_ACK, (ctx.estado == JUEGO_REPOSO) ? ESTADO_ACTIVO : ESTADO_JUGANDO, 0U, 0U);
        break;
    case CMD_INICIO_JUEGO:
        if (ctx.estado == JUEGO_REPOSO && p0 >= MODO_MEMORIA && p0 <= MODO_RITMO) {
            IniciarModo(p0, ahora_ms);
            FibraB_SendFrame(CMD_ACK_INICIO, p0, 0U, 0U);
        }
        break;
    case CMD_SLEEP:
        Leds_AllOff();
        memset(&ctx, 0U, sizeof(ctx));
        ctx.estado = JUEGO_REPOSO;
        FibraB_SendFrame(CMD_ACK_SLEEP, 0U, 0U, 0U);
        break;
    default: break;
    }
}

void FSM_Tick(uint32_t ahora_ms) {
    switch (ctx.estado) {
    case JUEGO_MODO_MEMORIA:    Tick_M1(ahora_ms); break;
    case JUEGO_MODO_FUERZA:     Tick_M2(ahora_ms); break;
    case JUEGO_MODO_INHIBICION: Tick_M3(ahora_ms); break;
    case JUEGO_MODO_RITMO:      Tick_M4(ahora_ms); break;
    default: break;
    }
}

void FSM_GolpePad(const EventoGolpe_t *g, uint32_t ahora_ms) {
    if (g == NULL) return;
    switch (ctx.estado) {
    case JUEGO_MODO_MEMORIA:    Golpe_M1(g, ahora_ms); break;
    case JUEGO_MODO_FUERZA:     Golpe_M2(g, ahora_ms); break;
    case JUEGO_MODO_INHIBICION: Golpe_M3(g, ahora_ms); break;
    case JUEGO_MODO_RITMO:      Golpe_M4(g, ahora_ms); break;
    default: break;
    }
}

static void IniciarModo(uint8_t modo_id, uint32_t ahora_ms) {
    memset(&ctx, 0U, sizeof(ctx));
    ctx.modo_id  = modo_id;
    ctx.nivel    = 1U;
    ctx.deadline = ahora_ms;   
    switch (modo_id) {
    case MODO_MEMORIA:    ctx.estado = JUEGO_MODO_MEMORIA;    break;
    case MODO_FUERZA:     ctx.estado = JUEGO_MODO_FUERZA;     break;
    case MODO_INHIBICION: ctx.estado = JUEGO_MODO_INHIBICION; break;
    case MODO_RITMO:      ctx.estado = JUEGO_MODO_RITMO;      break;
    default: return;
    }
    printf("\r\n>>> INICIO MODO %u: %s <<<\r\n", modo_id, NombreModo(modo_id));
}

static void FinPartida(uint8_t nivel, uint8_t fallos_pts, uint32_t ahora_ms) {
    (void)ahora_ms;
    printf("\r\n=== FIN PARTIDA: %s ===\r\n", NombreModo(ctx.modo_id));
    printf("  Nivel alcanzado : %u / 3\r\n", (unsigned)nivel);
    printf("  Puntos/Fallos   : %u\r\n",     (unsigned)fallos_pts);
    FibraB_SendFrame(CMD_REPORTE_FINAL, ctx.modo_id, nivel, fallos_pts);
    Leds_AllOff();
    ctx.estado = JUEGO_REPOSO;
}

static uint32_t ObtenerSemilla(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
    return (uint32_t)DWT->CYCCNT ^ (uint32_t)HAL_GetTick();
}

/* =======================================================
 * MODO 1: MEMORIA DE TRABAJO (CLÁSICO)
 * ======================================================= */
static void Tick_M1(uint32_t ahora_ms) {
    if (ahora_ms < ctx.deadline) return;

    switch (ctx.sub) {
    case M_GENERAR:
        ctx.mem.lon = M1_LONGITUD[ctx.nivel - 1U];
        ctx.mem.idx_demo = 0U; ctx.mem.idx_input = 0U;
        for (uint8_t i = 0U; i < ctx.mem.lon; i++) {
            ctx.mem.secuencia[i] = (uint8_t)(rand() % 4U);
            ctx.mem.colores[i] = (uint8_t)(rand() % 6U); 
        }
        FibraB_SendFrame(CMD_EVENTO_RT, ctx.modo_id, ctx.nivel, 0U);
        Leds_AllOff();
        ctx.sub = M_PRE_PAUSA; ctx.deadline = ahora_ms + M1_PRE_PAUSA_MS;
        break;

    case M_PRE_PAUSA:
    case M_DEMO_OFF:
        if (ctx.sub == M_DEMO_OFF) ctx.mem.idx_demo++;
        
        if (ctx.mem.idx_demo >= ctx.mem.lon) {
            Leds_AllOn(C_AZUL_R, C_AZUL_G, C_AZUL_B);
            ctx.sub = M_TURNO_FLASH; ctx.deadline = ahora_ms + M1_TURNO_MS;
        } else {
            uint8_t color_idx = ctx.mem.colores[ctx.mem.idx_demo];
            Leds_PadOn(ctx.mem.secuencia[ctx.mem.idx_demo], PALETA_R[color_idx], PALETA_G[color_idx], PALETA_B[color_idx]);
            ctx.sub = M_DEMO_ON; ctx.deadline = ahora_ms + M1_DEMO_ON_MS;
        }
        break;

    case M_DEMO_ON:
        Leds_PadOff(ctx.mem.secuencia[ctx.mem.idx_demo]);
        ctx.sub = M_DEMO_OFF; ctx.deadline = ahora_ms + M1_DEMO_OFF_MS;
        break;

    case M_TURNO_FLASH:
        Leds_AllOff();
        ctx.sub = M_INPUT; ctx.objetivo_ms = ahora_ms; ctx.deadline = ahora_ms + M1_INPUT_TIMEOUT;
        break;

    case M_INPUT:
        ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B);
        ctx.sub = M_FAIL_FLASH; ctx.deadline = ahora_ms + M1_FAIL_MS;
        break;

    case M_HIT_OK:
        Leds_PadOff(ctx.mem.secuencia[ctx.mem.idx_input - 1U]);
        if (ctx.mem.idx_input >= ctx.mem.lon) {
            ctx.nivel_alcanzado = ctx.nivel;
            Leds_AllOn(C_VERDE_R, C_VERDE_G, C_VERDE_B);
            if (ctx.nivel >= 3U) { ctx.sub = M_WIN_FLASH; ctx.deadline = ahora_ms + M1_WIN_MS; } 
            else { ctx.sub = M_NIVEL_OK; ctx.deadline = ahora_ms + M1_NIVEL_OK_MS; }
        } else {
            ctx.sub = M_INPUT; ctx.deadline = ahora_ms + M1_INPUT_TIMEOUT;
        }
        break;

    case M_NIVEL_OK:
        ctx.nivel++; Leds_AllOff(); ctx.sub = M_GENERAR; ctx.deadline = ahora_ms;
        break;
    case M_WIN_FLASH: FinPartida(ctx.nivel_alcanzado, (uint8_t)ctx.aciertos, ahora_ms); break;
    case M_FAIL_FLASH: FinPartida(ctx.nivel_alcanzado, ctx.fallos, ahora_ms); break;
    default: ctx.sub = M_GENERAR; ctx.deadline = ahora_ms; break;
    }
}

static void Golpe_M1(const EventoGolpe_t *g, uint32_t ahora_ms) {
    if (ctx.sub != M_INPUT) return;
    FibraB_SendFrame(CMD_EVENTO_RT, g->pad_id, g->fuerza_pct, 0);

    if (g->pad_id == ctx.mem.secuencia[ctx.mem.idx_input]) {
        ctx.aciertos++; ctx.mem.idx_input++;
        Leds_PadOn(g->pad_id, C_VERDE_R, C_VERDE_G, C_VERDE_B);
        ctx.sub = M_HIT_OK; ctx.deadline = ahora_ms + M1_HIT_FLASH_MS;
    } else {
        ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B);
        ctx.sub = M_FAIL_FLASH; ctx.deadline = ahora_ms + M1_FAIL_MS;
    }
}

/* =======================================================
 * MODO 2: WHACK-A-MOLE SUAVE
 * ======================================================= */
static void Tick_M2(uint32_t ahora_ms) {
    if (ahora_ms < ctx.deadline) return;

    switch (ctx.sub) {
    case M2_INICIO:
        ctx.fza.pads_necesarios = (ctx.nivel==1) ? M2_PADS_N1 : (ctx.nivel==2) ? M2_PADS_N2 : M2_PADS_N3;
        ctx.fza.tiempo_pad = (ctx.nivel==1) ? M2_TIEMPO_N1 : (ctx.nivel==2) ? M2_TIEMPO_N2 : M2_TIEMPO_N3;
        ctx.fza.pads_golpeados = 0;
        FibraB_SendFrame(CMD_EVENTO_RT, ctx.modo_id, ctx.nivel, 0U);
        ctx.sub = M2_GENERAR; ctx.deadline = ahora_ms;
        break;

    case M2_GENERAR:
        ctx.fza.pad_obj = rand() % 4;
        Leds_AllOff(); 
        Leds_PadOn(ctx.fza.pad_obj, C_CYAN_R, C_CYAN_G, C_CYAN_B); /* Cyan indica que hay que darle SUAVE */
        ctx.objetivo_ms = ahora_ms; 
        ctx.sub = M2_ESPERA; ctx.deadline = ahora_ms + ctx.fza.tiempo_pad;
        break;

    case M2_ESPERA:
        /* Timeout = No le dio a tiempo = Fallo */
        ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B);
        ctx.sub = M2_FAIL_FLASH; ctx.deadline = ahora_ms + M1_FAIL_MS;
        break;

    case M2_HIT_OK:
        Leds_AllOff();
        if (ctx.fza.pads_golpeados >= ctx.fza.pads_necesarios) {
            ctx.nivel_alcanzado = ctx.nivel;
            Leds_AllOn(C_VERDE_R, C_VERDE_G, C_VERDE_B);
            if (ctx.nivel >= 3U) { ctx.sub = M2_WIN_FLASH; ctx.deadline = ahora_ms + M1_WIN_MS; } 
            else { ctx.sub = M2_NIVEL_OK; ctx.deadline = ahora_ms + M1_NIVEL_OK_MS; }
        } else {
            ctx.sub = M2_GENERAR; ctx.deadline = ahora_ms; 
        }
        break;

    case M2_NIVEL_OK: ctx.nivel++; ctx.sub = M2_INICIO; ctx.deadline = ahora_ms; break;
    case M2_WIN_FLASH: FinPartida(ctx.nivel_alcanzado, (uint8_t)ctx.aciertos, ahora_ms); break;
    case M2_FAIL_FLASH: FinPartida(ctx.nivel_alcanzado, ctx.fallos, ahora_ms); break;
    default: ctx.sub = M2_INICIO; ctx.deadline = ahora_ms; break;
    }
}

static void Golpe_M2(const EventoGolpe_t *g, uint32_t ahora_ms) {
    if (ctx.sub != M2_ESPERA || g->pad_id != ctx.fza.pad_obj) return; 
    FibraB_SendFrame(CMD_EVENTO_RT, g->pad_id, g->fuerza_pct, 0);

    /* NUEVA REGLA: Si la fuerza supera el 50%, falla. Solo valen golpes suaves. */
    if (g->fuerza_pct > 50U) {
        ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B);
        ctx.sub = M2_FAIL_FLASH; ctx.deadline = ahora_ms + M1_FAIL_MS;
    } else {
        ctx.aciertos++; ctx.fza.pads_golpeados++;
        Leds_PadOn(ctx.fza.pad_obj, C_VERDE_R, C_VERDE_G, C_VERDE_B);
        ctx.sub = M2_HIT_OK; ctx.deadline = ahora_ms + M1_HIT_FLASH_MS;
    }
}

/* =======================================================
 * MODO 3: INHIBICIÓN 
 * ======================================================= */
static void Tick_M3(uint32_t ahora_ms) {
    if (ctx.sub == M_GENERAR) {
        if (ctx.inh.necesarios == 0U) {
            ctx.inh.necesarios = M3_NECESARIOS[ctx.nivel - 1U];
            ctx.inh.aciertos_nivel = 0U;
            FibraB_SendFrame(CMD_EVENTO_RT, ctx.modo_id, ctx.nivel, 0U);
        }
        ctx.inh.pad_verde = (uint8_t)(rand() % 4U);
        ctx.inh.hay_doble = 0U; ctx.inh.es_rojo = 0U;
        ctx.inh.color_obj_idx = 1U + (rand() % 5U);

        if (ctx.nivel == 3U && (uint8_t)(rand() % 100U) < M3_PROB_DOBLE_PCT) {
            ctx.inh.hay_doble = 1U;
            do { ctx.inh.pad_rojo = (uint8_t)(rand() % 4U); } while (ctx.inh.pad_rojo == ctx.inh.pad_verde);
            Leds_PadOn(ctx.inh.pad_verde, PALETA_R[ctx.inh.color_obj_idx], PALETA_G[ctx.inh.color_obj_idx], PALETA_B[ctx.inh.color_obj_idx]);
            Leds_PadOn(ctx.inh.pad_rojo,  C_ROJO_R,  C_ROJO_G,  C_ROJO_B);
        } else if ((uint8_t)(rand() % 100U) < M3_PROB_ROJO_PCT) {
            ctx.inh.es_rojo = 1U; ctx.inh.pad_rojo = (uint8_t)(rand() % 4U); ctx.inh.pad_verde = 0xFFU; 
            Leds_AllOff(); Leds_PadOn(ctx.inh.pad_rojo, C_ROJO_R, C_ROJO_G, C_ROJO_B);
        } else {
            Leds_AllOff(); 
            Leds_PadOn(ctx.inh.pad_verde, PALETA_R[ctx.inh.color_obj_idx], PALETA_G[ctx.inh.color_obj_idx], PALETA_B[ctx.inh.color_obj_idx]);
        }
        ctx.objetivo_ms = ahora_ms; ctx.sub = M_ESTIMULO; ctx.deadline = ahora_ms + M3_VENTANA_MS[ctx.nivel - 1U];
        return;
    }

    if (ahora_ms < ctx.deadline) return;

    switch (ctx.sub) {
    case M_ESTIMULO:
        if (ctx.inh.es_rojo || ctx.inh.hay_doble == 0U) {
            if (ctx.inh.es_rojo) { ctx.aciertos++; ctx.inh.aciertos_nivel++; Leds_AllOff(); } 
            else { ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B); ctx.sub = M_FAIL_FLASH; ctx.deadline = ahora_ms + M3_FAIL_MS; break; }
        } else {
            ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B); ctx.sub = M_FAIL_FLASH; ctx.deadline = ahora_ms + M3_FAIL_MS; break;
        }

        if (ctx.inh.aciertos_nivel >= ctx.inh.necesarios) {
            ctx.nivel_alcanzado = ctx.nivel; Leds_AllOn(C_VERDE_R, C_VERDE_G, C_VERDE_B);
            if (ctx.nivel >= 3U) { ctx.sub = M_WIN_FLASH; ctx.deadline = ahora_ms + M3_WIN_MS; } 
            else { ctx.sub = M_NIVEL_OK; ctx.deadline = ahora_ms + M3_NIVEL_OK_MS; }
        } else { ctx.sub = M_GENERAR; ctx.deadline = ahora_ms; }
        break;

    case M_HIT_OK:
        Leds_AllOff();
        if (ctx.inh.aciertos_nivel >= ctx.inh.necesarios) {
            ctx.nivel_alcanzado = ctx.nivel; Leds_AllOn(C_VERDE_R, C_VERDE_G, C_VERDE_B);
            if (ctx.nivel >= 3U) { ctx.sub = M_WIN_FLASH; ctx.deadline = ahora_ms + M3_WIN_MS; } 
            else { ctx.sub = M_NIVEL_OK; ctx.deadline = ahora_ms + M3_NIVEL_OK_MS; }
        } else { ctx.sub = M_GENERAR; ctx.deadline = ahora_ms; }
        break;

    case M_NIVEL_OK: ctx.nivel++; ctx.inh.necesarios = 0U; ctx.inh.aciertos_nivel = 0U; Leds_AllOff(); ctx.sub = M_GENERAR; ctx.deadline = ahora_ms; break;
    case M_WIN_FLASH: FinPartida(ctx.nivel_alcanzado, (uint8_t)ctx.aciertos, ahora_ms); break;
    case M_FAIL_FLASH: FinPartida(ctx.nivel_alcanzado, ctx.fallos, ahora_ms); break;
    default: ctx.sub = M_GENERAR; ctx.deadline = ahora_ms; break;
    }
}

static void Golpe_M3(const EventoGolpe_t *g, uint32_t ahora_ms) {
    if (ctx.sub != M_ESTIMULO) return;
    FibraB_SendFrame(CMD_EVENTO_RT, g->pad_id, g->fuerza_pct, 0);

    if ((ctx.inh.hay_doble && g->pad_id == ctx.inh.pad_rojo) || (ctx.inh.es_rojo && g->pad_id == ctx.inh.pad_rojo)) {
        ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B); ctx.sub = M_FAIL_FLASH; ctx.deadline = ahora_ms + M3_FAIL_MS;
        return;
    }

    if (!ctx.inh.es_rojo && g->pad_id == ctx.inh.pad_verde) {
        ctx.aciertos++; ctx.inh.aciertos_nivel++;
        Leds_PadOn(g->pad_id, PALETA_R[ctx.inh.color_obj_idx], PALETA_G[ctx.inh.color_obj_idx], PALETA_B[ctx.inh.color_obj_idx]);
        ctx.sub = M_HIT_OK; ctx.deadline = ahora_ms + M3_HIT_OK_MS;
    }
}

/* =======================================================
 * MODO 4: SIMON INFINITO ACUMULATIVO
 * ======================================================= */
static void Tick_M4(uint32_t ahora_ms) {
    if (ahora_ms < ctx.deadline) return;

    switch (ctx.sub) {
    case M_GENERAR:
        if (ctx.simon.seq_len == 0) {
            /* Primera vez */
            ctx.simon.seq_len = 1;
            ctx.simon.secuencia[0] = (uint8_t)(rand() % 4U);
            ctx.simon.colores[0]   = (uint8_t)(rand() % 6U);
            ctx.simon.score_rondas = 0;
            FibraB_SendFrame(CMD_EVENTO_RT, ctx.modo_id, ctx.simon.seq_len, 0U);
        } else {
            /* Añadimos 1 pad más a la secuencia (si no pasamos del max) */
            if (ctx.simon.seq_len < M4_MAX_SEQ) {
                ctx.simon.secuencia[ctx.simon.seq_len] = (uint8_t)(rand() % 4U);
                ctx.simon.colores[ctx.simon.seq_len]   = (uint8_t)(rand() % 6U);
                ctx.simon.seq_len++;
            }
        }
        ctx.simon.idx_demo = 0U; ctx.simon.idx_input = 0U;
        Leds_AllOff();
        ctx.sub = M_PRE_PAUSA; ctx.deadline = ahora_ms + M1_PRE_PAUSA_MS;
        break;

    case M_PRE_PAUSA:
    case M_DEMO_OFF:
        if (ctx.sub == M_DEMO_OFF) ctx.simon.idx_demo++;
        
        if (ctx.simon.idx_demo >= ctx.simon.seq_len) {
            Leds_AllOn(C_AZUL_R, C_AZUL_G, C_AZUL_B);
            ctx.sub = M_TURNO_FLASH; ctx.deadline = ahora_ms + M1_TURNO_MS;
        } else {
            uint8_t color_idx = ctx.simon.colores[ctx.simon.idx_demo];
            Leds_PadOn(ctx.simon.secuencia[ctx.simon.idx_demo], PALETA_R[color_idx], PALETA_G[color_idx], PALETA_B[color_idx]);
            ctx.sub = M_DEMO_ON; ctx.deadline = ahora_ms + M4_DEMO_ON_MS;
        }
        break;

    case M_DEMO_ON:
        Leds_PadOff(ctx.simon.secuencia[ctx.simon.idx_demo]);
        ctx.sub = M_DEMO_OFF; ctx.deadline = ahora_ms + M4_DEMO_OFF_MS;
        break;

    case M_TURNO_FLASH:
        Leds_AllOff();
        ctx.sub = M_INPUT; ctx.objetivo_ms = ahora_ms; ctx.deadline = ahora_ms + M4_INPUT_TIMEOUT;
        break;

    case M_INPUT:
        ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B);
        ctx.sub = M_FAIL_FLASH; ctx.deadline = ahora_ms + M1_FAIL_MS;
        break;

    case M_HIT_OK:
        Leds_PadOff(ctx.simon.secuencia[ctx.simon.idx_input - 1U]);
        if (ctx.simon.idx_input >= ctx.simon.seq_len) {
            /* Ronda completada con éxito */
            ctx.simon.score_rondas++;
            Leds_AllOn(C_VERDE_R, C_VERDE_G, C_VERDE_B);
            ctx.sub = M_NIVEL_OK; ctx.deadline = ahora_ms + M1_NIVEL_OK_MS; 
        } else {
            ctx.sub = M_INPUT; ctx.deadline = ahora_ms + M4_INPUT_TIMEOUT;
        }
        break;

    case M_NIVEL_OK:
        Leds_AllOff(); ctx.sub = M_GENERAR; ctx.deadline = ahora_ms + 500U;
        break;

    case M_FAIL_FLASH: 
        /* Calculamos el nivel en base al score final */
        if (ctx.simon.score_rondas >= 15) ctx.nivel_alcanzado = 3;
        else if (ctx.simon.score_rondas >= 10) ctx.nivel_alcanzado = 2;
        else if (ctx.simon.score_rondas >= 5) ctx.nivel_alcanzado = 1;
        else ctx.nivel_alcanzado = 0;
        
        FinPartida(ctx.nivel_alcanzado, ctx.simon.score_rondas, ahora_ms); 
        break;
        
    default: ctx.sub = M_GENERAR; ctx.deadline = ahora_ms; break;
    }
}

static void Golpe_M4(const EventoGolpe_t *g, uint32_t ahora_ms) {
    if (ctx.sub != M_INPUT) return;
    FibraB_SendFrame(CMD_EVENTO_RT, g->pad_id, g->fuerza_pct, 0);

    if (g->pad_id == ctx.simon.secuencia[ctx.simon.idx_input]) {
        ctx.aciertos++; ctx.simon.idx_input++;
        Leds_PadOn(g->pad_id, C_VERDE_R, C_VERDE_G, C_VERDE_B);
        ctx.sub = M_HIT_OK; ctx.deadline = ahora_ms + M1_HIT_FLASH_MS;
    } else {
        ctx.fallos++; Leds_AllOn(C_ROJO_R, C_ROJO_G, C_ROJO_B);
        ctx.sub = M_FAIL_FLASH; ctx.deadline = ahora_ms + M1_FAIL_MS;
    }
}
