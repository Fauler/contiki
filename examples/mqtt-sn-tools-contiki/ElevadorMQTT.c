#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-nameserver.h"
#include "mqtt-sn.h"
#include "rpl.h"
#include "net/ip/resolv.h"
#include "net/rime/rime.h"
#include "simple-udp.h"

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#include <stdio.h>
#include <string.h>
#include "dev/leds.h"

#include "board-peripherals.h"
#include "dev/adc-sensor.h"
#include "dev/watchdog.h"


#define UDP_PORT 1883


#define LED_ELEVADOR_0 IOID_29
#define LED_ELEVADOR_1 IOID_28
#define LED_ELEVADOR_2 IOID_27
#define LED_ELEVADOR_3 IOID_26
#define LED_ELEVADOR_4 IOID_25


#define REQUEST_RETRIES 4
#define DEFAULT_SEND_INTERVAL		(10 * CLOCK_SECOND)
#define REPLY_TIMEOUT (3 * CLOCK_SECOND)

static struct mqtt_sn_connection mqtt_sn_c;
static char mqtt_client_id[17];

static char elevadorStateTopic[23] = "0000000000000000/status\0";
static uint16_t elevadorStateID;
static uint16_t elevadorStateMsgID;


static char elevadorMovimentarTopic[27] = "0000000000000000/movimentar\0";
static uint16_t elevadorMovimentarID;
static uint16_t elevadorMovimentarMsgID;


//static char elevadorSensorTopic[23] = "0000000000000000/sensor\0";
static uint16_t elevadorSensorID;
static uint16_t elevadorSensorMsgID;


static publish_packet_t incoming_packet;
static uint16_t mqtt_keep_alive=10;
static int8_t qos = 1;
static uint8_t retain = FALSE;
static char device_id[17];
static clock_time_t send_interval;
static mqtt_sn_subscribe_request subreq;
static mqtt_sn_register_request regreq;
//uint8_t debug = FALSE;

static enum mqttsn_connection_status connection_state = MQTTSN_DISCONNECTED;

/*A few events for managing device state*/
static process_event_t mqttsn_connack_event;


//==========[LEDS]===============
static void acenderLed(int led){
    GPIO_writeDio(led,1);//TODO VAlidar o que acontece aqui
}

static void apagarLed(int led){
    GPIO_writeDio(led,0);//TODO VAlidar o que acontece aqui
}

static void apagarLedsElevador(){
	apagarLed(LED_ELEVADOR_0);
	apagarLed(LED_ELEVADOR_1);
	apagarLed(LED_ELEVADOR_2);
	apagarLed(LED_ELEVADOR_3);
	apagarLed(LED_ELEVADOR_4);
}




//==========[Constantes para Logica]===============

#define IMPRIMIR  1

#define MAX_ANDARES  5
#define MIN_ANDARES  0

#define PARAR  1
#define NAO_PARAR 0

#define ABERTA 1
#define FECHADA 0

#define ABAIXO_NIVEL -1
#define NO_NIVEL 0
#define ACIMA_NIVEL 1

//==========[Estruturas de logica]===============
static struct Porta {
   int status;
   int nivel;
};

static struct Andar {
   int ultimo;
   int atual;
   int proximo;
};

//==========[Variaveis Globais]===============
static struct Porta porta;
static struct Andar andar;
static int andaresParar[MAX_ANDARES] = {0,0,0,0,0};
static char *ANDAR = "ANDAR";
static char *GET_STATE = "GET_STATUS";

//==========[Funções de logica]===============
static int random_number(int min_num, int max_num){
//    if(IMPRIMIR) printf("\n\n====[random_number]====");
//    int result = 0;
//
//    srand(time(NULL));
//    result = (rand() % (max_num + 1 - min_num)) + min_num;
//    return result;
	return NO_NIVEL;
}
static void setPorta(int status){
    if(IMPRIMIR) printf("\n\n====[setPorta]====");
    porta.status = status;
    if(status == ABERTA) {
        porta.nivel = random_number(ABAIXO_NIVEL, ACIMA_NIVEL);
    } else {
        porta.nivel = random_number(ABAIXO_NIVEL, ACIMA_NIVEL);
    }
}

