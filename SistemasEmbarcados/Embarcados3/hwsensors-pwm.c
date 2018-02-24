/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "dev/leds.h"
#include "dev/watchdog.h"
#include "random.h"
#include "button-sensor.h"
#include "board-peripherals.h"

#include "cpu/cc26xx-cc13xx/lpm.h"

#include "ti-lib.h"

#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "dev/adc-sensor.h"
#include "lib/sensors.h"

static struct sensors_sensor *sensor;
static struct etimer et;

static int current_duty = 0;
static int loadvalue = 0;
static int max = 0;

uint8_t pwm_request_max_pm(void){
    return LPM_MODE_DEEP_SLEEP;
}

void sleep_enter(void){
    leds_on(LEDS_RED);
}

void sleep_leave(void){
    leds_off(LEDS_RED);
}

LPM_MODULE(pwmdrive_module, pwm_request_max_pm, sleep_enter, sleep_leave, LPM_DOMAIN_PERIPH);

int16_t pwminit(int32_t freq) {

    /* Register with LPM. This will keep the PERIPH PD powered on
    * during deep sleep, allowing the pwm to keep working while the chip is
    * being power-cycled */
    lpm_register_module(&pwmdrive_module);

    uint32_t load = 0;
    ti_lib_ioc_pin_type_gpio_output(IOID_21);
    leds_off(LEDS_RED);

    /* Enable GPT0 clocks under active mode */
    ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_TIMER0);

    ti_lib_prcm_peripheral_sleep_enable(PRCM_PERIPH_TIMER0);
    ti_lib_prcm_peripheral_deep_sleep_enable(PRCM_PERIPH_TIMER0);

    ti_lib_prcm_load_set();
    while (!ti_lib_prcm_load_get());

    /* Drive the I/O ID with GPT0 / Timer A */
    ti_lib_ioc_port_configure_set(IOID_21, IOC_PORT_MCU_PORT_EVENT0, IOC_STD_OUTPUT);

    /* GPT0 / Timer A: PWM, Interrupt Enable */
    ti_lib_timer_configure(GPT0_BASE,
            TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM | TIMER_CFG_B_PWM);

    /* Stop the timers */
    ti_lib_timer_disable(GPT0_BASE, TIMER_A);
    ti_lib_timer_disable(GPT0_BASE, TIMER_B);

    if (freq > 0) {
        load = (GET_MCU_CLOCK / freq);
        ti_lib_timer_load_set(GPT0_BASE, TIMER_A, load);
        ti_lib_timer_match_set(GPT0_BASE, TIMER_A, load-1);
        /* Start */
        ti_lib_timer_enable(GPT0_BASE, TIMER_A);
    }

    return load;
}

/*---------------------------------------------------------------------------*/
PROCESS(hwsensors_process, "hwsensors process");
AUTOSTART_PROCESSES(&hwsensors_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hwsensors_process, ev, data) {

    PROCESS_BEGIN();

    //seta o timer para 1s
    etimer_set(&et, CLOCK_SECOND * 0.3);
    sensor = sensors_find(ADC_SENSOR);
    sensor->configure(ADC_SENSOR_SET_CHANNEL, ADC_COMPB_IN_AUXIO4);

    loadvalue = pwminit(10000);
//    int ticks = (50 * loadvalue) / 100;
//    ti_lib_timer_match_set(GPT0_BASE, TIMER_A, loadvalue - ticks);

    int read, p, ticks;

    while(1) {

        PROCESS_YIELD();

         if(ev == PROCESS_EVENT_TIMER){

             SENSORS_ACTIVATE(*sensor);
             read = sensor->value(ADC_SENSOR_VALUE);
             SENSORS_DEACTIVATE(*sensor);

             if (read > max) max = read;
             p = read*100/max;

             if(p > 0){
                 current_duty = p;
                 ticks = (current_duty * loadvalue) / 100;
                 printf("read: %d, max: %d, %d %%, ticks: %d \n", read, max, p, ticks);
                 ti_lib_timer_match_set(GPT0_BASE, TIMER_A, loadvalue - ticks);
             }

             etimer_reset(&et);

         }
    }

    PROCESS_END();
}
