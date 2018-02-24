#include "contiki.h"
#include <stdio.h> /* For printf() */
#include "dev/leds.h"
#include "button-sensor.h"

static struct etimer et_hello;
static struct etimer et_blink;
static struct etimer et_proc3;

#define LED_PING_EVENT (44)
#define LED_PONG_EVENT (45)

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
PROCESS(blink_process, "Blink process");
PROCESS(proc3_process, "Proc3 process");
PROCESS(pong_process, "Pong process");
PROCESS(read_button_process, "Read Button process");
AUTOSTART_PROCESSES(&hello_world_process, &blink_process, &proc3_process, &pong_process, &read_button_process);


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{
    PROCESS_BEGIN();

    //Definino Periodo de tempo
    etimer_set(&et_hello, 15*CLOCK_SECOND);

    while(1){
        PROCESS_WAIT_EVENT();
        if(ev == PROCESS_EVENT_TIMER){
            etimer_reset(&et_hello);
            process_post(&pong_process, LED_PING_EVENT, (void*)(&hello_world_process));
            printf("\n\nProcesso HELLO - Enviando PING.");
        } else if(ev == LED_PONG_EVENT){
            printf("\nProcesso HELLO- Recebido ping do processo (%s).", ((struct process*)data)->name);

        }
    }


    PROCESS_END();
}

PROCESS_THREAD(blink_process, ev, data)
{
    PROCESS_BEGIN();

    //Definino Periodo de tempo
    etimer_set(&et_blink, 8*CLOCK_SECOND);

    leds_off(LEDS_GREEN);
    while(1){
        PROCESS_WAIT_EVENT();
        if(ev == PROCESS_EVENT_TIMER){
            leds_toggle(LEDS_GREEN);
            etimer_reset(&et_blink);
            process_post(&pong_process, LED_PING_EVENT, (void*)(&blink_process));
            printf("\n\nBLINK- Enviando PING.");
        } else if(ev == LED_PONG_EVENT){
            printf("\nProcesso BLINK- Recebido ping do processo (%s).", ((struct process*)data)->name);

        }
    }


    PROCESS_END();
}

PROCESS_THREAD(proc3_process, ev, data)
{
    PROCESS_BEGIN();

    //Definino Periodo de tempo
    etimer_set(&et_proc3, 20*CLOCK_SECOND);

    while(1){
        PROCESS_WAIT_EVENT();
        if(ev == PROCESS_EVENT_TIMER){
            etimer_reset(&et_proc3);
            process_post(&pong_process, LED_PING_EVENT, (void*)(&proc3_process));
            printf("\n\nProcesso 3 - Enviando PING.");
        } else if(ev == LED_PONG_EVENT){
            printf("\nProcesso 3 - Recebido ping do processo (%s).", ((struct process*)data)->name);

        }
    }


    PROCESS_END();
}


PROCESS_THREAD(pong_process, ev, data)
{
    PROCESS_BEGIN();

    while(1){
        PROCESS_WAIT_EVENT();
        if(ev == LED_PING_EVENT){
            process_post((struct process*)data, LED_PONG_EVENT, (void*)(&pong_process));
            printf("\nPONG _PROCESS- Recebido ping do processo (%s).", ((struct process*)data)->name);
        }
    }


    PROCESS_END();
}

PROCESS_THREAD(read_button_process, ev, data)
{
    PROCESS_BEGIN();
        while(1)
        {
            PROCESS_YIELD();

            if(ev == sensors_event){
                if(data == &button_left_sensor){
                    leds_toggle(LEDS_RED);
                    process_post(&pong_process, LED_PING_EVENT, (void*)(&read_button_process));
                    printf("\n\nButton - LEFT - Enviando PING.");
                }
                else if(data == &button_right_sensor){
                    leds_toggle(LEDS_GREEN);
                    process_post(&pong_process, LED_PING_EVENT, (void*)(&read_button_process));
                    printf("\n\nButton - RIGHT - Enviando PING.");
                }
            } else if(ev == LED_PONG_EVENT){
                printf("\nButton - Recebido ping do processo (%s).", ((struct process*)data)->name);

            }
        }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/
