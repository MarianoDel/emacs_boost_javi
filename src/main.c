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
#include "dma.h"


// Externals -----------------------------------------------
// -- Externals from or for the ADC ------------------------
volatile unsigned short adc_ch [ADC_CHANNEL_QUANTITY];
#ifdef ADC_WITH_INT
volatile unsigned char seq_ready = 0;
#endif

// -- Externals for the timers -----------------------------
volatile unsigned short timer_led = 0;
volatile unsigned short timer_standby;
volatile unsigned short timer_filters;
volatile unsigned short wait_ms_var = 0;


// Globals -------------------------------------------------

//  for the filters
ma16_u16_data_obj_t boost_sense_data_filter;
ma16_u16_data_obj_t battery_sense_data_filter;
ma16_u16_data_obj_t iout_sense_data_filter; 
ma16_u16_data_obj_t vout_sense_data_filter; 
ma16_u16_data_obj_t mains_sense_data_filter;

//  for the pid controllers
pid_data_obj_t voltage_pid;
pid_data_obj_t current_pid;
#define UNDERSAMPLING_TICKS    10
#define SOFT_START_CNT_ROOF    4
// ------- de los timers -------

// Module Functions ----------------------------------------
void TimingDelay_Decrement(void);
unsigned short CurrentLoop (unsigned short, unsigned short);
unsigned short VoltageLoop (unsigned short, unsigned short);


//-------------------------------------------//
// @brief  Main program.
// @param  None
// @retval None
//------------------------------------------//
//variables reuse
#define vin_sense_filtered battery_sense_filtered

