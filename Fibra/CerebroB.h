/**
 ******************************************************************************
 * @file    CerebroB.h
 * @author  Jose Vargas Gonzaga
 * @brief   Cabecera del Orquestador del Nodo B (Terminal Autónomo R.E.A.C.T.)
 * Expone la función principal del hilo que actúa como esclavo en mi 
 * arquitectura, reaccionando a los comandos de la Estación Base.
 ******************************************************************************
 */

#ifndef CEREBRO_B_H
#define CEREBRO_B_H

// Prototipo de mi hilo principal (invocado desde app_main)
void Hilo_Orquestador_CerebroB(void *argument);

#endif /* CEREBRO_B_H */
