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
#define UNDERSAMPLING_TICKS    10
unsigned short vin [SIZEOF_FILTER];
unsigned short voneten [SIZEOF_FILTER];

short d = 0;
short ez1 = 0;
short ez2 = 0;

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

#define VOUT_SETPOINT    OUT_40V
#define MAX_VOUT         OUT_45V

#ifdef TWO_RSENSE
//corriente de salida por 1.49V/A
#define IOUT_007MA    33   
#define IOUT_350MA    161
#define IOUT_700MA    323
#define IOUT_1000MA   461
#endif

#ifdef ONE_RSENSE
//corriente de salida por 2.97V/A
#define IOUT_007MA    66    
#define IOUT_350MA    322
#define IOUT_700MA    646
#define IOUT_1000MA   922
#endif

#define IOUT_SETPOINT    IOUT_700MA
#define IOUT_MIN         IOUT_007MA

#define ONE_TEN_ON    25
#define ONE_TEN_OFF   16


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
    unsigned short vin_filtered = 0;
    
#ifdef WITH_POTE_CTRL
    unsigned int setpoint;
    unsigned short voneten_filtered = 0;
#endif

#ifdef AUTOMATIC_CTRL
    unsigned int setpoint;    
    unsigned short ii = 33;
    unsigned char subiendo = 0;
#endif

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
    // Update_TIM3_CH2(DUTY_10_PERCENT);
    // while (1)
    // {
    //     // if (timer_led_pwm < 512)
    //     if (timer_led_pwm < Vout_Sense)
    //     // if (timer_led_pwm < One_Ten_Pote)
    //         LED_ON;
    //     else
    //         LED_OFF;

    //     if (timer_led_pwm > TIMER_LED_RELOAD)
    //         timer_led_pwm = 0;

    //     // if (LED)
    //     //     LED_OFF;
    //     // else
    //     //     LED_ON;

    //     // Wait_ms(20);
    // }
    //fin prueba led pwm contra adc



    
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
                else if (voneten_filtered > ONE_TEN_ON)
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

                            if (Vout_Sense > MAX_VOUT)    //maxima tension permitida sin cortar lazo
                            {
                                d = 0;
                                Update_TIM3_CH2 (d);
                            }
                            else
                            {
                                if (undersampling < UNDERSAMPLING_TICKS)
                                    undersampling++;
                                else
                                {
                                    //reviso lazo V o I
                                    if (Vout_Sense > VOUT_SETPOINT)    //uso lazo V
                                    {
                                        d = PID_roof (VOUT_SETPOINT, Vout_Sense, d, &ez1, &ez2);
                                    }
                                    else    //uso lazo I
                                    {
#ifdef AUTOMATIC_CTRL
                                        if (!timer_standby)
                                        {
                                            timer_standby = 5;    //con 5 va, 7 ya no
                                            if (subiendo)
                                            {
                                                if (ii < 63)
                                                    ii++;
                                                else
                                                    subiendo = 0;
                                            }
                                            else
                                            {
                                                if (ii > 33)
                                                    ii--;
                                                else
                                                    subiendo = 1;
                                            }


                                            setpoint = ii;
                                        }
                                            
                                        d = PID_roof ((unsigned short) setpoint, Iout_Sense, d, &ez1, &ez2);
#endif
#ifdef WITH_POTE_CTRL
                                        setpoint = IOUT_SETPOINT * voneten_filtered;
                                        setpoint >>= 10;
                                        if (setpoint < IOUT_MIN)
                                            setpoint = IOUT_MIN;
                                        d = PID_roof ((unsigned short) setpoint, Iout_Sense, d, &ez1, &ez2);
#endif
#ifdef FIXED_CTRL
                                        d = PID_roof (IOUT_SETPOINT, Iout_Sense, d, &ez1, &ez2);
#endif                                        
                                    }

                                    if (d < 0)
                                        d = 0;

                                    if (d > DUTY_80_PERCENT)	//no pasar del 90%
                                        d = DUTY_80_PERCENT;

                                    Update_TIM3_CH2 (d);

                                }                                
                            }    //cierra Vout max
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

                if (voneten_filtered < ONE_TEN_OFF)
                {
                    Update_TIM3_CH2(0);
                    ChangeLed(LED_STANDBY);
                    main_state = INIT;
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
        
        //envio de info analogica al ledpwm
#ifdef AUTOMATIC_CTRL        
        if (timer_led_pwm < d)            
#endif        
#ifdef WITH_POTE_CTRL        
        // if (timer_led_pwm < setpoint)
        if (timer_led_pwm < d)            
#endif
#ifdef FIXED_CTRL
        if (timer_led_pwm < Vout_Sense)
#endif
            LED_ON;
        else
            LED_OFF;

        if (timer_led_pwm > TIMER_LED_RELOAD)
            timer_led_pwm = 0;
        //fin envio de info analogica al ledpwm

        
        if (!timer_filters)
        {
            vin[0] = Vin_Sense;
            voneten[0] = One_Ten_Pote;
            calc_filters = 1;
            timer_filters = 5;
        }

        switch (calc_filters)    //distribuyo filtros en varios pasos
        {
            case 1:
                vin_filtered = MAFilter8(vin);
                calc_filters++;
                break;

            case 2:
#ifdef WITH_POTE_CTRL
                voneten_filtered = MAFilter8(voneten);
#endif
                calc_filters++;
                break;

            case 3:
                break;

        }
            
        // UpdateLed();
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

