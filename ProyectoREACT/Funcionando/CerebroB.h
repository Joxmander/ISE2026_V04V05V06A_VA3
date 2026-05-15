/**
 ******************************************************************************
 * @file    CerebroB.h
 * @author  Jose Vargas Gonzaga
 * @brief   Cabecera del Orquestador del Nodo B (Terminal Aut?nomo R.E.A.C.T.)
 * Expone la funci?n principal del hilo que act?a como esclavo en mi 
 * arquitectura, reaccionando a los comandos de la Estaci?n Base.
 ******************************************************************************
 */

#ifndef CEREBRO_B_H
#define CEREBRO_B_H

// Prototipo de mi hilo principal (invocado desde app_main)
void Hilo_Orquestador_CerebroB(void *argument);

#endif /* CEREBRO_B_H */