static void setAndar(int andarIR){
    if(IMPRIMIR) printf("\n\n====[setAndar]====");
    if(andarIR >= 0 && andarIR < MAX_ANDARES) {
        if(andarIR != andar.atual){
            //Setar que tem que parar
            andaresParar[andarIR] = PARAR;
        }
    }
}
static int getProximoAndar(){
    if(IMPRIMIR) printf("\n\n====[getProximoAndar]====");
    int i;
    if(andar.ultimo <= andar.atual){
        for(i = andar.atual; i < MAX_ANDARES ; i++) {
            if(andaresParar[i] == PARAR){
                return i;
            }
        }
        for(i = andar.atual-1; i >= MIN_ANDARES ; i--) {
            if(andaresParar[i] == PARAR){
                return i;
            }
        }
    } else {
        for(i = andar.atual; i >= MIN_ANDARES ; i--) {
            if(andaresParar[i] == PARAR){
                return i;
            }
        }
        for(i = andar.atual+1; i < MAX_ANDARES ; i++) {
            if(andaresParar[i] == PARAR){
                return i;
            }
        }
    }
    //Siginifica que nao ha mais andares para ir
    return andar.atual;
}

static void movimentar(){
    if(IMPRIMIR) printf("\n\n====[movimentar]====");
    int ultimoAndar = andar.atual;
    if(andar.atual == andar.proximo) {
        andar.proximo = getProximoAndar();
        andaresParar[andar.atual] = NAO_PARAR;
        setPorta(ABERTA);
    } else {
        setPorta(FECHADA);
        if(andar.atual > andar.ultimo){
            if(andar.atual < MAX_ANDARES){
                andar.atual++;
            } else {
                andar.atual--;
            }
        } else {
            if(andar.atual < andar.ultimo){
                if(andar.atual > MIN_ANDARES){
                    andar.atual--;
                } else {
                    andar.atual++;
                }
            } else {
                if(andar.atual < andar.proximo) {
                    if(andar.atual < MAX_ANDARES){
                        andar.atual++;
                    } else {
                        andar.atual--;
                    }
                } else {
                    if(andar.atual > MIN_ANDARES){
                        andar.atual--;
                    } else {
                        andar.atual++;
                    }
                }
            }
        }
    }
    andar.ultimo = ultimoAndar;
    //Acender LEDS
	apagarLedsElevador();
    switch(andar.atual){
    	case 0:
    		acenderLed(LED_ELEVADOR_0);
    		break;
		case 1:
			acenderLed(LED_ELEVADOR_1);
			break;
		case 2:
			acenderLed(LED_ELEVADOR_2);
			break;
		case 3:
			acenderLed(LED_ELEVADOR_3);
			break;
		case 4:
			acenderLed(LED_ELEVADOR_4);
			break;
		default:
			break;

    }
}

static void printPorta(){
    if(IMPRIMIR) printf("\n\n====[printPorta]====");
    printf("\n\nPorta.status: %d", porta.status);
    printf("\nPorta.nivel: %d\n\n", porta.nivel);
}

static void printAndaresParar(){
    if(IMPRIMIR) printf("\n\n====[printAndaresParar]====");
    int i;
    for(i = 0; i < MAX_ANDARES ; i++) {
        printf("\nAndar[%d] + %d", i, andaresParar[i]);
    }
}

static void printAndar(){
    if(IMPRIMIR) printf("\n\n====[printAndar]====");
    printf("\n\nAndar.ultimo: %d", andar.ultimo);
    printf("\nAndar.atual: %d", andar.atual);
    printf("\nAndar.proximo: %d\n\n", andar.proximo);
}

static void printAll(){
    printAndaresParar();
    printAndar();
    //printPorta();
}

static void iniciar(){
    if(IMPRIMIR) printf("\n\n====[iniciar]====");
    setPorta(FECHADA);
    setAndar(0);
}

static void enviarMSG(char *bufff){

	printf("\nEnviar MSG no Topic: %s -> msg: %s\n", elevadorMovimentarTopic, bufff);
	uint8_t buf_len = strlen(bufff);
	mqtt_sn_send_publish(&mqtt_sn_c, elevadorMovimentarID,MQTT_SN_TOPIC_TYPE_NORMAL,bufff, buf_len,qos,retain);
}

PROCESS(example_mqttsn_process, "Configure Connection and Topic Registration");//PROCESSO MAIN
PROCESS(elevadorMovimentar_process, "register topic and publish data");//PROCESSO QUE FICA LENDO O TOPICO elevadorMovimentar
PROCESS(elevadorState_subscription_process, "subscribe to a device control channel");//PROCESSO QUE FICA LENDO O TOPICO elevadorState e elevadorSensor


