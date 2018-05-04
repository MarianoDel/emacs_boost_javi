//---------------------------------------------
// #### PROYECTO BOOST JAVILANDIA - Custom Board ####
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
unsigned short vin [SIZEOF_FILTER];
unsigned short voneten [SIZEOF_FILTER];

short d = 0;
short ez1 = 0;
short ez2 = 0;

#define IN_9_5V     215    //son 9V en el sensor
#define IN_11_5V    262    //son 11V en el sensor
#define IN_16V      382
#define IN_20V      477 

#define OUT_24V      572
#define OUT_35V      835
#define OUT_40V      953


//--- VARIABLES GLOBALES ---//
#define TIMER_LED_RELOAD    1024
unsigned short timer_led_pwm = 0;

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
    unsigned short voneten_filtered = 0;
    unsigned short vin_filtered = 0;
        

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
    // Update_TIM3_CH2 (5);

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

    timer_standby = 1000;

    //prueba led pwm contra adc
    Update_TIM3_CH2(DUTY_10_PERCENT);
    while (1)
    {
        // if (timer_led_pwm < 512)
        if (timer_led_pwm < Vout_Sense)
            LED_ON;
        else
            LED_OFF;

        if (timer_led_pwm > TIMER_LED_RELOAD)
            timer_led_pwm = 0;

        // if (LED)
        //     LED_OFF;
        // else
        //     LED_ON;

        // Wait_ms(20);
    }




    
    while (1)
    {
        switch (main_state)
        {
            case INIT:
                if (!timer_standby)
                    main_state++;

                break;

            case STAND_BY:
                if (vin_filtered < IN_9_5V)    //baja tension aviso el error
                {
                    main_state = LOW_INPUT;
                    ChangeLed(LED_LOW_VOLTAGE);
                    timer_standby = 100;
                }
                else if (vin_filtered > IN_20V)    //exceso de tension, aviso del error
                {
                    main_state = HIGH_INPUT;
                    ChangeLed(LED_HIGH_VOLTAGE);
                    timer_standby = 100;                    
                }
                else
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

                                 d = PID_roof (OUT_24V, Vout_Sense, d, &ez1, &ez2);
                                //d = PID_roof (OUT_80V, Vout_Sense, d, &ez1, &ez2);
                                // d = PID_roof (OUT_50V, Vout_Sense, d, &ez1, &ez2);                                
                                if (d < 0)
                                    d = 0;

                                if (d > DUTY_10_PERCENT)	//no pasar del 90%
                                    d = DUTY_10_PERCENT;

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

                //reviso tensiones de alimentacion
                if (vin_filtered > IN_20V)
                {
                    //dejo de generar y vuelvo a 220
                    Update_TIM3_CH2(0);
                    ChangeLed(LED_HIGH_VOLTAGE);
                    main_state = HIGH_INPUT;
                    timer_standby = 500;
                }

                //reviso si esta ya muy baja la entrada
                if (vin_filtered < IN_9_5V)
                {
                    Update_TIM3_CH2(0);
                    ChangeLed(LED_LOW_VOLTAGE);
                    main_state = LOW_INPUT;
                    timer_standby = 500;
                }
                break;

            case LOW_INPUT:
                if (!timer_standby)
                {
                    if (vin_filtered > IN_11_5V)
                    {
                        //tengo buena tension de entrada                   
                        main_state = STAND_BY;
                    }
                    else
                        timer_standby = 10;
                }                
                break;

            case HIGH_INPUT:
                if (!timer_standby)
                {
                    if (vin_filtered < IN_16V)
                    {
                        //tengo bien la entrada
                        main_state = STAND_BY;
                    }
                    else
                        timer_standby = 10;
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
            vin[0] = Vin_Sense;
            voneten[0] = One_Ten_Pote;
            calc_filters = 1;
            timer_filters = 10;
        }

        switch (calc_filters)    //distribuyo filtros en varios pasos
        {
            case 1:
                vin_filtered = MAFilter8(vin);
                calc_filters++;
                break;

            case 2:
                voneten_filtered = MAFilter8(voneten);
                calc_filters++;
                break;

            case 3:
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

    if (timer_led_pwm < 0xFFFF)
        timer_led_pwm ++;
    
}

//--- end of file ---//

