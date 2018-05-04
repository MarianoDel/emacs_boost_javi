//---------------------------------------------
// #### PROYECTO BOOST TECNOCOM - Custom Board ####
// ##
// ## @Author: Med
// ## @Editor: Emacs - ggtags
// ## @TAGS:   Global
// ##
// #### MAIN.C ################################
//---------------------------------------------

/* Includes ------------------------------------------------------------------*/
#include "hard.h"
#include "stm32f0xx.h"
#include "gpio.h"
#include "uart.h"

#include "core_cm0.h"
#include "adc.h"
#include "tim.h"
#include "dsp.h"

//#include <stdio.h>
//#include <string.h>




//--- VARIABLES EXTERNAS ---//
// ------- Externals del ADC -------
volatile unsigned short adc_ch [ADC_CH_QUANTITY];
volatile unsigned char seq_ready;

// ------- Externals de los timers -------
volatile unsigned short timer_led = 0;
volatile unsigned short timer_standby;
volatile unsigned short timer_filters;
volatile unsigned short wait_ms_var = 0;


// ------- Definiciones para los filtros -------
#define SIZEOF_FILTER    8
#define UNDERSAMPLING_TICKS    5
unsigned short vmains [SIZEOF_FILTER];
unsigned short vbatt [SIZEOF_FILTER];
unsigned short iboost [SIZEOF_FILTER];
short d = 0;
short ez1 = 0;
short ez2 = 0;

#define IN_6V        335
#define IN_10V       559
#define IN_11V       614
#define IN_12V       670
#define IN_16V       894

#define BAT_9_5V     531
#define BAT_11_5V    643

#define OUT_35V      319
#define OUT_40V      364
#define OUT_45V      410
#define OUT_50V      455
#define OUT_80V      747    //ajustada 13-4-18

//--- VARIABLES GLOBALES ---//


// ------- de los timers -------

//--- FUNCIONES DEL MODULO ---//
void TimingDelay_Decrement(void);