AUTOSTART_PROCESSES(&example_mqttsn_process);

/*---------------------------------------------------------------------------*/
static void
puback_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  printf("Puback received\n");
}
/*---------------------------------------------------------------------------*/
static void
connack_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  uint8_t connack_return_code;
  connack_return_code = *(data + 3);
  printf("Connack received\n");
  if (connack_return_code == ACCEPTED) {
    process_post(&example_mqttsn_process, mqttsn_connack_event, NULL);
  } else {
    printf("Connack error: %s\n", mqtt_sn_return_code_string(connack_return_code));
  }
}
/*---------------------------------------------------------------------------*/
static void
regack_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  regack_packet_t incoming_regack;
  memcpy(&incoming_regack, data, datalen);
  printf("Regack received\n");
  if (incoming_regack.message_id == elevadorMovimentarMsgID) {
    if (incoming_regack.return_code == ACCEPTED) {
      elevadorMovimentarID = uip_htons(incoming_regack.topic_id);
    } else {
      printf("Regack error: %s\n", mqtt_sn_return_code_string(incoming_regack.return_code));
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
suback_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  suback_packet_t incoming_suback;
  memcpy(&incoming_suback, data, datalen);
/*
	printf("\nID DA MSG: %d - CODE: %d", incoming_suback.message_id, incoming_suback.return_code);
	printf("\nelevadorStateMsgID: %d", elevadorStateMsgID);
	printf("\nelevadorMovimentarMsgID: %d", elevadorMovimentarMsgID);
	printf("\nelevadorSensorMsgID: %d", elevadorSensorMsgID);
*/
	if (incoming_suback.message_id == elevadorStateMsgID) {
		if (incoming_suback.return_code == ACCEPTED) {
			elevadorStateID = uip_htons(incoming_suback.topic_id);
		} else {
			printf("Suback error: %s\n", mqtt_sn_return_code_string(incoming_suback.return_code));
		}
	} else if (incoming_suback.message_id == elevadorMovimentarMsgID) {
		if (incoming_suback.return_code == ACCEPTED) {
			elevadorMovimentarID = uip_htons(incoming_suback.topic_id);
		} else {
			printf("Suback error: %s\n", mqtt_sn_return_code_string(incoming_suback.return_code));
		}
	} else if (incoming_suback.message_id == elevadorSensorMsgID) {
		if (incoming_suback.return_code == ACCEPTED) {
			elevadorSensorID = uip_htons(incoming_suback.topic_id);
		} else {
			printf("Suback error: %s\n", mqtt_sn_return_code_string(incoming_suback.return_code));
		}
	}
/*
	printf("\nID DA topic_id: %d", incoming_suback.topic_id);
	printf("\n elevadorStateID: %d", elevadorStateID);
	printf("\n elevadorMovimentarID: %d", elevadorMovimentarID);
	printf("\n elevadorSensorID: %d", elevadorSensorID);*/
}
/*---------------------------------------------------------------------------*/
static void
publish_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  //publish_packet_t* pkt = (publish_packet_t*)data;
  memcpy(&incoming_packet, data, datalen);
  incoming_packet.data[datalen-7] = 0x00;
  char *msg = incoming_packet.data;
  printf("\n RECEBI MSG: %s\n", msg);

  /*printf("\nTopico_ID: %d", incoming_packet.topic_id);
  printf("\nelevadorStateID: %d", elevadorStateID);
  printf("\nelevadorSensorID: %d", elevadorSensorID);
*/
  //if (uip_htons(incoming_packet.topic_id) == elevadorStateID
	//	  || uip_htons(incoming_packet.topic_id) == elevadorSensorID) {

	  char *retorno = strstr(msg, GET_STATE);
	  if (retorno){
		  //Pisca os Leds
		  leds_toggle(LEDS_ALL);
		  	char mensagem[1024];
		  	if(andar.atual < andar.proximo){
		  		sprintf(mensagem, "Andar atual: [%d]\t [SUBINDO]", andar.atual);
		  	} else if(andar.atual > andar.proximo){
		  		sprintf(mensagem, "Andar atual: [%d]\t [DESCENDO]", andar.atual);
		  	} else {
		  		sprintf(mensagem, "Andar atual: [%d]\t [PARADO]", andar.atual);
		  	}

			enviarMSG(mensagem);
			printf("Enviar MSG - GET_STATUS");
	  } else {
		  int andar = msg[0]-'0';
		  printf("RECEBEU ANDAR: %d", andar);
		  if(andar >= MIN_ANDARES && andar < MAX_ANDARES) {
			  //setAndar(andar);
			  andaresParar[andar] = PARAR;
			  printAndaresParar();
		  }
	  }


      //send_interval = 10 * CLOCK_CONF_SECOND;
  //} else {
  //  printf("unknown publication received\n");
  //}

}
/*---------------------------------------------------------------------------*/
static void
pingreq_receiver(struct mqtt_sn_connection *mqc, const uip_ipaddr_t *source_addr, const uint8_t *data, uint16_t datalen)
{
  printf("PingReq received\n");
}
/*---------------------------------------------------------------------------*/
/*Add callbacks here if we make them*/
static const struct mqtt_sn_callbacks mqtt_sn_call = {
  publish_receiver,
  pingreq_receiver,
  NULL,
  connack_receiver,
  regack_receiver,
  puback_receiver,
  suback_receiver,
  NULL,
  NULL
  };

