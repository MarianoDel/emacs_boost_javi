//---------------------------------------------
// ##
// ## @Author: Med
// ## @Editor: Emacs - ggtags
// ## @TAGS:   Global
// ##
// #### HARD.H #################################
//---------------------------------------------

#ifndef _HARD_H_
#define _HARD_H_

#include "stm32f0xx.h"


//-- Defines For Configuration -------------------
//---- Configuration - Program Versions -------
// #define BOOST_JAVI    //boost desde 12V generealmente con control de 1-10V
#define BOOST_BACKUP_TECNOCOM    //boost desde 12V con fuente (700mA out) o bateria (200mA out)


//---- Features Configuration ----------------
#ifdef BOOST_JAVI
// #define AUTOMATIC_CTRL
// #define WITH_POTE_CTRL
#define FIXED_CTRL
#endif


#define LED_SHOW_STATUS
// #define LED_SHOW_INTERNAL_VALUES

//------ Hardware Config -----
#define ONE_RSENSE
// #define TWO_RSENSE


//---- End of Features Configuration ----------

//---- Voltage Measurements Settings ----------
#define VOUT_SETPOINT    OUT_40V
#define MAX_VOUT         OUT_45V
#define VOUT_FOR_SOFT_START    OUT_24V

#define IOUT_SETPOINT    IOUT_700MA
#define IOUT_SP_BATTERY    IOUT_200MA
#define IOUT_MIN         IOUT_007MA

#define MIN_INPUT_VOLTAGE    IN_9_5V
#define MAX_INPUT_VOLTAGE    IN_20V

#define MAINS_LOW_VOLTAGE    IN_9_5V
#define MAINS_HIGH_VOLTAGE    IN_20V
#define MAINS_TO_RECONNECT_VOLTAGE    IN_11_5V

#define BATTERY_MIN_VOLTAGE    IN_9_5V
#define BATTERY_MAX_VOLTAGE    IN_20V
#define BATTERY_TO_RECONNECT_VOLTAGE    IN_11_5V

//tensionde entrada dividido 13
#define IN_9_5V     215    //son 9V en el sensor
#define IN_11_5V    262    //son 11V en el sensor
#define IN_16V      382
#define IN_20V      477 

//tension de salida dividido 23
#define OUT_24V      323
#define OUT_35V      471
#define OUT_40V      539
#define OUT_45V      606

#ifdef TWO_RSENSE
//corriente de salida por 1.49V/A
#define IOUT_007MA    33   
#define IOUT_350MA    161
#define IOUT_700MA    323
#define IOUT_1000MA   461
#endif

#ifdef ONE_RSENSE
//corriente de salida por 2.8V/A
#define IOUT_007MA    66    
#define IOUT_200MA    175
#define IOUT_350MA    305
#define IOUT_700MA    611
#define IOUT_1000MA   872
#endif

#define ONE_TEN_ON    25
#define ONE_TEN_OFF   16


//-------- Configuration for Outputs-Channels -----


//---- Configuration for Firmware-Programs --------


//-------- Configuration for Outputs-Firmware ------


//-- End Of Defines For Configuration ---------------

//GPIOA pin0    Boost_Sense
//GPIOA pin1    Vin_Sense (JAVI) VBat_Sense (TECNOCOM)
//GPIOA pin2    Iout_Sense -- 3 ADC channels

//GPIOA pin3
//GPIOA pin4    NC

//GPIOA pin5    Vout_Sense
//GPIOA pin6    One_Ten_Pote (JAVI) Vmains_Sense (TECNOCOM) -- 2 ADC channels

//GPIOA pin7    NC

//GPIOB pin0
#define LED ((GPIOB->ODR & 0x0001) != 0)
#define LED_ON GPIOB->BSRR = 0x00000001
#define LED_OFF GPIOB->BSRR = 0x00010000

//GPIOB pin1    
#define STOP_JUMPER ((GPIOB->IDR & 0x0002) == 0)

//GPIOA pin8    
//GPIOA pin9
//GPIOA pin10    
//GPIOA pin11   
//GPIOA pin12
//GPIOA pin13
//GPIOA pin14    
//GPIOA pin15    NC

//GPIOB pin3     
//GPIOB pin4    NC

//GPIOB pin5     TIM3_CH2
#define CTRL_BOOST ((GPIOB->ODR & 0x0020) != 0)
#define CTRL_BOOST_ON GPIOB->BSRR = 0x00000020     //esto es estado alto Hi-z
#define CTRL_BOOST_OFF GPIOB->BSRR = 0x00200000    //esto es estado bajo 0V

//GPIOB pin6
//GPIOB pin7    NC


//ESTADOS DEL PROGRAMA PRINCIPAL
#ifdef BOOST_JAVI
typedef enum {
    INIT = 0,
    STAND_BY,
    GENERATING,
    LOW_INPUT,
    HIGH_INPUT,
    OVERCURRENT,
    JUMPER_PROTECTED,
    JUMPER_PROTECT_OFF
    
} main_state_t;
#endif

#ifdef BOOST_BACKUP_TECNOCOM
typedef enum {
    INIT = 0,
    STAND_BY,
    SOFT_START_FROM_MAINS,
    GENERATING_FROM_MAINS,
    SOFT_START_FROM_BATTERY,
    GENERATING_FROM_BATTERY,
    OVERCURRENT,
    JUMPER_PROTECTED,
    JUMPER_PROTECT_OFF
    
} main_state_t;
#endif

//ESTADOS DEL LED
typedef enum
{    
    START_BLINKING = 0,
    WAIT_TO_OFF,
    WAIT_TO_ON,
    WAIT_NEW_CYCLE
} led_state_t;


//Estados Externos de LED BLINKING
#define LED_NO_BLINKING               0
#define LED_STANDBY                   1
#define LED_GENERATING                2
#define LED_GENERATING_FROM_BATTERY   3
#define LED_LOW_VOLTAGE               4
#define LED_HIGH_VOLTAGE              5
#define LED_JUMPER_PROTECTED          6
#define LED_OVERCURRENT_ERROR         7

#define LED_GENERATING_FROM_MAINS  LED_GENERATING

/* Module Functions ------------------------------------------------------------*/
void ChangeLed (unsigned char);
void UpdateLed (void);

#endif /* HARD_H_ */
