#include <stdio.h>
#include <stdlib.h>


#define LIMPAR_TELA "cls"
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

static int op = 1;
static int porta = PORTA_ABERTA;
static int nivelElevador = NIVEL_ELEVADOR_CORRETO;
static int elevador = ELEVADOR_PARADO;
static int andarAtual = 0;
static int andarDestino = 0;

void lerSensores(){
    int validacao = 0;

    do{
        system(LIMPAR_TELA);
        printf("===Estado da Porta===");
        printf("\n1 - Aberta");
        printf("\n2 - Fechada");
        printf("\nPorta: ");
        scanf("%d", &op);
        porta = op == 1 ? PORTA_ABERTA : PORTA_FECHADA;
        if(elevador == ELEVADOR_EM_MOVIMENTO && porta == PORTA_ABERTA){
            printf("\nA Porta nao pode abrir, pois o elevador esta em movimento\n\n");
            system("pause");
            validacao = 1;
        } else {
            validacao = 0;
        }
    } while(validacao == 1);

    do{
        system(LIMPAR_TELA);
        printf("===Estado do Elevador===");
        printf("\n1 - Em movimento");
        printf("\n2 - Parado");
        printf("\nElevador: ");
        scanf("%d", &op);
        elevador = op == 1 ? ELEVADOR_EM_MOVIMENTO : ELEVADOR_PARADO;
        if(elevador == ELEVADOR_EM_MOVIMENTO && porta == PORTA_ABERTA){
            printf("\nO Elevador nao pode ir para Em Movimento, pois a Porta esta Aberta\n\n");
            system("pause");
            validacao = 1;
        } else {
            validacao = 0;
        }
    } while(validacao == 1);

    if(elevador == ELEVADOR_PARADO){
        do{
            system(LIMPAR_TELA);
            printf("===Este predio tem os andares de %d ate %d ===",  ANDAR_MINIMO, ANDAR_MAXIMO);
            printf("\nQual eh o andar de destino?");
            printf("\nAndar: ");
            scanf("%d", &op);
            if(op < ANDAR_MINIMO || op > ANDAR_MAXIMO){
                printf("\nO andar de destino eh invalido. Este predio tem os andares de %d ate %d\n\n", ANDAR_MINIMO, ANDAR_MAXIMO);
                system("pause");
                validacao = 1;
            } else {
                validacao = 0;
                andarAtual = andarDestino;
                andarDestino = op;
            }
        } while(validacao == 1);

        system(LIMPAR_TELA);
        printf("===Estado do Nivel do Elevador===");
        printf("\n1 - Correto");
        printf("\n2 - Nivel Acima do Esperado");
        printf("\n3 - Nivel Abaixo do Esperado");
        printf("\nNivel do Elevador: ");
        scanf("%d", &op);
        if(op == 1){
            nivelElevador = NIVEL_ELEVADOR_CORRETO;
            andares[andarDestino +(ANDAR_MINIMO * -1)] = andares[andarDestino +(ANDAR_MINIMO * -1)] > 0 ? andares[andarDestino +(ANDAR_MINIMO * -1)] -1 : 0;
        } else if(op == 2) {
            nivelElevador = NIVEL_ELEVADOR_ACIMA;
            andares[andarDestino +(ANDAR_MINIMO * -1)]++;
        } else {
            nivelElevador = NIVEL_ELEVADOR_ABAIXO;
            andares[andarDestino +(ANDAR_MINIMO * -1)]++;
        }
    }
}

void imprimir(){
    system(LIMPAR_TELA);
    printf("===Estado dos Sensores===");
    printf("\nPorta: %s", porta == PORTA_ABERTA ? "Porta Aberta" : "Porta Fechada");
    printf("\nElevador: %s", elevador == ELEVADOR_EM_MOVIMENTO? "Elevador Em Movimento" : "Elevador Parado");

    printf("\nAndar de Atual: %d", andarAtual);
    printf("\nAndar de Destino: %d", andarDestino);
    if(andarAtual > andarDestino) {
        printf("\nO Elevador Descendo");
    } else if(andarDestino > andarAtual) {
        printf("\nO Elevador Subindo");
    } else {
        printf("\nO Elevador Parado");
    }

    if(nivelElevador == NIVEL_ELEVADOR_ABAIXO){
        printf("\nNivel do Elevador: Abaixo do Esperado");
    } else if(nivelElevador == NIVEL_ELEVADOR_ACIMA){
        printf("\nNivel do Elevador: Acima do Esperado");
    } else {
        printf("\nNivel do Elevador: Correto");
    }

    printf("\n\n i = %d, v = %d",andarDestino +(ANDAR_MINIMO * -1), andares[andarDestino +(ANDAR_MINIMO * -1)]);

    //Atingiu o numero maximo de erros permitidos
    if(andares[andarDestino +(ANDAR_MINIMO * -1)] > QTD_MAX_ERROS_PERMITIDO){
        printf("\n\nOBS: Chamar a equipe de manutencao, pois o elevador excedeu o numero maximo de erros permitidos no andar: %d", andarDestino);
    }
    printf("\n\n");
    system("pause");
}

int main()
{
    //Para fazer analises dos andares
    andares = malloc (qtdMaxAndar * sizeof (int));
    int i;
    for(i = 0; i< qtdMaxAndar; i++){
        andares[i] = 0;
    }
    //Loop
    while(op != -1)
    {
        lerSensores();
        imprimir();


    }
    printf("\n\n\nFIM do Programa\n\n\n");

    system("pause");
    return 0;
}
