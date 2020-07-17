/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2015 Piotr Esden-Tempski <piotr@esden.net>
 * Copyright (C) 2015 Jack Ziesing <jziesing@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>


#include "whoot4.h"

// ok - having rotary encoder - means can control servo - etc. really interesting.
// For hard real time - we need to avoid interrupts.

/*

    Uses - TIM3 and slave mode, so very similar....
      https://github.com/aklomp/stm32-rotary-zero/blob/master/src/rotary.c

      PA6 and PA7 which on stm32f4 - are TIM3_CH1 and TIM3_CH2

      So maybe those pins instead of being outputs can be inputs also.


    When a higher resolution is needed, it is possible for the counter to count the
    leading and trailing edges of the quadrature encoder’s pulse train from one
    channel, which doubles (x2) the number of pulses per revolution. Counting both
    leading and trailing edges of both channels (A and B channels) of a quadrature
    encoder will quadruple (x4) the number of pulses per revolution.

    https://deepbluembedded.com/stm32-timer-encoder-mode-stm32-rotary-encoder-interfacing/
      example here - uses two input timers.

    https://stm32f4-discovery.net/2014/08/library-26-rotary-encoder-stm32f4/
      stm32 f4
    , PD0 and PD1, PD3,PD5 are used .



  Hi Tilen, I am surprised that you do not mention that STM32F4 can do encoder
  inputs directly on TIM2-5. Page 614 of ST RM0090 refers.
  --------

    example using, interrupts,
      https://github.com/Derecho/stm32-rotaryencoder/tree/master/rotaryencoder


  VERY GOOD on timers, generally,
  https://letanphuc.net/2015/06/stm32f0-timer-tutorial-and-counter-tutorial/

  alternate functions are p62  of the manual.
 
  we only actually really need a simple button counter... 

  MAYBE SHOULD BE USING ETR --- although doesn't exist. on TIM3
    NO.

  OK. looks very good, counting pulses, using   STM32F407VG
  https://www.fmf.uni-lj.si/~ponikvar/STM32F407%20project/Ch9%20-%20Counting%20pulses%20by%20Timer%202.pdf
    Uses PA15 TIM2_ETR
    even though there are pins for TIM2_CH1_ETR and another for the same which may be errata for TIM2_CH1_ETR 

  TIM3 ETR is on pin PD2   AF2 . but there are no separate channels...
  TIM2 ETR is on pin 15

  OK. so we should just try to get a single clock working.

  REALLY NOT clear - they are external clock modes...
    ETR - is external clock mode 2.
    non ETR and CH are external clock mode 1

  Note the XOR block on the non-ETR inputs, p17
    https://www.st.com/content/ccc/resource/technical/document/application_note/group0/91/01/84/3f/7c/67/41/3f/DM00236305/files/DM00236305.pdf/jcr:content/translations/en.DM00236305.pdf

    ETR - extended timer, edge triggered ? what the fuck 
  -----------

  https://letanphuc.net/2015/06/stm32f0-timer-tutorial-and-counter-tutorial/
    clock mode 1 -> external input pin     - TIx
    clock mode 2 -> external trigger input -  ETR   etr='external'. or 'external trigger'

    Cannot see TIx in the internal functions.

  --------------
  Picture above is an example of pin PA0, which has the “TIM2_CH1” function. It
  means that you can use this pin as an input signal for Counter TIM2. 

  ------

  example of rotatary in code...
    https://sourceforge.net/p/libopencm3/mailman/libopencm3-devel/?viewmonth=201508
    except doesn't work.

  https://studylib.es/doc/8805589/384834373-beginning-stm32

  //////////

  Need to do,
    - try a different channel, CH2 ... 
    - use the on-board button instead which we know works, see if can get it configured to count
    - use blocking read and write - in main control loop...
 */



