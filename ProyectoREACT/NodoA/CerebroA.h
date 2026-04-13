/**
 ******************************************************************************
 * @file    CerebroA.h
 * @author  Jose Vargas Gonzaga
 * @brief   Cabecera del Orquestador de la Estación Base R.E.A.C.T.
 * Expone la función principal del hilo que actúa como "Director de Orquesta"
 * para gestionar la red, la interfaz y la fibra óptica.
 ******************************************************************************
 */

#ifndef CEREBRO_A_H
#define CEREBRO_A_H

// Prototipo de mi hilo principal (invocado en app_main)
void Hilo_Orquestador_Cerebro(void *argument);

#endif /* CEREBRO_A_H */
