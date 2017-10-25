#include <stdio.h>
#include <stdint.h>
#include <math.h>
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
#include "net/ip/uip-debug.h"
#include "random.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "ti-lib.h"



//Definicoes de Constantes
#define CONN_PORT 8802//Porta para MSG UDP
#define DEBUG DEBUG_PRINT //Para imprimir o IPV6
#define DEBUG true
#define TAMANHO_BUFFER 1024

#define PORTA_ABERTA 0x6A
#define PORTA_FECHADA 0x6B

#define NIVEL_ELEVADOR_ABAIXO 0x6C
#define NIVEL_ELEVADOR_ACIMA 0x6D
#define NIVEL_ELEVADOR_CORRETO 0x6E

#define ELEVADOR_EM_MOVIMENTO 0x5A
#define ELEVADOR_PARADO 0x5B

#define ANDAR_MINIMO -2
#define ANDAR_MAXIMO 3

#define QTD_MAX_ERROS_PERMITIDO 2

#define LED_ELEVADOR_MOVIMENTO_1 IOID_10
#define LED_ELEVADOR_MOVIMENTO_2 IOID_11
#define LED_ELEVADOR_MOVIMENTO_3 IOID_12


#define LED_NIVEL_ELEVADOR_ACIMA IOID_13
#define LED_NIVEL_ELEVADOR_ABAIXO IOID_14
#define LED_NIVEL_ELEVADOR_CORRETO IOID_15

#define LED_COR_PORTA_ABERTA 31
#define LED_COR_PORTA_FECHADA 32


//Estruturas para o projeto
static struct sensors_sensor *sensor;
static struct etimer et;
static struct uip_udp_conn *server_conn;

//Variaveis Globais
static int qtdMaxAndar = ((ANDAR_MAXIMO >= 0 ? ANDAR_MAXIMO : ANDAR_MAXIMO * -1) + (ANDAR_MINIMO >= 0 ? ANDAR_MINIMO : ANDAR_MINIMO * -1)) +1;
static int porta = PORTA_ABERTA;
static int nivelElevador = NIVEL_ELEVADOR_CORRETO;
static int elevador = ELEVADOR_PARADO;
static int andarAtual = 0;
static int andarDestino = 0;
static int *andares;
static char log[TAMANHO_BUFFER];

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

//Metodo para tratar um evento do tipo TCP/IP
static void eventoTcpIP(void) {
	printf("\n\nEvento TCP/IP - Recebido");

	//Convertendo a MSG de Bytes para uma String
	char* msg = (char*) uip_appdata;
	int i;

	//Validando se existe um novo conteudo
	if (uip_newdata()) {
		((char *) uip_appdata)[uip_datalen()] = 0;

		switch (msg[0]) {
			case PORTA_ABERTA: {
				printf("\nPorta Aberta");
				porta = PORTA_ABERTA;

				if(elevador == ELEVADOR_EM_MOVIMENTO && porta == PORTA_ABERTA){
					printf("\nA Porta nao pode abrir, pois o elevador esta em movimento\n\n");
					porta = PORTA_FECHADA;//Volta o status
				}
				break;
			}
			case PORTA_FECHADA: {
				printf("\nPorta Fechada");
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
				andares[andarDestino +(ANDAR_MINIMO * -1)]++;
				break;
			}
			case NIVEL_ELEVADOR_ACIMA: {
				printf("\nNivel do Elevador - Acima do Esperado");
				nivelElevador = NIVEL_ELEVADOR_ACIMA;
				andares[andarDestino +(ANDAR_MINIMO * -1)]++;
				break;
			}
			case NIVEL_ELEVADOR_CORRETO: {
				printf("\nNivel do Elevador - Correto");
				nivelElevador = NIVEL_ELEVADOR_CORRETO;
				andares[andarDestino +(ANDAR_MINIMO * -1)] = andares[andarDestino +(ANDAR_MINIMO * -1)] > 0 ? andares[andarDestino +(ANDAR_MINIMO * -1)] -1 : 0;
				break;
			}
			default: {
				printf("Comando Invalido: ");
				for (i = 0; i < uip_datalen(); i++) {
					printf("0x%02X ", msg[i]);
				}
				printf("\n");
				break;
			}
		}
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
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_3);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_2);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_2);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_1);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_1);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

	} else {//Elevador Descendo
		acenderLed(LED_ELEVADOR_MOVIMENTO_1);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_1);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_2);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_2);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar

		acenderLed(LED_ELEVADOR_MOVIMENTO_3);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
		apagarLed(LED_ELEVADOR_MOVIMENTO_3);
		sleep_now(0.5);//TODO validar se aqui faz o sleep para dar o efeito de piscar
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

