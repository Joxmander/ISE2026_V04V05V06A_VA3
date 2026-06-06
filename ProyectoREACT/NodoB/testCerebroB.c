/**
 ******************************************************************************
 * @file    testCerebroB.c
 * @brief   Suite de tests de hardware del Nodo B — LEDs + Pads.
 *
 *  Secuencia:
 *    TEST 1 — LEDs
 *      1a. Arcoíris en todos los anillos (~20 s)
 *      1b. Anillos individuales × colores puros (R, G, B, Blanco) — detecta crosstalk
 *      1c. Chase cian  0?1?2?3  (3 pasadas)
 *      1d. Flash blanco de confirmación (3 destellos)
 *
 *    TEST 2 — Ruido en reposo
 *      Mide delta ADC sin tocar nada durante 10 s.
 *      Diagnóstico por pad: LIMPIO (<30) / ACEPTABLE (30-80) / RUIDOSO (>80).
 *      LED verde/amarillo/rojo por resultado. Pistas de causa si hay ruido alto.
 *
 *    TEST 3 — Golpes interactivos
 *      30 s esperando golpes en cualquier pad.
 *      Feedback LED por zona de fuerza: AZUL (<33 %) / VERDE (33-66 %) / ROJO (>66 %).
 *      Estadísticas finales por pad.
 ******************************************************************************
 */

#include "testCerebroB.h"
#include "LedsRGB.h"
#include "Pads.h"
#include "cmsis_os2.h"
#include <stdio.h>

/* -- Duraciones ----------------------------------------------------------- */
#define T_COLOR_MS       800U   /* Tiempo que se mantiene cada color en test individual */
#define T_RUIDO_S         10U   /* Segundos de monitorización de ruido */
#define T_GOLPES_S        30U   /* Segundos del test de golpes interactivo */

/* -- Umbrales de calidad del ruido --------------------------------------- */
#define RUIDO_LIMPIO      30U   /* delta máx aceptable en reposo perfecto */
#define RUIDO_ACEPTABLE   80U   /* por encima = problema de hardware */

/* ---------------------------------------------------------------------------
 * Helpers internos
 * --------------------------------------------------------------------------- */

static void separador(const char *titulo)
{
    printf("\r\n--- %s ---\r\n", titulo);
}

/* Enciende un solo anillo con el color dado, espera T_COLOR_MS y lo apaga */
static void color_anillo(uint8_t anillo, uint8_t r, uint8_t g, uint8_t b,
                          const char *nombre)
{
    LedsRGB_Clear();
    LedsRGB_FillAnillo(anillo, r, g, b);
    LedsRGB_Show();
    printf("    Anillo %u -> %-8s  (verifica: solo ese anillo debe brillar)\r\n",
           (unsigned)anillo, nombre);
    osDelay(T_COLOR_MS);
    LedsRGB_Clear();
    LedsRGB_Show();
    osDelay(200U);
}

/* ---------------------------------------------------------------------------
 * TEST 1 — DIAGNÓSTICO DE LEDs
 * --------------------------------------------------------------------------- */

