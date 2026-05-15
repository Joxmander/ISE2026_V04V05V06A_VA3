#include "spz.h"

/*----------------------------------------------------------------------------
 *      Thread 1 'Thread_Name': Sample thread
 *---------------------------------------------------------------------------*/
 
osThreadId_t tid_spz;                        // thread id
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc3;
osMessageQueueId_t id_cola_fuerza;    //Cola con el valor del golpe del piezo
osMessageQueueId_t id_cola_eventos;

Juego_t juego;

void Th_spz (void *argument);                   // thread function
void init_pins(void);
void init_ADC1(void);
void init_ADC3(void);

uint16_t piezo_raw[4] = {0};
uint16_t piezo_filtered[4] = {0};
uint16_t piezo_peak[4] = {0};

uint8_t pads_inhibidos[4] = {0,0,0,0};


/*HILO TEST*/
void Th_test_spz(void *argument);
int Init_Th_test_spz(void);
uint8_t test_pad;
uint16_t test_fuerza;
uint16_t test_fuerza_filtrada[4];

//const osThreadAttr_t spz_attr = {
//    .stack_size = 512
//};


int Init_Th_piezo (void) {
  
  init_pins();
  init_ADC1();
  init_ADC3();
  

  
  tid_spz = osThreadNew(Th_spz, NULL, NULL);
  if (tid_spz == NULL) {
    return(-1);
  }
  
  id_cola_fuerza = osMessageQueueNew(4, sizeof(FuerzaPiezos_t), NULL);
  id_cola_eventos = osMessageQueueNew(10, sizeof(EventoGolpe_t), NULL);
  
  
  return(0);
}
 
void Th_spz (void *argument) {
  
  const uint16_t threshold = 150;       //Ajuste segun el sensor  Umbral minimo que debe superar la se?al del piezo para considerar que ha habido un golpe
  const float alpha = 0.2f;            //Filtro IIR
  
  
  juego.esperando_input = 1;
  juego.proximidad_activa = 1;
  pads_inhibidos[0] = 0;
  
  while (1) {
    
    /* ----------------------------- */
    /* Lectura de los 4 piezos       */
    /* ----------------------------- */
    
    /* Lectura de PC3 (ADC1) */
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10); ////Porq 10 
    piezo_raw[0] = HAL_ADC_GetValue(&hadc1);
    

    /* Lectura de los 3 piezos conectados a ADC3 */
    HAL_ADC_Start(&hadc3);   // Iniciar secuencia de 3 conversiones

    // Rank 1 - PF10 - ADC3_IN8 - piezo_raw[1]
    HAL_ADC_PollForConversion(&hadc3, 10);
    piezo_raw[1] = HAL_ADC_GetValue(&hadc3);

    // Rank 2 - PF3 - ADC3_IN9  piezo_raw[2]
    HAL_ADC_PollForConversion(&hadc3, 10);
    piezo_raw[2] = HAL_ADC_GetValue(&hadc3);

    // Rank 3 - PF5 - ADC3_IN15 - piezo_raw[3]
    HAL_ADC_PollForConversion(&hadc3, 10);
    piezo_raw[3] = HAL_ADC_GetValue(&hadc3);
    
    /*Filtrado y deteccion de picos*/
    for(int j=0; j < 4; j++){
      
      //Filtro IIR
      piezo_filtered[j] = (uint16_t)(alpha*piezo_raw[j] + (1-alpha)*piezo_filtered[j]);
      
    }
// Enviar fuerza filtrada a la cola de fuerza (para test/CGI)
      FuerzaPiezos_t fuerza_msg;
      
      for(int x = 0; x < 4; x++){
        
        fuerza_msg.fuerza[x] = piezo_filtered[x];
        
      }
      osMessageQueuePut(id_cola_fuerza, &fuerza_msg, 0, 0);

      //Deteccion de golpe 
      for(int j=0; j < 4; j++){
        
        if(piezo_filtered[j] > threshold){
          
          if (juego.esperando_input && juego.proximidad_activa){
            
            EventoGolpe_t ev_golpe;
            ev_golpe.sensor_id = j;
            
            if (pads_inhibidos[j])
              ev_golpe.fuerza = 0xFFFF;  // golpe prohibido o 50000
            else
              ev_golpe.fuerza = piezo_filtered[j];
            
            osMessageQueuePut(id_cola_eventos, &ev_golpe, 0, 0);
          }
        }
      }
          osDelay(5);
    }
    

//    ; // Insert thread code here...
//    osThreadYield();                            // suspend thread
}

void init_pins(void){
  
  GPIO_InitTypeDef GPIO_InitStruct;
  
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  
  /*PC3 -- ADC1_IN13 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  
  
  /*PF3 -- ADC3_IN9 */  
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  
  /*PF5 -- ADC3_IN15 */  
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  
  /*PF10 -- ADC3_IN8 */  
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
  
}

void init_ADC1(void){
  
  __HAL_RCC_ADC1_CLK_ENABLE();
  
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;  //porq de 12 bits
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  
  HAL_ADC_Init(&hadc1);
  
  ADC_ChannelConfTypeDef sConfig = {0};
  sConfig.Channel= ADC_CHANNEL_13;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;   //Porq no usar menos ciclos
  
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);
  
}


void init_ADC3(void){

    __HAL_RCC_ADC3_CLK_ENABLE();

    hadc3.Instance = ADC3;
    hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc3.Init.Resolution = ADC_RESOLUTION_12B;
    hadc3.Init.ScanConvMode = ENABLE;       // Para leer varios canales
    hadc3.Init.ContinuousConvMode = DISABLE;
    hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc3.Init.NbrOfConversion = 3;

    HAL_ADC_Init(&hadc3);

    ADC_ChannelConfTypeDef sConfig = {0};

    /* PF10 -- ADC3_IN8 */
    sConfig.Channel = ADC_CHANNEL_8;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_144CYCLES;
    HAL_ADC_ConfigChannel(&hadc3, &sConfig);

    /* PF3 -- ADC3_IN9 */
    sConfig.Channel = ADC_CHANNEL_9;
    sConfig.Rank = 2;
    //sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    HAL_ADC_ConfigChannel(&hadc3, &sConfig);

    /* PF5 -- ADC3_IN15 */
    sConfig.Channel = ADC_CHANNEL_15;
    sConfig.Rank = 3;
    //sConfig.SamplingTime = ADC_SAMPLETIME_56CYCLES;
    HAL_ADC_ConfigChannel(&hadc3, &sConfig);
    
    
}



/****************************************/
osThreadId_t tid_test_spz;


int Init_Th_test_spz(void)
{
    tid_test_spz = osThreadNew(Th_test_spz, NULL, NULL);

    if (tid_test_spz == NULL)
        return -1;

    return 0;
}

void Th_test_spz(void *argument){
    EventoGolpe_t ev;
    FuerzaPiezos_t fuerza_msg;

    while(1)
    {
        // Leer fuerza filtrada
        if (osMessageQueueGet(id_cola_fuerza, &fuerza_msg, NULL, 0) == osOK)
        {
            for(int i = 0; i < 4; i++)
                test_fuerza_filtrada[i] = fuerza_msg.fuerza[i];
        }

        // Leer eventos de golpe
        if (osMessageQueueGet(id_cola_eventos, &ev, NULL, 0) == osOK)
        {
            test_pad = ev.sensor_id;
            test_fuerza = ev.fuerza; // 0xFFFF si golpe prohibido    punto de ruptura para mirar el golpe detectado
        }

        osDelay(5);
    }
}
