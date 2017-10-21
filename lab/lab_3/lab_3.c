#include "contiki.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

#include <stdio.h> /* For printf() */
/*---------------------------------------------------------------------------*/
PROCESS(Lab_3_process, "Lab 3 process");
AUTOSTART_PROCESSES(&Lab_3_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(Lab_3_process, ev, data)
{
  PROCESS_BEGIN();
  SENSORS_ACTIVATE(button_sensor);

  while (1) {
      PROCESS_YIELD();
      if(ev == sensors_event){
          if(data == &button_left_sensor){ //botao 1
              printf("Botao 1 precionado.\n");
              leds_toggle(LEDS_RED);
          } else if (data == &button_right_sensor){ //botao 2
              printf("Botao 2 precionado.\n");
              leds_toggle(LEDS_GREEN);
          }
      }
  }

  SENSORS_DEACTIVATE(button_sensor);
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