static void Test1_LEDs(void)
{
    printf("\r\n");
    printf("+----------------------------------------------+\r\n");
    printf("|       TEST 1: DIAGNOSTICO DE LEDs            |\r\n");
    printf("+----------------------------------------------+\r\n");

    /* -- 1a. Arcoíris global ----------------------------------------------- */
    separador("1a. Arcoiris en todos los anillos");
    printf("  Duracion ~20 s. Todos los anillos deben rotar colores.\r\n");
    LedsRGB_ArcoirisAll(1U);
    printf("  OK\r\n");

    /* -- 1b. Anillos individuales ------------------------------------------- */
    separador("1b. Anillos individuales (buscar crosstalk)");
    printf("  IMPORTANTE: solo debe encenderse el anillo indicado.\r\n");
    printf("  Si otro anillo se ilumina tenuemente -> problema de hardware\r\n");
    printf("  (cortocircuito entre pines, solder bridge, PC6/PC7 adyacentes).\r\n\r\n");

    const char *etiqueta[4] = {
        "Set1(PB6)", "Set2(PD13)", "Set3(PC6)", "Set4(PC7)"
    };

    for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        printf("  [Anillo %u / %s]\r\n", (unsigned)a, etiqueta[a]);
        color_anillo(a, 200U,   0U,   0U, "ROJO");
        color_anillo(a,   0U, 200U,   0U, "VERDE");
        color_anillo(a,   0U,   0U, 200U, "AZUL");
        color_anillo(a, 160U, 160U, 160U, "BLANCO");
    }

    /* -- 1c. Chase cian 0?1?2?3 ------------------------------------------- */
    separador("1c. Chase cian (3 pasadas)");
    for (uint8_t rep = 0U; rep < 3U; rep++) {
        for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
            LedsRGB_Clear();
            LedsRGB_FillAnillo(a, 0U, 160U, 220U);
            LedsRGB_Show();
            osDelay(300U);
        }
    }
    LedsRGB_Clear();
    LedsRGB_Show();
    printf("  OK\r\n");

    /* -- 1d. Flash de confirmación ----------------------------------------- */
    separador("1d. Flash blanco de confirmacion (3x)");
    for (uint8_t f = 0U; f < 3U; f++) {
        for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
            LedsRGB_FillAnillo(a, 200U, 200U, 200U);
        }
        LedsRGB_Show();
        osDelay(150U);
        LedsRGB_Clear();
        LedsRGB_Show();
        osDelay(150U);
    }

    printf("\r\n  >>> TEST 1 COMPLETADO <<<\r\n");
}

/* ---------------------------------------------------------------------------
 * TEST 2 — MONITOR DE RUIDO EN REPOSO
 * --------------------------------------------------------------------------- */

static void Test2_RuidoReposo(void)
{
    printf("\r\n");
    printf("+----------------------------------------------+\r\n");
    printf("|     TEST 2: RUIDO DE PADS EN REPOSO          |\r\n");
    printf("+----------------------------------------------+\r\n");
    printf("  *** NO TOQUES NINGUN PAD DURANTE %u SEGUNDOS ***\r\n\r\n",
           (unsigned)T_RUIDO_S);

    /* LED azul muy tenue de "esperando" */
    for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        LedsRGB_FillAnillo(a, 0U, 0U, 12U);
    }
    LedsRGB_Show();

    uint32_t max_ruido[PADS_NUM_CANALES] = {0U};
    uint32_t sum_ruido[PADS_NUM_CANALES] = {0U};
    uint32_t muestras = 0U;

    uint32_t inicio      = osKernelGetTickCount();
    uint32_t ultimo_print = inicio;
    uint32_t fin         = inicio + (T_RUIDO_S * 1000U);

    while ((int32_t)(fin - osKernelGetTickCount()) > 0) {

        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
            uint16_t d = Pads_GetRawInstant(i);
            if (d > max_ruido[i]) { max_ruido[i] = d; }
            sum_ruido[i] += d;
        }
        muestras++;

        /* Imprimir línea de estado cada segundo */
        uint32_t ahora = osKernelGetTickCount();
        if ((ahora - ultimo_print) >= 1000U) {
            ultimo_print = ahora;
            uint32_t restante = (fin > ahora) ? (fin - ahora) / 1000U : 0U;
            printf("  t=%2us | PAD0=%-4u  PAD1=%-4u  PAD2=%-4u  PAD3=%-4u\r\n",
                   (unsigned)restante,
                   (unsigned)Pads_GetRawInstant(0U),
                   (unsigned)Pads_GetRawInstant(1U),
                   (unsigned)Pads_GetRawInstant(2U),
                   (unsigned)Pads_GetRawInstant(3U));
        }

        osDelay(50U);
    }

    /* -- Tabla de resultados ----------------------------------------------- */
    printf("\r\n  === RESULTADO RUIDO EN REPOSO ===\r\n");
    printf("  %-6s  %-8s  %-8s  %s\r\n", "PAD", "MAX", "MEDIA", "ESTADO");
    printf("  %-6s  %-8s  %-8s  %s\r\n", "------", "--------", "-----", "------");

    uint8_t hay_problema = 0U;

    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        uint32_t media = (muestras > 0U) ? (sum_ruido[i] / muestras) : 0U;
        const char *estado;
        uint8_t r = 0U, g = 0U, b = 0U;

        if (max_ruido[i] < RUIDO_LIMPIO) {
            estado = "LIMPIO";       g = 160U;
        } else if (max_ruido[i] < RUIDO_ACEPTABLE) {
            estado = "ACEPTABLE";   r = 200U; g = 140U;
        } else {
            estado = "RUIDOSO !";   r = 200U; hay_problema = 1U;
        }

        printf("  PAD%u    %-8u  %-8u  %s\r\n",
               (unsigned)i, (unsigned)max_ruido[i],
               (unsigned)media, estado);

        LedsRGB_FillAnillo(i, r, g, b);
    }
    LedsRGB_Show();

    if (hay_problema) {
        printf("\r\n  [!] Ruido alto detectado. Causas mas comunes:\r\n");
        printf("      1. Sensor VL53L0X con VCC conectado pero sin GND.\r\n");
        printf("         -> Conectar GND o desconectar VCC por completo.\r\n");
        printf("      2. Circuito de fibra optica introduciendo ruido\r\n");
        printf("         en el rail de 3.3V (desconectar y repetir el test).\r\n");
        printf("      3. Condensador de desacoplo del TLC274 demasiado\r\n");
        printf("         lejos del IC o con mala soldadura.\r\n");
    } else {
        printf("\r\n  [OK] Nivel de ruido dentro de lo esperado.\r\n");
    }

    osDelay(3000U);
    LedsRGB_Clear();
    LedsRGB_Show();

    printf("\r\n  >>> TEST 2 COMPLETADO <<<\r\n");
}

