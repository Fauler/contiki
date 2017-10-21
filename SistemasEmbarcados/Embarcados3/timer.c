/*
 * Copyright (c) 2014, Texas Instruments Incorporated - http://www.ti.com/
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
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"

#include "dev/leds.h"
#include "dev/watchdog.h"
#include "dev/adc-sensor.h"

#include "random.h"
#include "button-sensor.h"
#include "board-peripherals.h"
#include "lib/sensors.h"
#include "ti-lib.h"

#include "lpm.h"

#include <stdio.h>
#include <stdint.h>
/*---------------------------------------------------------------------------*/
static struct etimer et;
static struct etimer et_adc;
static struct etimer et_adc_sensor;
static struct etimer et_gpio;
static uint16_t single_adc_sample;

/*---------------------------------------------------------------------------*/
PROCESS(gpio_process, "gpio bare metal process");

PROCESS(adc_process_sensor, "adc driver process");

AUTOSTART_PROCESSES(&gpio_process, &adc_process_sensor);
/*---------------------------------------------------------------------------*/


/* GPIO */
PROCESS_THREAD(gpio_process, ev, data)
{
    static int8_t counter = 0;
    PROCESS_BEGIN();

    etimer_set(&et_gpio, CLOCK_SECOND * 1);

    /* initialization of GPIO output */
    ti_lib_rom_ioc_pin_type_gpio_output(IOID_27);
    ti_lib_rom_ioc_pin_type_gpio_output(IOID_30);
    ti_lib_gpio_clear_multi_dio(1<<IOID_27 | 1<<IOID_30);

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_gpio));
        counter++;

        if(counter & 0x1)
            ti_lib_gpio_set_dio(IOID_27);
        else
            ti_lib_gpio_clear_dio(IOID_27);

        if(counter & 0x2)
            ti_lib_gpio_set_dio(IOID_30);
        else
            ti_lib_gpio_clear_dio(IOID_30);

        etimer_reset(&et_gpio);
    }

    PROCESS_END();
}





PROCESS_THREAD(adc_process_sensor, ev, data)
{
  static struct sensors_sensor *sensor;
  static int rv;
  PROCESS_BEGIN();
 
  etimer_set(&et_adc_sensor, CLOCK_SECOND * 4);
  while(1) {

      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_adc_sensor));

      sensor = sensors_find(ADC_SENSOR);
      if(sensor) {
          SENSORS_ACTIVATE(*sensor);
          sensor->configure(ADC_SENSOR_SET_CHANNEL,ADC_COMPB_IN_AUXIO7);
          rv = sensor->value(ADC_SENSOR_VALUE);
          SENSORS_DEACTIVATE(*sensor);
          printf("ADC value is %d\n", rv);
      }
      else
      {
          printf("Sensor not found!\n");

      }
      etimer_reset(&et_adc_sensor);
  }

  PROCESS_END();
}






/**
 * @}
 * @}
 * @}
 */