static void imprimir(){
	strcpy(log, "");
	strcat(log, "\n\n\n===Estado dos Sensores===");
    strcat(log, "\nPorta: %s", porta == PORTA_ABERTA ? "Porta Aberta" : "Porta Fechada");
    strcat(log, "\nElevador: %s", elevador == ELEVADOR_EM_MOVIMENTO? "Elevador Em Movimento" : "Elevador Parado");

    strcat(log, "\nAndar de Atual: %d", andarAtual);
    strcat(log, "\nAndar de Destino: %d", andarDestino);
    if(andarAtual > andarDestino) {
        strcat(log, "\nO Elevador Descendo");
    } else if(andarDestino > andarAtual) {
        strcat(log, "\nO Elevador Subindo");
    } else {
        strcat(log, "\nO Elevador Parado");
    }

    if(nivelElevador == NIVEL_ELEVADOR_ABAIXO){
        strcat(log, "\nNivel do Elevador: Abaixo do Esperado");
    } else if(nivelElevador == NIVEL_ELEVADOR_ACIMA){
        strcat(log, "\nNivel do Elevador: Acima do Esperado");
    } else {
        strcat(log, "\nNivel do Elevador: Correto");
    }

    strcat(log, "\n\n i = %d, v = %d",andarDestino +(ANDAR_MINIMO * -1), andares[andarDestino +(ANDAR_MINIMO * -1)]);

    //Atingiu o numero maximo de erros permitidos
    if(andares[andarDestino +(ANDAR_MINIMO * -1)] > QTD_MAX_ERROS_PERMITIDO){
        strcat(log, "\n\nOBS: Chamar a equipe de manutencao, pois o elevador excedeu o numero maximo de erros permitidos no andar: %d", andarDestino);
    }
    strcat(log, "\n\n");

    printf(log);
}

PROCESS_THREAD(projetoUDP_process, ev, data) {

	//Configuracao Inicial de ROUTER
	#if UIP_CONF_ROUTER
		uip_ipaddr_t ipaddr;
		rpl_dag_t *dag;
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

		//Criando um evento de tempo a cada 1s
		etimer_set(&et, CLOCK_SECOND * 1);

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
	    IOCPinTypeGpioOutput(IOID_8);
	    IOCPinTypeGpioOutput(IOID_7);

		//Definir porta a ser escutada
		server_conn = udp_new(NULL, UIP_HTONS(CONN_PORT), NULL);
		udp_bind(server_conn, UIP_HTONS(CONN_PORT));

		//Inicializando o controle dos andares
		//Para fazer analises dos andares
		andares = malloc (qtdMaxAndar * sizeof (int));
		int i;
		for(i = 0; i< qtdMaxAndar; i++){
			andares[i] = 0;
		}

		while (1) {
			//Liberando o processador
			PROCESS_YIELD();

			//Aguardando um Evento do tipo TCP/IP
			if (ev == tcpip_event) {
				eventoTcpIP();
				//Apos cada evento, vamos imprimir o status atual
				imprimir();
				//Executa acoes na placa para exibir o status do elevador
				acoes();
			}

		}
	//Fim do PROCESSO
	PROCESS_END();
}