/* ---------------------------------------------------------------------------
 * TEST 3 — GOLPES INTERACTIVOS
 * --------------------------------------------------------------------------- */

static void Test3_Golpes(void)
{
    printf("\r\n");
    printf("+----------------------------------------------+\r\n");
    printf("|       TEST 3: DETECCION DE GOLPES            |\r\n");
    printf("+----------------------------------------------+\r\n");
    printf("  Golpea cada pad al menos 1 vez en %u s.\r\n", (unsigned)T_GOLPES_S);
    printf("  Color LED por zona de fuerza:\r\n");
    printf("    AZUL   = Suave  (<33 %%)\r\n");
    printf("    VERDE  = Medio  (33-66 %%)\r\n");
    printf("    ROJO   = Fuerte (>66 %%)\r\n\r\n");

    /* LED azul tenue de "esperando" en todos los anillos */
    for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
        LedsRGB_FillAnillo(a, 0U, 0U, 12U);
    }
    LedsRGB_Show();

    uint16_t golpes_pad[PADS_NUM_CANALES]     = {0U};
    uint16_t fuerza_max_pad[PADS_NUM_CANALES] = {0U};
    uint32_t tick_apagar[PADS_NUM_CANALES]    = {0U};
    uint8_t  led_on[PADS_NUM_CANALES]         = {0U};

    uint32_t inicio = osKernelGetTickCount();
    uint32_t fin    = inicio + (T_GOLPES_S * 1000U);
    uint32_t ahora;

    while ((int32_t)(fin - (ahora = osKernelGetTickCount())) > 0) {

        Pads_Poll();

        for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {

            /* Apagar LED de feedback cuando haya pasado el tiempo */
            if (led_on[i] && (ahora >= tick_apagar[i])) {
                LedsRGB_FillAnillo(i, 0U, 0U, 12U);  /* volver a azul tenue */
                LedsRGB_Show();
                led_on[i] = 0U;
            }

            if (!Pads_HayGolpe(i)) { continue; }

            uint8_t  fuerza = Pads_GetFuerza(i);
            uint16_t delta  = Pads_GetRawDelta(i);

            golpes_pad[i]++;
            if (fuerza > fuerza_max_pad[i]) { fuerza_max_pad[i] = fuerza; }

            /* Color y etiqueta según zona de fuerza */
            uint8_t r = 0U, g = 0U, b = 0U;
            const char *zona;
            if (fuerza < 33U) {
                b = 200U; zona = "SUAVE  ";
            } else if (fuerza < 67U) {
                g = 200U; zona = "MEDIO  ";
            } else {
                r = 200U; zona = "FUERTE!";
            }

            LedsRGB_FillAnillo(i, r, g, b);
            LedsRGB_Show();
            tick_apagar[i] = ahora + 350U;
            led_on[i]      = 1U;

            uint32_t restante = (fin > ahora) ? (fin - ahora) / 1000U : 0U;
            printf("  [t=%2us] PAD%u | delta=%-4u | fuerza=%-3u%% | %s\r\n",
                   (unsigned)restante, (unsigned)i,
                   (unsigned)delta, (unsigned)fuerza, zona);
        }

        osDelay(5U);
    }

    /* -- Estadísticas finales ---------------------------------------------- */
    printf("\r\n  === ESTADISTICAS DE GOLPES ===\r\n");
    printf("  %-6s  %-8s  %-12s  %s\r\n", "PAD", "GOLPES", "MAX_FUERZA%%", "ESTADO");
    printf("  %-6s  %-8s  %-12s  %s\r\n", "------", "------", "-----------", "------");

    uint8_t todos_ok = 1U;
    for (uint8_t i = 0U; i < PADS_NUM_CANALES; i++) {
        const char *estado;
        uint8_t r = 0U, g = 0U, b = 0U;

        if (golpes_pad[i] > 0U) {
            estado = "OK";         g = 160U;
        } else {
            estado = "SIN GOLPES"; r = 200U; b = 100U;
            todos_ok = 0U;
        }

        printf("  PAD%u    %-8u  %-12u  %s\r\n",
               (unsigned)i, (unsigned)golpes_pad[i],
               (unsigned)fuerza_max_pad[i], estado);

        LedsRGB_FillAnillo(i, r, g, b);
    }
    LedsRGB_Show();

    if (todos_ok) {
        printf("\r\n  [OK] Todos los pads respondieron correctamente.\r\n");
    } else {
        printf("\r\n  [!] Uno o mas pads no recibieron golpe.\r\n");
        printf("      Verifica soldadura del piezo y conexion al ADC.\r\n");
    }

    osDelay(3000U);
    LedsRGB_Clear();
    LedsRGB_Show();

    printf("\r\n  >>> TEST 3 COMPLETADO <<<\r\n");
}

/* ---------------------------------------------------------------------------
 * PUNTO DE ENTRADA PRINCIPAL
 * --------------------------------------------------------------------------- */

void TestNodoB_Ejecutar(void)
{
    printf("\r\n");
    printf("+================================================+\r\n");
    printf("|  SUITE DE TEST HARDWARE — NODO B  R.E.A.C.T   |\r\n");
    printf("|  Modulos: LEDs + Pads                          |\r\n");
    printf("+================================================+\r\n\r\n");

    Test1_LEDs();
    Test2_RuidoReposo();
    Test3_Golpes();

    /* Flash verde de victoria */
    for (uint8_t f = 0U; f < 5U; f++) {
        for (uint8_t a = 0U; a < WS2812_NUM_ANILLOS; a++) {
            LedsRGB_FillAnillo(a, 0U, 200U, 0U);
        }
        LedsRGB_Show();
        osDelay(150U);
        LedsRGB_Clear();
        LedsRGB_Show();
        osDelay(150U);
    }

    printf("\r\n");
    printf("+================================================+\r\n");
    printf("|    SUITE COMPLETA. Pasando a produccion...     |\r\n");
    printf("+================================================+\r\n\r\n");
}