/*---------------------------------------------------------------------------*/
/* Processo para Enviar uma MSG*/
PROCESS_THREAD(elevadorMovimentar_process, ev, data)
{
  static uint8_t registration_tries;
  static mqtt_sn_register_request *rreq = &regreq;

  PROCESS_BEGIN();
	  memcpy(elevadorMovimentarTopic,device_id,16);
	  registration_tries =0;


	  while (registration_tries < REQUEST_RETRIES)
	  {

		elevadorMovimentarMsgID = mqtt_sn_register_try(rreq,&mqtt_sn_c,elevadorMovimentarTopic,REPLY_TIMEOUT);

		printf("\n\nMe Registrei MSG_ID[%d] Topic: [%s]\n", elevadorMovimentarMsgID, elevadorMovimentarTopic);

		PROCESS_WAIT_EVENT_UNTIL(mqtt_sn_request_returned(rreq));
		if (mqtt_sn_request_success(rreq)) {
		  registration_tries = 4;
		  printf("\nRegistrado no Topico %s", elevadorMovimentarTopic);
		}
		else {
		  registration_tries++;
		  if (rreq->state == MQTTSN_REQUEST_FAILED) {
			  printf("Regack error: %s\n", mqtt_sn_return_code_string(rreq->return_code));
		  }
		}
	  }

	  //Aqui já estamos registrado
	  if (mqtt_sn_request_success(rreq)){
		  char buff[1024];
		  sprintf(buff, "Fauler - MINHA MSG 000");
		  enviarMSG(buff);

	  } else {
		printf("\nNao foi possivel se registrar no topico: %s", elevadorMovimentarTopic);
	  }
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/*this process will create a subscription and monitor for incoming traffic*/
PROCESS_THREAD(elevadorState_subscription_process, ev, data)
{
  static uint8_t subscription_tries;
  static mqtt_sn_subscribe_request *sreq = &subreq;

  PROCESS_BEGIN();
	  subscription_tries = 0;

	  //Aqui copia para a variavel elevadorStateTopic o MAC do Dispositivo
	  memcpy(elevadorStateTopic,device_id,16);

	  while(subscription_tries < REQUEST_RETRIES)
	  {
		  //Aqui obtem o id de inscricao
		  //eh feito a inscricao  passando os param (Struct de submit,	conexao  ,  o topicoInscrito, QOS, TIME_OUT
		  elevadorStateMsgID = mqtt_sn_subscribe_try(sreq,              &mqtt_sn_c,  elevadorStateTopic,         0,REPLY_TIMEOUT);

		  printf("\n\nMe Registrei MSG_ID[%d] Topic: [%s]\n", elevadorStateMsgID, elevadorStateTopic);

		  PROCESS_WAIT_EVENT_UNTIL(mqtt_sn_request_returned(sreq));
		  if (mqtt_sn_request_success(sreq)) {
			  subscription_tries = 4;
			  printf("\n\nRegistrado com Sucesso");
		  }
		  else {
			  subscription_tries++;
			  if (sreq->state == MQTTSN_REQUEST_FAILED) {
				  printf("Suback error: %s\n", mqtt_sn_return_code_string(sreq->return_code));
			  }
		  }
	  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/*this main process will create connection and register topics*/
/*---------------------------------------------------------------------------*/


static struct ctimer connection_timer;
static process_event_t connection_timeout_event;

static void connection_timer_callback(void *mqc)
{
  process_post(&example_mqttsn_process, connection_timeout_event, NULL);
}

/*---------------------------------------------------------------------------*/
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Client IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
static resolv_status_t
set_connection_address(uip_ipaddr_t *ipaddr)
{
#ifndef UDP_CONNECTION_ADDR
#if RESOLV_CONF_SUPPORTS_MDNS
//#define UDP_CONNECTION_ADDR       sdtdf.com.brr//Guilherme
#define UDP_CONNECTION_ADDR       pksr.eletrica.eng.br//OHARA
//#define UDP_CONNECTION_ADDR       iot.eclipse.org//ENDERECO DO MQTT-SERVER
#elif UIP_CONF_ROUTER
#define UDP_CONNECTION_ADDR       fd00:0:0:0:0212:7404:0004:0404
#else
#define UDP_CONNECTION_ADDR       fe80:0:0:0:6466:6666:6666:6666
#endif
#endif /* !UDP_CONNECTION_ADDR */

#define _QUOTEME(x) #x
#define QUOTEME(x) _QUOTEME(x)

  resolv_status_t status = RESOLV_STATUS_ERROR;

  if(uiplib_ipaddrconv(QUOTEME(UDP_CONNECTION_ADDR), ipaddr) == 0) {
    uip_ipaddr_t *resolved_addr = NULL;
    status = resolv_lookup(QUOTEME(UDP_CONNECTION_ADDR),&resolved_addr);
    if(status == RESOLV_STATUS_UNCACHED || status == RESOLV_STATUS_EXPIRED) {
      PRINTF("Attempting to look up %s\n",QUOTEME(UDP_CONNECTION_ADDR));
      resolv_query(QUOTEME(UDP_CONNECTION_ADDR));
      status = RESOLV_STATUS_RESOLVING;
    } else if(status == RESOLV_STATUS_CACHED && resolved_addr != NULL) {
      PRINTF("Lookup of \"%s\" succeded!\n",QUOTEME(UDP_CONNECTION_ADDR));
    } else if(status == RESOLV_STATUS_RESOLVING) {
      PRINTF("Still looking up \"%s\"...\n",QUOTEME(UDP_CONNECTION_ADDR));
    } else {
      PRINTF("Lookup of \"%s\" failed. status = %d\n",QUOTEME(UDP_CONNECTION_ADDR),status);
    }
    if(resolved_addr)
      uip_ipaddr_copy(ipaddr, resolved_addr);
  } else {
    status = RESOLV_STATUS_CACHED;
  }

  return status;
}



PROCESS_THREAD(example_mqttsn_process, ev, data)
{

  static struct etimer periodic_timer;
  static struct etimer et;
  static uip_ipaddr_t broker_addr,google_dns;
  static uint8_t connection_retries = 0;
  char contiki_hostname[16];

  PROCESS_BEGIN();

#if RESOLV_CONF_SUPPORTS_MDNS
#ifdef CONTIKI_CONF_CUSTOM_HOSTNAME
  sprintf(contiki_hostname,"node%02X%02X",linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);
  resolv_set_hostname(contiki_hostname);
  PRINTF("Setting hostname to %s\n",contiki_hostname);
#endif
#endif

  mqttsn_connack_event = process_alloc_event();

  mqtt_sn_set_debug(1);
  uip_ip6addr(&google_dns, 0x2001, 0x4860, 0x4860, 0x0, 0x0, 0x0, 0x0, 0x8888);//172.16.220.1 with tayga
  etimer_set(&periodic_timer, 2*CLOCK_SECOND);
  while(uip_ds6_get_global(ADDR_PREFERRED) == NULL)
  {
    PROCESS_WAIT_EVENT();
    if(etimer_expired(&periodic_timer))
    {
        PRINTF("Aguardando auto-configuracao de IP\n");
        etimer_set(&periodic_timer, 2*CLOCK_SECOND);
    }
  }

  print_local_addresses();




  rpl_dag_t *dag = rpl_get_any_dag();
  if(dag) {
    //uip_ipaddr_copy(sixlbr_addr, globaladdr);
    uip_nameserver_update(&google_dns, UIP_NAMESERVER_INFINITE_LIFETIME);
  }

  static resolv_status_t status = RESOLV_STATUS_UNCACHED;
  while(status != RESOLV_STATUS_CACHED) {
    status = set_connection_address(&broker_addr);

    if(status == RESOLV_STATUS_RESOLVING) {
      PROCESS_WAIT_EVENT_UNTIL(ev == resolv_event_found);
    } else if(status != RESOLV_STATUS_CACHED) {
      PRINTF("Can't get connection address.\n");
      PROCESS_YIELD();
    }
  }

  //uip_ip6addr(&broker_addr, 0x2804,0x7f4,0x3b80,0xcdf7,0x241b,0x1ab2,0xa46a,0x9912);//172.16.220.128 with tayga

  mqtt_sn_create_socket(&mqtt_sn_c,UDP_PORT, &broker_addr, UDP_PORT);
  (&mqtt_sn_c)->mc = &mqtt_sn_call;

  sprintf(device_id,"%02X%02X%02X%02X%02X%02X%02X%02X",linkaddr_node_addr.u8[0],
          linkaddr_node_addr.u8[1],linkaddr_node_addr.u8[2],linkaddr_node_addr.u8[3],
          linkaddr_node_addr.u8[4],linkaddr_node_addr.u8[5],linkaddr_node_addr.u8[6],
          linkaddr_node_addr.u8[7]);

  printf("\n\nMeu DeviceID é: %s\n\n",device_id);

  sprintf(mqtt_client_id,"sens%02X%02X%02X%02X",linkaddr_node_addr.u8[4],linkaddr_node_addr.u8[5],linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);


  /*Request a connection and wait for connack*/
  printf("requesting connection \n ");
  connection_timeout_event = process_alloc_event();
  //testegoto:
  //connection_retries = 0;
  ctimer_set( &connection_timer, REPLY_TIMEOUT, connection_timer_callback, NULL);
  mqtt_sn_send_connect(&mqtt_sn_c,mqtt_client_id,mqtt_keep_alive);
  connection_state = MQTTSN_WAITING_CONNACK;


  //Espera para ter conexão com o Servidor
  while (connection_retries < 15) {
	PROCESS_WAIT_EVENT();

		if (ev == mqttsn_connack_event) {
			//if success
			printf("connection acked\n");
			ctimer_stop(&connection_timer);
			connection_state = MQTTSN_CONNECTED;
			connection_retries = 15; //using break here may mess up switch statement of proces
		}
		if (ev == connection_timeout_event) {
			connection_state = MQTTSN_CONNECTION_FAILED;
			connection_retries++;
			printf("connection timeout...%d\n", connection_retries);
			ctimer_restart(&connection_timer);
			if (connection_retries < 15) {
				mqtt_sn_send_connect(&mqtt_sn_c, mqtt_client_id, mqtt_keep_alive);
				connection_state = MQTTSN_WAITING_CONNACK;
			}
		}
	}

  //Já temos CONEXÃO COM o Servidor
  //   COMECO DO PROGRAMA
  //    MAIN()
	ctimer_stop(&connection_timer);
	if (connection_state == MQTTSN_CONNECTED) {
		//Aqui comeca o processo que fica lendo o topico elevadorState
		process_start(&elevadorState_subscription_process, 0);

		//Aqui comeca o processo que fica lendo o topico elevadorMovimentar
		process_start(&elevadorMovimentar_process, 0);

		//Cria um evento de tempo de 3 clockSecond
		etimer_set(&periodic_timer, 3 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

		//Seta o evento de tempo a cada 10 ClockSecond
		etimer_set(&et, 3 * CLOCK_SECOND);


		//vamos inciar as variaveis
		iniciar();

		//Definino pinos de Saidas
		IOCPinTypeGpioOutput(LED_ELEVADOR_0);
		IOCPinTypeGpioOutput(LED_ELEVADOR_1);
		IOCPinTypeGpioOutput(LED_ELEVADOR_2);
		IOCPinTypeGpioOutput(LED_ELEVADOR_3);
		IOCPinTypeGpioOutput(LED_ELEVADOR_4);

		while (1) {
			//Espera um evento
			PROCESS_WAIT_EVENT();
			//Caso seja um evento de TEMPO
			if (etimer_expired(&et)) {
				//Aqui efetua um evento de TEMPO
				movimentar();
				printAll();

				if(!(andar.atual == andar.proximo && andar.atual == andar.ultimo)){
					//Enviar MSG
					char mensagem[1024];
					sprintf(mensagem, "Elevador:\nDe:[%d]\tPara:[%d]", andar.ultimo,andar.atual);
					enviarMSG(mensagem);
				}
				etimer_restart(&et);
			}
		}
	} else {
		printf("\n\nNAO foi possivel conectar\n");
	}
	PROCESS_END();
}