int main(void)
{

	rcc_clock_setup_pll(&rcc_hse_8mhz_3v3[RCC_CLOCK_3V3_168MHZ]);

  usart_setup();



  ///////////////////////
  // good, console code,  ../../stm32f429i-discovery/lcd-serial/console.c

  // board button is PD15.  AF2  TIM4_CH4, p65 of manual

  // PA6, PA7    AF2

	rcc_periph_clock_enable(RCC_GPIOD);
	// gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO6 | GPIO7 );
	gpio_mode_setup(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO6 | GPIO7 );
  // or floating...
  gpio_set_af(GPIOA, GPIO_AF2, GPIO6 | GPIO7 );  

  /*
  HERES AN EXAMPLE - from RTOS
  https://git.rnd2.org/erigas/stm32f103c8t6/src/commit/f8109e63a94f8fc8a50d9a165f27932fba148e4f/rtos/tim4_pwm_in/main.c 
  */

  // THINK - we should check the rotary encoder is actually delivering signals. on gpio pins.
  // by modifying button code... 

  // think we really do require this...  otherwise cannot do set_counter, and the slave_set_mode doesn't mix in a count...
   rcc_periph_clock_enable(RCC_TIM3);

   // TIM3:
   timer_disable_counter(TIM3);


   // timer_reset(TIM3);
    rcc_periph_reset_pulse(RST_TIM3);

   // nvic_set_priority(NVIC_DMA1_CHANNEL3_IRQ,2);
   // nvic_enable_irq(NVIC_TIM3_IRQ);
   timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
   // timer_set_prescaler(TIM3,72);
   timer_set_prescaler(TIM3, 1);
   timer_ic_set_input(TIM3,TIM_IC1,TIM_IC_IN_TI1);
   timer_ic_set_input(TIM3,TIM_IC2,TIM_IC_IN_TI2);

   // timer_ic_set_filter(TIM3,TIM_IC_IN_TI4,TIM_IC_CK_INT_N_2);

   timer_ic_set_prescaler(TIM3,TIM_IC1,TIM_IC_PSC_OFF);
   // timer_slave_set_mode(TIM3,TIM_SMCR_SMS_RM);
   timer_slave_set_mode(TIM3, 7);     // OK. this is a mixer - eg. can take internal or external clock.

   timer_slave_set_trigger(TIM3,TIM_SMCR_TS_TI1FP1);

   TIM_CCER(TIM3) &= ~(TIM_CCER_CC2P|TIM_CCER_CC2E
    |TIM_CCER_CC1P|TIM_CCER_CC1E);
   TIM_CCER(TIM3) |= TIM_CCER_CC2P|TIM_CCER_CC2E|TIM_CCER_CC1E;
   timer_ic_enable(TIM3,TIM_IC1);
   timer_ic_enable(TIM3,TIM_IC2);
   // timer_enable_irq(TIM3,TIM_DIER_CC1IE|TIM_DIER_CC2IE);



  // weird... thiis doesn't work ? 
  // because the clocking to the whole clock thing is not on...
  timer_set_counter(TIM3 , 9999 ); // OK. working now.  to do this we have to have the main clock runnning.
                                    // which indicates should always be running...

   timer_enable_counter(TIM3);
  
	while (1) {

      int count = timer_get_counter(TIM3);

      printf("count.. %d\n", count);

			__asm__("nop");
	}

	return 0;
}





  // ck_int - is internal clock.
  // ic = input compare, internal clock? 
   //  oc = output compare mode - correct ?
  // 77 functions... for timers....


  /*
  p602,
  External clock source mode 1
  This mode is selected when SMS=111 in the TIMx_SMCR register. The counter can count at
  each rising or falling edge on a selected input.
  */
 

  // DO i need interrupts set. to get count on external clock? 
  // ic = input compare, or input channel



/*
      switch(motor_pos % 2) {
        case 0: gpio_clear(GPIOE, GPIO0); break;
        case 1: gpio_set(GPIOE, GPIO0); break;
      }
*/
