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
//---- Configuration for Hardware Versions -------
#define HARDWARE_VERSION_1_0


#define SOFTWARE_VERSION_1_0
// #define SOFTWARE_VERSION_1_1

//---- Features Configuration ----------------


//------ Configuration for Firmware-Channels -----


//---- End of Features Configuration ----------



//--- Hardware Welcome Code ------------------//
#ifdef HARDWARE_VERSION_1_0
#define HARD "Hardware Version: 1.0\n"
#endif
#ifdef HARDWARE_VERSION_2_0
#define HARD "Hardware Version: 2.0\n"
#endif

//--- Software Welcome Code ------------------//
#ifdef SOFTWARE_VERSION_1_2
#define SOFT "Software Version: 1.2\n"
#endif
#ifdef SOFTWARE_VERSION_1_1
#define SOFT "Software Version: 1.1\n"
#endif
#ifdef SOFTWARE_VERSION_1_0
#define SOFT "Software Version: 1.0\n"
#endif

//-------- Configuration for Outputs-Channels -----


//---- Configuration for Firmware-Programs --------


//-------- Configuration for Outputs-Firmware ------


//-- End Of Defines For Configuration ---------------

//GPIOA pin0
//GPIOA pin1
//GPIOA pin2    3 ADC channels

//GPIOA pin3
//GPIOA pin4    NC

//GPIOA pin5
//GPIOA pin6    2 ADC channels

//GPIOA pin7

//GPIOB pin0
#define LED ((GPIOB->ODR & 0x0001) != 0)
#define LED_ON GPIOB->BSRR = 0x00000001
#define LED_OFF GPIOB->BSRR = 0x00010000

//GPIOB pin1    JUMPER NO GEN
#define JUMPER_NO_GEN ((GPIOB->IDR & 0x0002) == 0)

//GPIOA pin8    
//GPIOA pin9
//GPIOA pin10    
//GPIOA pin11   
//GPIOA pin12
//GPIOA pin13
//GPIOA pin14    
//GPIOA pin15    

//GPIOB pin3     
//GPIOB pin4

//GPIOB pin5     TIM3_CH2
#define CTRL_BOOST ((GPIOB->ODR & 0x0020) != 0)
#define CTRL_BOOST_ON GPIOB->BSRR = 0x00000020     //esto es estado alto Hi-z
#define CTRL_BOOST_OFF GPIOB->BSRR = 0x00200000    //esto es estado bajo 0V

//GPIOB pin6
//GPIOB pin7    NC

//ESTADOS DEL PROGRAMA PRINCIPAL
typedef enum {
    INIT = 0,
    STAND_BY,
    TO_GEN,
    GENERATING,
    TO_STAND_BY,
    LOW_BAT,
    OVERCURRENT
} main_state_t;
          

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
#define LED_LOW_VOLTAGE               3
#define LED_PROTECTED                 4
#define LED_VIN_ERROR                 5
#define LED_OVERCURRENT_ERROR         6

//---- ADC configurations ----//
#define ADC_CH_QUANTITY        4
#define Vbatt_Sense      adc_ch[0]
#define Boost_Sense    adc_ch[1]
#define Vout_Sense     adc_ch[2]
#define Vmains_Sense    adc_ch[3]

/* Module Functions ------------------------------------------------------------*/
void ChangeLed (unsigned char);
void UpdateLed (void);

#endif /* HARD_H_ */