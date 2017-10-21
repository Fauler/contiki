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

//    int ticks = (50 * loadvalue) / 100;
//    ti_lib_timer_match_set(GPT0_BASE, TIMER_A, loadvalue - ticks);

    static int maxUp, maxDown;
    maxUp = sensor->value(ADC_SENSOR_VALUE) * 1.1;
    maxDown = sensor->value(ADC_SENSOR_VALUE) * 0.9;

    while(1) {

        PROCESS_YIELD();

         if(ev == PROCESS_EVENT_TIMER){

             SENSORS_ACTIVATE(*sensor);
             int read = sensor->value(ADC_SENSOR_VALUE);
             SENSORS_DEACTIVATE(*sensor);

             if(read >= maxUp){
                 printf("Elevador parou acima do limite");
             } else if (read <= maxDown){
                 printf("Elevador parou abaixo do limite");
             } else {
                 printf("Abre porta");
             }

             etimer_reset(&et);

         }
    }

    PROCESS_END();
}
