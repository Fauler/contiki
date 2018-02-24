
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

#define DEBUG true

static struct sensors_sensor *sensor;
static struct etimer et;

static int flagParada = 0;
static int idx = 0, timeOut = 0;
static int reads[4] = {-1,-1,-1,-1};

/*---------------------------------------------------------------------------*/
PROCESS(hwsensors_process, "hwsensors process");
AUTOSTART_PROCESSES(&hwsensors_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(hwsensors_process, ev, data) {

    PROCESS_BEGIN();

    etimer_set(&et, CLOCK_SECOND * 0.15);

    sensor = sensors_find(ADC_SENSOR);
    sensor->configure(ADC_SENSOR_SET_CHANNEL, ADC_COMPB_IN_AUXIO7);

    IOCPinTypeGpioOutput(IOID_11);
    IOCPinTypeGpioOutput(IOID_12);
    IOCPinTypeGpioOutput(IOID_13);
    IOCPinTypeGpioOutput(IOID_14);
    IOCPinTypeGpioOutput(IOID_15);

    while(1) {

        PROCESS_YIELD();

         if(ev == PROCESS_EVENT_TIMER){

             SENSORS_ACTIVATE(*sensor);
             int read = sensor->value(ADC_SENSOR_VALUE);
             SENSORS_DEACTIVATE(*sensor);

             int volts = read/1000000;

             GPIO_clearDio(7);//Led onbord verde
             GPIO_clearDio(6);//Led onbord vermelho

             /*Leitura*/
             if(volts > 0 || idx > 2){
                 if (idx <= 2) GPIO_setDio(7);//Led onbord verde
                 else if (idx > 3) idx = 0;
                 timeOut = 0;
                 reads[idx++] = volts;
                 if (DEBUG) {
                     int decimal = read%1000000/1000;
                     printf("Sensor: %d,%d V\n", volts,decimal);
                     printf("Vetor: %d|%d|%d|%d\n", reads[0],reads[1],reads[2],reads[3]);
                 }
             } else if(timeOut++ > 15 &&
                     (reads[0] > 0 || reads[1] > 0 || reads[2] > 0 || reads[3] > 0)) {
                 GPIO_clearMultiDio(GPIO_DIO_ALL_MASK);
                 GPIO_setDio(6);//Led onbord vermelho
                 timeOut = 0;
                 flagParada = 0;
                 idx = 0;
                 reads[0] = -1; reads[1] = -1; reads[2] = -1; reads[3] = -1;
                 if (DEBUG) printf("Vetor: %d|%d|%d|%d\n", reads[0],reads[1],reads[2],reads[3]);
             }

             /*Ações*/
             if (reads[3] == 0) { //Elevador em movimento

                if (reads[0] == 1 && reads[2] == 3) { //Elevador subiu
                    GPIO_setDio(12);
                    printf("[Sensor 01] Elevador passou por aqui subindo.\n");
                } else if (reads[0] == 3 && reads[2] == 1) { //Elevador desceu
                    GPIO_setDio(11);
                    printf("[Sensor 01] Elevador passou por aqui decendo.\n");
                }

             } else if (reads[3] > 0 && flagParada == 0) { // Elevador parado

                 flagParada = 1;
                 switch (reads[3]){
                 case 1: // Parou abaixo do nível
                     GPIO_setDio(15);
                     printf("[Sensor 01] Elevador parou abaixo do nivel!\n");
                     break;
                 case 2: // Parou no nivel
                     GPIO_setDio(14);
                     printf("[Sensor 01] Elevador no nivel. ABRIR PORTA.\n");
                     break;
                 case 3: // Parou acima do nível
                     GPIO_setDio(13);
                     printf("[Sensor 01] Elevador parou acima do nivel!\n");
                     break;
                 }

             }

             etimer_reset(&et);
         }

    }

    PROCESS_END();
}