int main(void)
{
    unsigned short boost_sense_filtered = 0;
    unsigned short battery_sense_filtered = 0;
    unsigned short iout_sense_filtered = 0;
    unsigned short vout_sense_filtered = 0;
    unsigned short mains_sense_filtered = 0;

    unsigned char undersampling = 0;
    unsigned char soft_start_cnt = 0;
    main_state_t main_state = INIT;
    
    //GPIO Configuration.
    GPIO_Config();

    //ACTIVAR SYSTICK TIMER
    if (SysTick_Config(48000))
    {
        while (1)	/* Capture error */
        {
            if (LED)
                LED_OFF;
            else
                LED_ON;

            for (unsigned char i = 0; i < 255; i++)
            {
                asm (	"nop \n\t"
                        "nop \n\t"
                        "nop \n\t" );
            }
        }
    }

    TIM_3_Init();
    CTRL_MOSFET (DUTY_NONE);

    //ADC and DMA configuration
    AdcConfig();
    DMAConfig();
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    ADC1->CR |= ADC_CR_ADSTART;
    //end of ADC & DMA

    //start the circular filters
    MA16_U16Circular_Reset(&boost_sense_data_filter);
    MA16_U16Circular_Reset(&battery_sense_data_filter);
    MA16_U16Circular_Reset(&iout_sense_data_filter);
    MA16_U16Circular_Reset(&vout_sense_data_filter);
    MA16_U16Circular_Reset(&mains_sense_data_filter);

    // Initial Setup for PID Controller
    PID_Small_Ki_Flush_Errors(&voltage_pid);
    PID_Small_Ki_Flush_Errors(&current_pid);

    voltage_pid.kp = 128;
    voltage_pid.ki = 16;
    voltage_pid.kd = 0;
    current_pid.kp = 128;
    current_pid.ki = 16;
    current_pid.kd = 0;
    unsigned short d = 0;

    timer_standby = 100;    //doy tiempo a los filtros

#ifdef BOOST_BACKUP_TECNOCOM
    while (1)
    {
        //Most work involved is sample by sample
        if (sequence_ready)
        {
            sequence_ready_reset;

            //filters
            boost_sense_filtered = MA16_U16Circular(&boost_sense_data_filter, Boost_Sense);
            battery_sense_filtered = MA16_U16Circular(&battery_sense_data_filter, VBat_Sense);
            iout_sense_filtered = MA16_U16Circular(&iout_sense_data_filter, Iout_Sense);
            vout_sense_filtered = MA16_U16Circular(&vout_sense_data_filter, Vout_Sense);
            mains_sense_filtered = MA16_U16Circular(&mains_sense_data_filter, Vmains_Sense);
            
            switch (main_state)
            {
            case INIT:
                if (!timer_standby)
                {
                    ChangeLed(LED_STANDBY);
                    main_state++;
                }
                break;

            case STAND_BY:
                //reviso si genero desde 220Vac
                if ((mains_sense_filtered > MAINS_TO_RECONNECT_VOLTAGE) &&
                    (mains_sense_filtered < MAINS_HIGH_VOLTAGE))
                {
                    //paso a generar
                    main_state = SOFT_START_FROM_MAINS;
                    ChangeLed(LED_GENERATING_FROM_MAINS);
                    PID_Small_Ki_Flush_Errors(&voltage_pid);
                    PID_Small_Ki_Flush_Errors(&current_pid);
                }
                //reviso si genero de bateria
                else if ((battery_sense_filtered > BATTERY_TO_RECONNECT_VOLTAGE) &&
                         (battery_sense_filtered < BATTERY_MAX_VOLTAGE))
                {
                    main_state = SOFT_START_FROM_BATTERY;
                    ChangeLed(LED_GENERATING_FROM_BATTERY);
                    PID_Small_Ki_Flush_Errors(&voltage_pid);
                    PID_Small_Ki_Flush_Errors(&current_pid);
                }                
                break;

            case SOFT_START_FROM_MAINS:
                soft_start_cnt++;
                
                //check to not go overvoltage
                if (vout_sense_filtered < VOUT_FOR_SOFT_START)
                {
                    //do a soft start checking the voltage
                    if (soft_start_cnt > SOFT_START_CNT_ROOF)    //update 200us aprox.
                    {
                        soft_start_cnt = 0;
                    
                        if (d < DUTY_FOR_DMAX)
                        {
                            d++;
                            CTRL_MOSFET(d);
                        }
                        else
                        {
                            //update PID
                            voltage_pid.last_d = d;
                            main_state = GENERATING_FROM_MAINS;
                        }
                    }
                }
                else
                {
                    //update PID
                    voltage_pid.last_d = d;
                    main_state = GENERATING_FROM_MAINS;
                }
                break;

            case GENERATING_FROM_MAINS:
                if (Vout_Sense > MAX_VOUT)    //maxima tension permitida sin cortar lazo
                {
                    d = 0;
                    CTRL_MOSFET (d);
                }
                else
                {
                    if (undersampling < UNDERSAMPLING_TICKS)
                        undersampling++;
                    else
                    {
                        //reviso lazo V o I
                        if (Vout_Sense > VOUT_SETPOINT)    //uso lazo V
                            d = VoltageLoop (VOUT_SETPOINT, Vout_Sense);

                        else    //uso lazo I
                            d = CurrentLoop (IOUT_SETPOINT, Iout_Sense);

                        CTRL_MOSFET (d);
                    }                                
                }    //cierra Vout max

                //si la tension de entrada baja paso a bateria directo
                if (mains_sense_filtered < MAINS_LOW_VOLTAGE)
                {
                    main_state = GENERATING_FROM_BATTERY;
                    ChangeLed(LED_GENERATING_FROM_BATTERY);
                    timer_standby = 2000;
                }
                break;
                
            case SOFT_START_FROM_BATTERY:
                soft_start_cnt++;
                
                //check to not go overvoltage
                if (vout_sense_filtered < VOUT_FOR_SOFT_START)
                {
                    //do a soft start checking the voltage
                    if (soft_start_cnt > SOFT_START_CNT_ROOF)    //update 200us aprox.
                    {
                        soft_start_cnt = 0;
                    
                        if (d < DUTY_FOR_DMAX)
                        {
                            d++;
                            CTRL_MOSFET(d);
                        }
                        else
                        {
                            //update PID
                            voltage_pid.last_d = d;
                            main_state = GENERATING_FROM_BATTERY;
                        }
                    }
                }
                else
                {
                    //update PID
                    voltage_pid.last_d = d;
                    main_state = GENERATING_FROM_BATTERY;
                }
                break;

            case GENERATING_FROM_BATTERY:
                if (Vout_Sense > MAX_VOUT)    //maxima tension permitida sin cortar lazo
                {
                    d = 0;
                    CTRL_MOSFET (d);
                }
                else
                {
                    if (undersampling < UNDERSAMPLING_TICKS)
                        undersampling++;
                    else
                    {
                        //reviso lazo V o I
                        if (Vout_Sense > VOUT_SETPOINT)    //uso lazo V
                            d = VoltageLoop (VOUT_SETPOINT, Vout_Sense);

                        else    //uso lazo I
                            d = CurrentLoop (IOUT_SP_BATTERY, Iout_Sense);

                        CTRL_MOSFET (d);
                    }                                
                }    //cierra Vout max


                if (!timer_standby)
                {
                    //estoy con bateria y vuelven los 220Vac
                    if (mains_sense_filtered > MAINS_TO_RECONNECT_VOLTAGE)
                    {
                        main_state = GENERATING_FROM_MAINS;
                        ChangeLed(LED_GENERATING_FROM_MAINS);
                    }
                    
                    //estoy con bateria pero baja mucho
                    if (battery_sense_filtered < BATTERY_MIN_VOLTAGE)
                    {
                        CTRL_MOSFET(DUTY_NONE);
                        main_state = INIT;
                    }
                }
                break;
                
            case OVERCURRENT:
                break;

            case JUMPER_PROTECTED:
                if (!timer_standby)
                {
                    if (!STOP_JUMPER)
                    {
                        main_state = JUMPER_PROTECT_OFF;
                        timer_standby = 400;
                    }
                }                
                break;

            case JUMPER_PROTECT_OFF:
                if (!timer_standby)
                {
                    //vuelvo a INIT
                    main_state = INIT;
                }                
                break;            
                
            default:
                main_state = INIT;
                break;
            }
        }    //end sequence_ready

        //
        //The things that are not directly attached to the samples period
        //
#ifdef LED_SHOW_STATUS
        UpdateLed();
#endif
        
        if ((STOP_JUMPER) &&
            (main_state != JUMPER_PROTECTED) &&
            (main_state != JUMPER_PROTECT_OFF) &&            
            (main_state != OVERCURRENT))
        {
            CTRL_MOSFET(DUTY_NONE);
            
            ChangeLed(LED_JUMPER_PROTECTED);
            main_state = JUMPER_PROTECTED;
            timer_standby = 1000;
        }

    }    //end while 1
#endif    //BOOST BACKUP TECNOCOM

#ifdef BOOST_JAVI
#ifdef WITH_POTE_CTRL
    unsigned int setpoint;
    unsigned short voneten_filtered = 0;
#endif
#ifdef AUTOMATIC_CTRL
    unsigned int setpoint;    
    unsigned short ii = 33;
    unsigned char subiendo = 0;
#endif

    while (1)
    {
        //Most work involved is sample by sample
        if (sequence_ready)
        {
            sequence_ready_reset;

            //filters
            boost_sense_filtered = MA16_U16Circular(&boost_sense_data_filter, Boost_Sense);
            battery_sense_filtered = MA16_U16Circular(&battery_sense_data_filter, VBat_Sense);
            iout_sense_filtered = MA16_U16Circular(&iout_sense_data_filter, Iout_Sense);
            vout_sense_filtered = MA16_U16Circular(&vout_sense_data_filter, Vout_Sense);
            mains_sense_filtered = MA16_U16Circular(&mains_sense_data_filter, Vmains_Sense);
            
            switch (main_state)
            {
            case INIT:
                if (!timer_standby)
                    main_state++;

                break;

            case STAND_BY:
                if (vin_sense_filtered < MIN_INPUT_VOLTAGE)    //baja tension aviso el error
                {
                    main_state = LOW_INPUT;
                    ChangeLed(LED_LOW_VOLTAGE);
                    timer_standby = 100;
                }
                else if (vin_sense_filtered > MAX_INPUT_VOLTAGE)    //exceso de tension, aviso del error
                {
                    main_state = HIGH_INPUT;
                    ChangeLed(LED_HIGH_VOLTAGE);
                    timer_standby = 100;                    
                }
#ifdef WITH_POTE_CTRL
                else if (voneten_filtered > ONE_TEN_ON)
                {
                    //paso a generar
                    main_state = GENERATING;
                    ChangeLed(LED_GENERATING);
                    PID_Small_Ki_Flush_Errors(&current_pid);
                }
#else    //FIXED_CTRL
                else
                {
                    //paso a generar
                    main_state = GENERATING;
                    ChangeLed(LED_GENERATING);
                    PID_Small_Ki_Flush_Errors(&voltage_pid);
                    PID_Small_Ki_Flush_Errors(&current_pid);
                }                
#endif
                break;

            case GENERATING:
                if (Vout_Sense > MAX_VOUT)    //maxima tension permitida sin cortar lazo
                {
                    d = 0;
                    CTRL_MOSFET (d);
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
                            d = VoltageLoop (VOUT_SETPOINT, Vout_Sense);
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
                            d = CurrentLoop (IOUT_SETPOINT, Iout_Sense);
#endif                                        
                        }
                        CTRL_MOSFET (d);
                    }                                
                }    //cierra Vout max

                //reviso tensiones de alimentacion
                if (vin_sense_filtered > IN_20V)
                {
                    //dejo de generar y vuelvo a 220
                    CTRL_MOSFET(0);
                    ChangeLed(LED_HIGH_VOLTAGE);
                    main_state = HIGH_INPUT;
                    timer_standby = 500;
                }

                //reviso si esta ya muy baja la entrada
                if (vin_sense_filtered < IN_9_5V)
                {
                    CTRL_MOSFET(0);
                    ChangeLed(LED_LOW_VOLTAGE);
                    main_state = LOW_INPUT;
                    timer_standby = 500;
                }

#ifdef WITH_POTE_CTRL
                if (voneten_filtered < ONE_TEN_OFF)
                {
                    CTRL_MOSFET(0);
                    ChangeLed(LED_STANDBY);
                    main_state = INIT;
                    timer_standby = 500;
                }
#endif
                break;

            case LOW_INPUT:
                if (!timer_standby)
                {
                    if (vin_sense_filtered > IN_11_5V)
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
                    if (vin_sense_filtered < IN_16V)
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

            case JUMPER_PROTECTED:
                if (!timer_standby)
                {
                    if (!STOP_JUMPER)
                    {
                        main_state = JUMPER_PROTECT_OFF;
                        timer_standby = 400;
                    }
                }                
                break;

            case JUMPER_PROTECT_OFF:
                if (!timer_standby)
                {
                    //vuelvo a INIT
                    main_state = INIT;
                }                
                break;            
                
            default:
                main_state = INIT;
                break;
            }
        }    //end sequence_ready

        //
        //The things that are not directly attached to the samples period
        //
#ifdef LED_SHOW_STATUS
        UpdateLed();
#endif
        
        if ((STOP_JUMPER) &&
            (main_state != JUMPER_PROTECTED) &&
            (main_state != JUMPER_PROTECT_OFF) &&            
            (main_state != OVERCURRENT))
        {
            CTRL_MOSFET(DUTY_NONE);
            
            ChangeLed(LED_JUMPER_PROTECTED);
            main_state = JUMPER_PROTECTED;
            timer_standby = 1000;
        }

    }    //end while 1
