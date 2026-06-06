/**
 ******************************************************************************
 * @file    testCerebroB.h
 * @brief   Suite de tests de hardware del Nodo B.
 *
 *  Uso:
 *    Llamar a TestNodoB_Ejecutar() en CerebroB.c justo antes del bucle
 *    principal, compilando con MODO_TEST = 1.
 *    Cuando el test termina, el sistema cae en el bucle de producción normal.
 *
 *  Tests incluidos:
 *    1. LEDs    — arcoíris, anillos individuales (detección crosstalk), chase
 *    2. Ruido   — monitor de delta en reposo durante 10 s, diagnóstico
 *    3. Golpes  — test interactivo 30 s con feedback LED por zona de fuerza
 ******************************************************************************
 */

#ifndef TEST_CEREBRO_B_H
#define TEST_CEREBRO_B_H

/**
 * @brief Ejecuta la suite completa de tests.
 *        Bloqueante — tarda ~75 s en total (LEDs ~30 s, ruido 10 s, golpes 30 s).
 *        Al finalizar devuelve el control para que CerebroB entre en producción.
 */
void TestNodoB_Ejecutar(void);

#endif /* TEST_CEREBRO_B_H */
