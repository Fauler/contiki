#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>// necessário p/ as funções rand() e srand()
#include <time.h>//necessário p/ função time()
#include "board-peripherals.h"
#include "button-sensor.h"
#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "cpu/cc26xx-cc13xx/lpm.h"
#include "dev/adc-sensor.h"
#include "dev/leds.h"
#include "dev/watchdog.h"
#include "lib/sensors.h"
#include "net/rpl/rpl.h"


#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"
#include "random.h"
#include "ti-lib.h"

//Definicoes de Constantes
#define PORT_MSG 8802//Porta para MSG UDP
#define PORTA_DESTINO 8805//Porta para MSG UDP

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_UDP_BUF  ((struct uip_udp_hdr *)&uip_buf[UIP_LLH_LEN + UIP_IPH_LEN])


#define TAMANHO_BUFFER 1024

#define ABRIR_PORTA 0x3A
#define FECHAR_PORTA 0x3B
#define GET_STATUS 0x3C

#define STATUS 0x4A

#define ELEVADOR_EM_MOVIMENTO 0x5A
#define ELEVADOR_PARADO 0x5B
#define SET_ANDAR 0x5C

#define PORTA_ABERTA 0x6A
#define PORTA_FECHADA 0x6B

#define NIVEL_ELEVADOR_ABAIXO 0x6C
#define NIVEL_ELEVADOR_ACIMA 0x6D
#define NIVEL_ELEVADOR_CORRETO 0x6E

#define ANDAR_MINIMO 0
#define ANDAR_MAXIMO 2

#define QTD_MAX_ERROS_PERMITIDO 2

#define LED_ELEVADOR_MOVIMENTO_1 IOID_13
#define LED_ELEVADOR_MOVIMENTO_2 IOID_14
#define LED_ELEVADOR_MOVIMENTO_3 IOID_15


#define LED_NIVEL_ELEVADOR_ACIMA IOID_25
#define LED_NIVEL_ELEVADOR_CORRETO IOID_26
#define LED_NIVEL_ELEVADOR_ABAIXO IOID_27

#define LED_COR_PORTA_ABERTA 31
#define LED_COR_PORTA_FECHADA 32


//Estruturas para o projeto
static struct sensors_sensor *sensor;
static struct uip_udp_conn *server_conn;
static struct uip_udp_conn *conexaoDestino;
static uip_ipaddr_t ipDestino;

static struct StatusElevador {
	int  porta;
	int  elevador;
	int  nivelPorta;
   int  andarAtual;
   int	andarDestino;
} statusElevador;

static struct StatusElevador statusElevador;

//Variaveis Globais
static int porta = PORTA_ABERTA;
static int nivelElevador = NIVEL_ELEVADOR_CORRETO;
static int elevador = ELEVADOR_PARADO;
static int andarAtual = 0;
static int andarDestino = 0;

/*---------------------------------------------------------------------------*/
PROCESS(projetoUDP_process, "projetoUDP process");
AUTOSTART_PROCESSES(&projetoUDP_process);
/*---------------------------------------------------------------------------*/

