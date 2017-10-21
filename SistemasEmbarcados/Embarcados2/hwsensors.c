#include "contiki.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "dev/leds.h"
#include "dev/watchdog.h"
#include "dev/adc-sensor.h"
#include "random.h"
#include "button-sensor.h"
#include "board-peripherals.h"

#include "ti-lib.h"

#include <stdio.h>
#include <stdint.h>

static struct ctimer ct;
static struct etimer et_hello;

static int32_t leds=0;


/*---------------------------------------------------------------------------*/
PROCESS(read_button_process, "read button process");
AUTOSTART_PROCESSES(&read_button_process);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(read_button_process, ev, data)
{
    PROCESS_BEGIN();
    printf("Vai IoT\n");

    // configuração da porta como output
    IOCPinTypeGpioOutput(IOID_8);
    IOCPinTypeGpioOutput(IOID_7);


    while(1){
        PROCESS_YIELD();

        if(ev == sensors_event){
            if(data == &button_left_sensor){
                printf("Left Button!\n");
                GPIO_writeDio(IOID_8,1);
                GPIO_writeDio(IOID_7,0);
            }
            else if(data == &button_right_sensor){
                leds_toggle(LEDS_GREEN);
                GPIO_writeDio(IOID_8,0);
                printf("Right Button!\n");
                GPIO_writeDio(IOID_7,1);
            }
        }
        PROCESS_END();
    }
    return 0;
}
