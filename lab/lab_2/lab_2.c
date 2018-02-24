#include "contiki.h"
#include "sys/etimer.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

#include <stdio.h> /* For printf() */
/*---------------------------------------------------------------------------*/
PROCESS(Lab_2_process, "Lab 2 process");
AUTOSTART_PROCESSES(&Lab_2_process);
/*---------------------------------------------------------------------------*/

static struct etimer et;

//static void ctimer_callback_ex(void *ptrValor){
//    leds_toggle(LEDS_RED|LEDS_GREEN);
//    ctimer_set(&ct, CLOCK_SECOND * 2, ctimer_callback_ex, void);
//}

PROCESS_THREAD(Lab_2_process, ev, data)
{
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);

//  etimer_set(&et, CLOCK_SECOND * 0.5);
//  PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
//  leds_set(LEDS_RED|~LEDS_GREEN);

  while (1) {
      PROCESS_YIELD();
      if(ev == PROCESS_EVENT_TIMER){
          leds_toggle(LEDS_RED|LEDS_GREEN);
          etimer_restart(&et);
      } else if(ev == sensors_event){
          if(data == &button_left_sensor){ //botao 1
              etimer_set(&et, CLOCK_SECOND * 0.5);
              PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
              leds_set(LEDS_RED|~LEDS_GREEN);
              etimer_set(&et, CLOCK_SECOND * 2);
          } else if (data == &button_right_sensor){ //botao 2
              etimer_stop(&et);
              leds_set(~LEDS_RED|~LEDS_GREEN);
          }
      }
  }

  SENSORS_DEACTIVATE(button_sensor);
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
