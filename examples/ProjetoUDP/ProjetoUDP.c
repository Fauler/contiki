#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/rpl/rpl.h"
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

//Para imprimir o IPV6
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define CONN_PORT (8802)
#define DEBUG true
#define MAX_PAYLOAD_LEN 120

#define LED_TOGGLE_REQUEST (0x79)
#define LED_SET_STATE (0x7A)
#define LED_GET_STATE (0x7B)
#define LED_STATE (0x7C)

#define ABRIR_PORTA (0x6A)
#define LER_SENSOR (0x6B)

static struct sensors_sensor *sensor;
static struct etimer et;
static struct uip_udp_conn *server_conn;

static int flagParada = 0;
static int idx = 0, timeOut = 0;
static int reads[4] = {-1,-1,-1,-1};


/*---------------------------------------------------------------------------*/
PROCESS(projetoUDP_process, "projetoUDP process");
AUTOSTART_PROCESSES(&projetoUDP_process);
/*---------------------------------------------------------------------------*/

static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  printf("Server IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {

      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\nFim");
    }
  }
}

static void event_timer_handler(){

    //Ativar um Sensor
    SENSORS_ACTIVATE(*sensor);
    //Ler entrada do Sensor
    int read = sensor->value(ADC_SENSOR_VALUE);
    //Desativa o Sensor
    SENSORS_DEACTIVATE(*sensor);

    //Convertendo a entrada que está em Micro Volts para Volts
    int volts = read/1000000;

    //Apagando as LEDS RED and GREEN
    GPIO_clearDio(7);//Led onbord verde
    GPIO_clearDio(6);//Led onbord vermelho

    //#############################Regra de Negocio##############################
    //####[Analise]####
    if(volts > 0 || idx > 2){
        if (idx <= 2) GPIO_setDio(7);//Led onbord verde
        else if (idx > 3) idx = 0;
        timeOut = 0;
        reads[idx++] = volts;
        if (DEBUG) {
            int decimal = read%1000server_conn000/1000;
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

    //####[Resultado]####
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

    //Reseta o evento de tempo
    etimer_reset(&et);

}

static void tcpip_handler_func(void)
{
    char* msg = (char*)uip_appdata;
    int i;

    if(uip_newdata()) {
        ((char *)uip_appdata)[uip_datalen()] = 0;
        printf("MSG Recebida: '%s'", msg);
        printf("\n");


        switch (msg[0])
        {
            case ABRIR_PORTA:
            {
                printf("\nAbrir a porta");
                break;
            }
            case LER_SENSOR:
            {
                printf("\nLer Sensor");
                break;
            }
            default:
            {
                printf("Comando Invalido: ");
                for(i=0;i<uip_datalen();i++)
                {
                    printf("0x%02X ",msg[i]);
                }
                printf("\n");
                break;
            }
        }
    }

    return;
}


PROCESS_THREAD(projetoUDP_process, ev, data) {


    #if UIP_CONF_ROUTER
      uip_ipaddr_t ipaddr;
      rpl_dag_t *dag;
    #endif /* UIP_CONF_ROUTER */


    PROCESS_BEGIN();

    #if UIP_CONF_ROUTER
      uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
      uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
      uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
    #endif

    //Print IP
    print_local_addresses();

    //Criando um evento de tempo a cada 15MS
    etimer_set(&et, CLOCK_SECOND * 0.15);

    //Configurando um sensor do tipo ADC
    sensor = sensors_find(ADC_SENSOR);
    //Definindo um PINO de ENTRADA
    sensor->configure(ADC_SENSOR_SET_CHANNEL, ADC_COMPB_IN_AUXIO7);

    //Definino pinos de Saidas
    IOCPinTypeGpioOutput(IOID_11);
    IOCPinTypeGpioOutput(IOID_12);
    IOCPinTypeGpioOutput(IOID_13);
    IOCPinTypeGpioOutput(IOID_14);
    IOCPinTypeGpioOutput(IOID_15);

    //Definir porta a ser escutada
    server_conn = udp_new(NULL, UIP_HTONS(CONN_PORT), NULL);
    udp_bind(server_conn, UIP_HTONS(CONN_PORT));

    while(1) {
        //Liberando o processador
        PROCESS_YIELD();

        //
        if(ev == tcpip_event) {
            printf("\nEvento de TCP");
            tcpip_handler_func();
        }

        else
        //Aguardando um evento de tempo
         if(ev == PROCESS_EVENT_TIMER){
             event_timer_handler();
         }

    }

    PROCESS_END();
}