//Metodo para imprimir o IPV6 do equipamento
static void print_local_addresses(void) {
	int i;
	uint8_t state;

	printf("Server IPv6 addresses: ");
	for (i = 0; i < UIP_DS6_ADDR_NB; i++) {
		state = uip_ds6_if.addr_list[i].state;
		if (uip_ds6_if.addr_list[i].isused
				&& (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {

			PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
			printf("\nFim");
		}
	}
}

static int randomLed()
{
	//srand(time(NULL));
	//return 0 + rand() % (3 + 1 - 0);

	return 1;
}


//Metodo para tratar um evento do tipo TCP/IP
static void eventoTcpIP(void) {
	printf("\n\nEvento TCP/IP - Recebido");

	//Convertendo a MSG de Bytes para uma String
	char* msg = (char*) uip_appdata;
	int i;

	//Validando se existe um novo conteudo
	if (uip_newdata()) {
		((char *) uip_appdata)[uip_datalen()] = 0;

		char operacao[50];

		switch (msg[0]) {
			case GET_STATUS: {
				strcpy(operacao, "\nGET_STATUS");
				printf(operacao);


				//Fazer uma Função que retornar um valor randomico par ao nivel do elevar 1, 2 e 3
				int nivel = randomLed();
				int respostaNivel;
				if(nivel == 1) {
					respostaNivel = NIVEL_ELEVADOR_ABAIXO;
				} else if(nivel == 2) {
					respostaNivel = NIVEL_ELEVADOR_CORRETO;
				} else {
					respostaNivel = NIVEL_ELEVADOR_ACIMA;
				}

				printf("\nNivel = %d", respostaNivel);

				break;
			}
			case SET_ANDAR: {
				strcpy(operacao, "\nSET ANDAR");
				printf(operacao);
				andarAtual = andarDestino;
				andarDestino = msg[1];

				break;
			}
			case PORTA_ABERTA: {
				strcpy(operacao, "\nPorta Aberta");
				printf(operacao);
				porta = PORTA_ABERTA;

				if(elevador == ELEVADOR_EM_MOVIMENTO && porta == PORTA_ABERTA){
					printf("\nA Porta nao pode abrir, pois o elevador esta em movimento\n\n");
					porta = PORTA_FECHADA;//Volta o status
				}

				break;
			}
			case PORTA_FECHADA: {
				strcpy(operacao, "\nPorta Fehada");
				printf(operacao);
				porta = PORTA_FECHADA;

				break;
			}
			case ELEVADOR_EM_MOVIMENTO: {
				printf("\nElevador Em Movimento");
				elevador = ELEVADOR_EM_MOVIMENTO;
				if(elevador == ELEVADOR_EM_MOVIMENTO && porta == PORTA_ABERTA){
					printf("\nO Elevador nao pode ir para Em Movimento, pois a Porta esta Aberta\n\n");
					elevador = ELEVADOR_PARADO;//Volta o Status
				}
				break;
			}
			case ELEVADOR_PARADO: {
				printf("\nElevador Parado");
				elevador = ELEVADOR_PARADO;
				break;
			}
			case NIVEL_ELEVADOR_ABAIXO: {
				printf("\nNivel do Elevador - Abaixo do Esperado");
				nivelElevador = NIVEL_ELEVADOR_ABAIXO;
				break;
			}
			case NIVEL_ELEVADOR_ACIMA: {
				printf("\nNivel do Elevador - Acima do Esperado");
				nivelElevador = NIVEL_ELEVADOR_ACIMA;
				break;
			}
			case NIVEL_ELEVADOR_CORRETO: {
				printf("\nNivel do Elevador - Correto");
				nivelElevador = NIVEL_ELEVADOR_CORRETO;
				break;
			}
			default: {
				printf("\nComando Invalido: ");
				for (i = 0; i < uip_datalen(); i++) {
					printf("0x%02X ", msg[i]);
				}
				printf("\n");
				break;
			}
		}
		printf("\nCriando MSG - ");

						//Enviar MSG MANUAL 2804:7f4:3b80:a336:5b28:1dc7:2166:7d4f
						uip_ip6addr(&ipDestino, 0x2804, 0x7f4, 0x3b80, 0xa336, 0x5b28, 0x1dc7, 0x2166, 0x7d4f);
						conexaoDestino = udp_new(&ipDestino, UIP_HTONS(PORTA_DESTINO), NULL);
						PRINT6ADDR(&conexaoDestino->ripaddr);
						udp_bind(conexaoDestino, UIP_HTONS(PORTA_DESTINO));
						//uip_udp_packet_send( conexaoDestino, minhaMSG, 2);
		//				uip_udp_packet_send( conexaoDestino, "aa", strlen("aa"));

						//Prepara o Buffer

						printf("\n\nMAndando MSG");
						statusElevador.elevador = elevador;
						statusElevador.nivelPorta = nivelElevador;
						statusElevador.porta = porta;
						statusElevador.andarAtual = andarAtual;
						statusElevador.andarDestino = andarDestino;

						//Envia MSG para o JAVA
						uip_udp_packet_send(conexaoDestino, &statusElevador, sizeof(struct StatusElevador));

						printf("\n\nMSG Enviada");

	}

	return;
}

static void acenderLed(int led){
    GPIO_writeDio(led,1);//TODO VAlidar o que acontece aqui
}

static void apagarLed(int led){
    GPIO_writeDio(led,0);//TODO VAlidar o que acontece aqui
}

static void apagarLedsElevador(){
	apagarLed(LED_ELEVADOR_MOVIMENTO_1);
	apagarLed(LED_ELEVADOR_MOVIMENTO_2);
	apagarLed(LED_ELEVADOR_MOVIMENTO_3);
	apagarLed(LED_NIVEL_ELEVADOR_ACIMA);
	apagarLed(LED_NIVEL_ELEVADOR_ABAIXO);
	apagarLed(LED_NIVEL_ELEVADOR_CORRETO);
}

static void acenderLedRGB(int led, int cor){
	//TODO FAzer o led RGB trocar de cor
}

static void acaoElevadorEmMovimento(int op){

	apagarLedsElevador();
	if(op == 0){//Elevador Parado
		if(porta == PORTA_ABERTA) {
			acenderLedRGB(LED_ELEVADOR_MOVIMENTO_2, LED_COR_PORTA_ABERTA);
		} else {//Porta Fechada
			acenderLedRGB(LED_ELEVADOR_MOVIMENTO_2, LED_COR_PORTA_FECHADA);
		}
	} else if(op == 1){//Elevador Subindo
		acenderLed(LED_ELEVADOR_MOVIMENTO_3);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_3);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_2);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_2);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_1);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_1);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

	} else {//Elevador Descendo
		acenderLed(LED_ELEVADOR_MOVIMENTO_1);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_1);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_2);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_2);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_3);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_3);
		//sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
	}
}