#endif    //BOOST JAVI

    return 0;
}
//--- End of Main ---//


unsigned short CurrentLoop (unsigned short setpoint, unsigned short new_sample)
{
    short d = 0;
    
    current_pid.setpoint = setpoint;
    current_pid.sample = new_sample;
    d = PID_Small_Ki(&current_pid);
                    
    if (d > 0)
    {
        if (d > DUTY_FOR_DMAX)
        {
            d = DUTY_FOR_DMAX;
            current_pid.last_d = DUTY_FOR_DMAX;
        }
    }
    else
    {
        d = DUTY_NONE;
        current_pid.last_d = DUTY_NONE;
    }

    return (unsigned short) d;
}


unsigned short VoltageLoop (unsigned short setpoint, unsigned short new_sample)
{
    short d = 0;
    
    voltage_pid.setpoint = setpoint;
    voltage_pid.sample = new_sample;
    d = PID_Small_Ki(&voltage_pid);
                    
    if (d > 0)
    {
        if (d > DUTY_FOR_DMAX)
        {
            d = DUTY_FOR_DMAX;
            voltage_pid.last_d = DUTY_FOR_DMAX;
        }
    }
    else
    {
        d = DUTY_NONE;
        voltage_pid.last_d = DUTY_NONE;
    }

    return (unsigned short) d;
}


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