//-------------------------------------------//
// @brief  Main program.
// @param  None
// @retval None
//------------------------------------------//
int main(void)
{
    unsigned short i = 0;
    main_state_t main_state = INIT;
    unsigned char protected = 0;
    unsigned char calc_filters = 0;
    unsigned char undersampling = 0;
    unsigned short curr_filtered = 0;
    unsigned short batt_filtered = 0;
    unsigned short mains_filtered = 0;
        

    //GPIO Configuration.
    GPIO_Config();

    //TIM Configuration.
    // TIM_3_Init();
    // TIM_14_Init();

    //ACTIVAR SYSTICK TIMER
    if (SysTick_Config(48000))
    {
        while (1)	/* Capture error */
        {
            if (LED)
                LED_OFF;
            else
                LED_ON;

            for (i = 0; i < 255; i++)
            {
                asm (	"nop \n\t"
                        "nop \n\t"
                        "nop \n\t" );
            }
        }
    }

    //prueba led y wait
    // while (1)
    // {
    //     if (LED)
    //         LED_OFF;
    //     else
    //         LED_ON;

    //     Wait_ms(1000);
    // }
    //fin prueba led y wait

    //prueba modulo adc.c tim.c e int adc
    // TIM_3_Init();
    // Update_TIM3_CH2 (25);

    // AdcConfig();
    // ADC1->CR |= ADC_CR_ADSTART;
    
    // while (1)
    // {
    //     if (seq_ready)
    //     {
    //         seq_ready = 0;
    //         if (LED)
    //             LED_OFF;
    //         else
    //             LED_ON;
    //         // LED_OFF;
    //     }
    // }               
    //fin prueba modulo adc.c tim.c e int adc
    TIM_3_Init();
    Update_TIM3_CH2 (0);

    AdcConfig();
    ADC1->CR |= ADC_CR_ADSTART;
    ChangeLed(LED_STANDBY);
    RELAY_ON;

    timer_standby = 1000;
        
    while (1)
    {
        switch (main_state)
        {
            case INIT:
                if (!timer_standby)
                {
                    
                    main_state++;
                }
                break;

            case STAND_BY:    //tengo 220V
                // if (mains_filtered < IN_16V)    //tengo baja tension de 220, me paso a bateria
                if (mains_filtered < IN_11V)    //tengo baja tension de 220, me paso a bateria
                {
                    main_state = TO_GEN;
                    RELAY_OFF;
                    timer_standby = 100;
                }
                break;

            case TO_GEN:
                if (!timer_standby)
                {
                    //paso a generar
                    main_state = GENERATING;
                    ChangeLed(LED_GENERATING);
                    d = 0;
                    ez1 = 0;
                    ez2 = 0;
                }
                break;

            case GENERATING:
                if (!protected)
                {
                    if (!JUMPER_NO_GEN)
                    {              
                        if (seq_ready)
                        {
                            seq_ready = 0;
                            if (undersampling < UNDERSAMPLING_TICKS)
                                undersampling++;
                            else
                            {

                                // d = PID_roof (OUT_35V, Vout_Sense, d, &ez1, &ez2);
                                d = PID_roof (OUT_80V, Vout_Sense, d, &ez1, &ez2);
                                // d = PID_roof (OUT_50V, Vout_Sense, d, &ez1, &ez2);                                
                                if (d < 0)
                                    d = 0;

                                if (d > DUTY_90_PERCENT)	//no pasar del 90%
                                    d = DUTY_90_PERCENT;

                                Update_TIM3_CH2 (d);
                            }
                        }    //cierra sequence
                    }    //cierra jumper protected
                    else
                    {
                        //me piden que no envie senial y proteja
                        Update_TIM3_CH2 (0);
                        protected = 1;
                        ChangeLed(LED_PROTECTED);
                    }
                }    //cierra variable protect
                else
                {
                    //estoy protegido reviso si tengo que salir
                    if (!JUMPER_NO_GEN)
                    {
                        protected = 0;
                        ChangeLed(LED_GENERATING);
                        d = 0;
                        ez1 = 0;
                        ez2 = 0;
                    }
                }

                //reviso si vuelven los 220
                if (mains_filtered > IN_12V)
                {
                    //dejo de generar y vuelvo a 220
                    Update_TIM3_CH2(0);
                    ChangeLed(LED_STANDBY);
                    main_state = TO_STAND_BY;
                    timer_standby = 500;
                }

                //reviso si esta ya muy baja la bateria
                if (batt_filtered < BAT_9_5V)
                {
                    Update_TIM3_CH2(0);
                    ChangeLed(LED_LOW_VOLTAGE);
                    main_state = LOW_BAT;
                }
                break;

            case TO_STAND_BY:
                if (!timer_standby)
                {
                    RELAY_ON;
                    main_state = STAND_BY;
                }
                break;

            case LOW_BAT:
                if (batt_filtered > BAT_11_5V)
                {
                    //si tengo 220 vuelvo al principal
                    if (mains_filtered > IN_11V)
                        main_state = TO_STAND_BY;
                    else
                        main_state = TO_GEN;
                }                
                break;

            case OVERCURRENT:
                break;
                
            default:
                break;
        }
        
        // if (seq_ready)
        // {
        //     seq_ready = 0;
        //     // if (LED)
        //         // LED_OFF;
        //     // else
        //         // LED_ON;
        //     LED_OFF;
        // }
        if (!timer_filters)
        {
            vmains[0] = Vmains_Sense;
            vbatt[0] = Vbatt_Sense;
            iboost[0] = Boost_Sense;
            calc_filters = 1;
            timer_filters = 100;
        }

        switch (calc_filters)    //distribuyo filtros en varios pasos
        {
            case 1:
                mains_filtered = MAFilter8(vmains);
                calc_filters++;
                break;

            case 2:
                batt_filtered = MAFilter8(vbatt);
                calc_filters++;
                break;

            case 3:
                curr_filtered = MAFilter8(iboost);
                calc_filters++;
                break;

            case 4:
                break;
        }
            
        UpdateLed();
    }               

    // AdcConfig();
    // ADC1->CR |= ADC_CR_ADSTART;
    
    // TIM_14_Init();
    // UpdateLaserCh1(0);
    // UpdateLaserCh2(0);
    // UpdateLaserCh3(0);
    // UpdateLaserCh4(0);

    // USART1Config();

    
    // while (1)
    // {        
    //     TreatmentManager();
    //     UpdateCommunications();
    //     UpdateLed();
    //     UpdateBuzzer();
    // }
    // //fin prueba modulo signals.c comm.c tim.c adc.c

    //prueba modulo adc.c tim.c e int adc
    // TIM_3_Init();
    // Update_TIM3_CH1(511);
    // Update_TIM3_CH2(0);
    // Update_TIM3_CH3(0);
    // Update_TIM3_CH4(0);

    // AdcConfig();
    // ADC1->CR |= ADC_CR_ADSTART;
    
    // while (1)
    // {
    //     if (seq_ready)
    //     {
    //         seq_ready = 0;
    //         if (LED)
    //             LED_OFF;
    //         else
    //             LED_ON;
    //     }
    // }               
    //fin prueba modulo adc.c tim.c e int adc



    // Update_TIM3_CH1(511);
    // Update_TIM3_CH2(511);
    // Update_TIM3_CH3(511);
    // Update_TIM3_CH4(511);
    // while (1);
    





    return 0;
}
//--- End of Main ---//

void TimingDelay_Decrement(void)
{
    if (wait_ms_var)
        wait_ms_var--;

    if (timer_standby)
        timer_standby--;

    if (timer_filters)
        timer_filters--;
    
    if (timer_led)
        timer_led--;

}

//--- end of file ---//