static void acoes(){
    printf("\n\n\n===Estado dos Sensores===");
	if(andarAtual > andarDestino) {
		printf("\nO Elevador Descendo");
		acaoElevadorEmMovimento(-1);
	} else if(andarDestino > andarAtual) {
		printf("\nO Elevador Subindo");
		acaoElevadorEmMovimento(1);
	} else {
		printf("\nO Elevador Parado");
		acaoElevadorEmMovimento(0);
	}

	if(nivelElevador == NIVEL_ELEVADOR_ABAIXO){
		printf("\nNivel do Elevador: Abaixo do Esperado");
		acenderLed(LED_NIVEL_ELEVADOR_ABAIXO);
	} else if(nivelElevador == NIVEL_ELEVADOR_ACIMA){
		printf("\nNivel do Elevador: Acima do Esperado");
		acenderLed(LED_NIVEL_ELEVADOR_ACIMA);
	} else {
		printf("\nNivel do Elevador: Correto");
		acenderLed(LED_NIVEL_ELEVADOR_CORRETO);
	}
}


PROCESS_THREAD(projetoUDP_process, ev, data) {

	//Configuracao Inicial de ROUTER
	#if UIP_CONF_ROUTER
		uip_ipaddr_t ipaddr;
	#endif /* UIP_CONF_ROUTER */


	//Comeco do PROCESSO
	PROCESS_BEGIN();

		#if UIP_CONF_ROUTER
			uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
			uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
			uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
		#endif

		//Print IP
		print_local_addresses();

		//Configurando um sensor do tipo ADC
		sensor = sensors_find(ADC_SENSOR);
		//Definindo um PINO de ENTRADA
		sensor->configure(ADC_SENSOR_SET_CHANNEL, ADC_COMPB_IN_AUXIO7);

		//Definino pinos de Saidas
		IOCPinTypeGpioOutput(LED_ELEVADOR_MOVIMENTO_1);
		IOCPinTypeGpioOutput(LED_ELEVADOR_MOVIMENTO_2);
		IOCPinTypeGpioOutput(LED_ELEVADOR_MOVIMENTO_3);
		IOCPinTypeGpioOutput(LED_NIVEL_ELEVADOR_ACIMA);
		IOCPinTypeGpioOutput(LED_NIVEL_ELEVADOR_ABAIXO);
		IOCPinTypeGpioOutput(LED_NIVEL_ELEVADOR_CORRETO);

		//Definir porta a ser escutada
		server_conn = udp_new(NULL, UIP_HTONS(PORT_MSG), NULL);
		udp_bind(server_conn, UIP_HTONS(PORT_MSG));


		//Apagar todos os LEDS
		apagarLedsElevador();

		while (1) {
			//Liberando o processador
			PROCESS_YIELD();

			//Aguardando um Evento do tipo TCP/IP
			if (ev == tcpip_event) {
				eventoTcpIP();
				//Executa acoes na placa para exibir o status do elevador
				acoes();
			}

		}
	//Fim do PROCESSO
	PROCESS_END();
}
