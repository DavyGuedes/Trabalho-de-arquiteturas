#include <iostream>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "cores.h"

#define TAMANHO_RAM 100000000
#define TRUE 1
#define FALSE 0

using namespace std;

typedef unsigned int palavra; 				//32 bits
typedef unsigned char byte;					//8  bits
typedef unsigned long int microinstrucao;	//64 bits

//REGISTRADORES//
palavra MAR = 0, MDR = 0, PC = 0; 	//ACESSO MEMÓRIA
byte MBR = 0; 						//ACESSO MEMÓRIA

palavra SP = 0, LV = 0, CPP = 0,	//OPERAÇÃO NA ULA
		TOS = 0, OPC = 0, H = 0;	//OPERAÇÃO NA ULA

microinstrucao MIR;					//CONTÉM A MICROINSTRUÇÃO ATUAL

//BARRAMENTOS
palavra bB, bC;

//FLIP-FLOP PARA O DESLOCADOR
byte Z, N; 

//AUXILIARES PARA DECODIFICAR A MICROINSTRUÇÃO
byte mi_barramentoB, mi_operacao, mi_pulo, mi_memoria, mi_deslocador; 
palavra mi_gravar, MPC = 0; 

//FIRMWARE QUE ARMAZENA O MICROPROGRAMA DE CONTROLE
microinstrucao armazenamento[512];


//MEMORIA PRINCIPAL (RAM) DO EMULADOR
byte memoria[TAMANHO_RAM];

//FUNÇÕES QUE CARREGAM O FIRMWARE E A MEMÓRIA RAM
void carrega_microprograma();				//Lê o arquivo microprog.rom e carrega o microprograma para o armazenamento
void carrega_programa(const char *arquivo); //Lê o arquivo passado como argumento e carrega o programa na memória

//FUNÇÕES QUE EXECUTAM A MICROINSTRUÇÃO
void decodificar_microinstrucao();//Separa a microinstrução e atribui as variáveis de apoio
void atribuir_barramentoB();	  //Envia para o barramento B o registrador solicitado na microinstrução
void ULA();						  //Realiza as operações da ULA
void atribuir_registradores();	  //Envaminha o resultado da ULA para os registradores
void operar_memoria();		  	  //Realiza as operações FEATCH, READ E WRITE na memória
void pular();					  //Realiza os pulos se zero, não zero e MPC caso seja solicitado

//FUNÇÕES QUE EXIBEM AS INFORMAÇÕES DO EMULADOR
void exibe_processo();				 //Exibe as informações que são atualizadas no Emulador
void binario(void *valor, int tipo); //Mostra os valores em binário

//Função principal
int main(int argc, const char *argv[]){
	carrega_microprograma();
	carrega_programa(argv[1]);

	while(true){
		exibe_processo();
		//Atualiza o Registrador que armazena a microisntrução atual
		MIR = armazenamento[MPC];

		//--CONJUNTO DE OPERAÇÕES--//
		decodificar_microinstrucao();//Separa a microinstrução e atribui as variáveis de apoio
		atribuir_barramentoB();		 //Envia para o barramento B o registrador solicitado na microinstrução
		ULA();						 //Realiza as operações da ULA
		atribuir_registradores();	 //Envaminha o resultado da ULA para os registradores
		operar_memoria();			 //Realiza as operações FEATCH, READ E WRITE na memória
		pular();					 //Realiza os pulos se zero, não zero e MPC caso seja solicitado
		
	}
	return 0;
}

//Envia para o armazenamento de controle o microprograma de controle contido no arquivo microprog.rom
void carrega_microprograma(){
	FILE *microprograma;
	microprograma = fopen("microprog.rom", "rb");

	if (microprograma != NULL) {
		fread(armazenamento, sizeof(microinstrucao), 512, microprograma);
		fclose(microprograma);

	}
}

//Envia para a memória ram o programa montado em um arquivo binário
void carrega_programa(const char *arquivo){
	FILE *prog;
	palavra tamanho;
	byte tam_arquivo[4];

	prog = fopen(arquivo, "rb");
	
	if (prog != NULL) {
		//Carrega os primeiros 4 bytes que contém o tamanho do arquivo para um vetor e depois carrega esse vetor na variável tamanho.
		fread(tam_arquivo, sizeof(byte), 4, prog);
		memcpy(&tamanho, tam_arquivo, 4);

		//Carrega os 20 primeiros bytes que contém a inicialização do programa para os primeiros 20 bytes da memória
		fread(memoria, sizeof(byte), 20, prog);

		//Carrega o programa na memória a partir da posição PC
		fread(&memoria[0x0401], sizeof(byte), tamanho-20, prog);
	
		fclose(prog);

	}
}

//Onde será feita a separação da microinstrução e as mi_operacaoções
void decodificar_microinstrucao(){

	mi_barramentoB  =  MIR 		  & 0b1111;		//Qual dos registradores será usado no barramento B
	mi_memoria 		= (MIR >> 4)  & 0b111;		//Qual operação será feita com a memoria principal
	mi_gravar 		= (MIR >> 7)  & 0b111111111;//Qual dos registradores será gravado o barramento C
	mi_operacao 	= (MIR >> 16) & 0b111111;	//Qual a operacaoção que será feita na ULA
	mi_deslocador 	= (MIR >> 22) & 0b11;		//Qual será a operação feita pelo deslocador
	mi_pulo			= (MIR >> 24) & 0b111;		//Se haverá pulo ou não
	MPC 			= (MIR >> 27) & 0b111111111;//Qual será a próxima instruçãoss
		
}

//Faz a atribuição do barramento B
void atribuir_barramentoB(){
	//Carrega um registrador para o barramento B
	switch(mi_barramentoB){
		case 0: bB = MDR;				break;
		case 1: bB = PC;				break;
		//O caso 2 carrega o MBR com sinal fazendo a extensão de sinal, ou seja, copia-se o bit mais significativo do MBR para as 24 posições mais significativas do barramento B.
		case 2: bB = MBR;
			if(MBR & (0b10000000))
					bB = bB | (0b111111111111111111111111 << 8);
										break;
		case 3: bB = MBR;				break;
		case 4: bB = SP;				break;
		case 5: bB = LV;				break;
		case 6: bB = CPP;				break;
		case 7: bB = TOS;				break;
		case 8: bB = OPC;				break;
		default: bB = -1;				break;
	}
	
}

//Faz a mi_operacaoção da ULA
void ULA(){
	switch(mi_operacao){
		//Cada operação da ULA é representado pela sequencia dos bits de operação. Cada operação útil foi convertida para inteiro para facilitar a escrita do switch
		case 12: bC = H & bB;		break;
		case 17: bC = 1;			break;
		case 18: bC = -1;			break;
		case 20: bC = bB;			break;
		case 24: bC = H;			break;
		case 26: bC = ~H;			break;
		case 28: bC = H | bB;		break;
		case 44: bC = ~bB;			break;
		case 53: bC = bB + 1;		break;
		case 54: bC = bB - 1;		break;
		case 57: bC = H + 1;		break;
		case 59: bC = -H;			break;
		case 60: bC = H + bB;		break;
		case 61: bC = H + bB + 1;	break;
		case 63: bC = bB - H;		break;

		default: break;
	}
	
	//Verifica o resultado da ula e atribui as variáveis zero e nzero
	
	if(bC) { //Se bC for diferente de zero
		Z = FALSE;
		N = TRUE;
	} else { //Se bC for igual a zero
		Z = TRUE;
		N = FALSE;
	}
	
	//Faz o deslocamento do mi_deslocador
	switch(mi_deslocador){
		//Faz o deslocamento em um bit para direita
		case 1: bC = bC >> 1;		break;
		//Faz o deslocamento em 8 bits para a esquerda
		case 2: bC = bC << 8;		break;
	}
}

//Grava o resultado através do barramento C
void atribuir_registradores(){
	//Pode atribuir vários registradores ao mesmo tempo dependendo se mi_gravar possui bit alto para o registrador correspondente
	if(mi_gravar & 0b000000001)   MAR = bC;
	if(mi_gravar & 0b000000010)   MDR = bC;
	if(mi_gravar & 0b000000100)   PC  = bC;
	if(mi_gravar & 0b000001000)   SP  = bC;
	if(mi_gravar & 0b000010000)   LV  = bC;
	if(mi_gravar & 0b000100000)   CPP = bC;
	if(mi_gravar & 0b001000000)   TOS = bC;
	if(mi_gravar & 0b010000000)   OPC = bC;
	if(mi_gravar & 0b100000000)   H   = bC;
}


//Operações Fetch, Read, Write da memória
void operar_memoria(){
	if(mi_memoria & 0b001) MBR = memoria[PC];					//FEATCH
	//MDR recebe os 4 bytes referente a palavra MAR 
	if(mi_memoria & 0b010) memcpy(&MDR, &memoria[MAR*4], 4);	//READ
	//Os 4 bytes na memória da palavra MAR recebem o valor de MDR
	if(mi_memoria & 0b100) memcpy(&memoria[MAR*4], &MDR, 4);	//WRITE

}


//Faz a operação do pulo
void pular(){
	//Realiza o pulo se a saída da ULA for zero
	if(mi_pulo & 0b001) MPC = MPC | (Z << 8);
	//Realiza o pulo se a saída da ula for diferente de zero
	if(mi_pulo & 0b010) MPC = MPC | (N << 8);
	//Pula para a posição do MBR
	if(mi_pulo & 0b100) MPC = MPC | MBR;

}


//ÁREA RESPONSÁVEL POR PRINTAR AS INFORMAÇÕES DA ULA//
void exibe_processo(){
	system("clear");
	cout << COR_AMARELO;
	cout << "\n ██ █ █ █ █ █ █ █ ██  EMULADOR IJVM  ██ █ █ █ █ █ █ █ ██";
	cout << "\n";	
	

	//Exibe a pilha de operandos quando o emulador já realizou a inicialização
	if (LV && SP) { cout << COR_VERDE;
		cout << "\n                  ╭────────────────────╮";
		cout << "\n   ───────────────┤ PILHA DE OPERANDOS ├─────────────";
		cout << "\n                  ╰────────────────────╯";

		cout << COR_CIANO;
		cout << "\n\t\t\t\t       ENDEREÇO";
		cout << "\n\t\t BINÁRIO\t\t  DE      INT";
		cout << "\n\t\t        \t\tPALAVRA\n";

		//Exibe a área delimitada por SP e LV para mostrar a pilha de operandos
		cout << COR_BRANCO;
		for (int i = SP; i >= LV; i--) {
			palavra valor;
			memcpy(&valor, &memoria[i*4], 4);

			binario(&valor , 1); cout << "\t "<< i; cout << "\t  " << (int)valor; cout << "\n";
		}
		cout << COR_VERDE;
		cout << "   ───────────────────────────────────────────────────";

	}

	//Exibe a área do programa quando o Emulador já realizou a inicialização
	if (PC >= 0x0401) {
		cout << COR_AZUL;
		cout << "\n                  ╭──────────────────╮";
		cout << "\n   ───────────────┤ ÁREA DO PROGRAMA ├───────────────";
		cout << "\n                  ╰──────────────────╯";
		cout << COR_CIANO;
		cout << "\n\t\t                       ENDEREÇO";
		cout << "\n\t\t BINÁRIO        HEXA      DE      INT";
		cout << "\n\t\t                         BYTE\n";
		cout << COR_BRANCO;

		//Exibe a área ao redor de PC para mostrar trechos do programa que o Emulador está executando no momento
		for (int i = PC-2; i <= PC+3; i++) {
			cout << COR_VERMELHO;
			if (i == PC) cout << "  Em execução ►";
			else cout << "\t       ";
			cout << COR_BRANCO;
			binario(&memoria[i], 2);
			printf("\t0x%02X", memoria[i]); 
			cout << "\t "<< i; 
			cout << "\t  " << (int)memoria[i];
			cout << "\n";
		}
		cout << COR_AZUL;
		cout << "   ───────────────────────────────────────────────────";
	}

	//Exibe os registradores
	cout << COR_MAGENTA;
	cout << "\n                  ╔═══════════════════╗";
	cout << "\n   ═══════════════╣   REGISTRADORES   ╠════════════════";
	cout << "\n                  ╚═══════════════════╝";

	cout << COR_CIANO;;
	cout << "\n\n\t\t\t  BINÁRIO\t           INT\n";
	cout << COR_BRANCO;
	cout << "\n    MAR :  ";		  binario(&MAR , 3); cout << "      " << MAR;
	cout << "\n    MDR :  ";   	 	  binario(&MDR , 3); cout << "      " << MDR;
	cout << "\n    PC  :  "; 	 	  binario(&PC  , 3); cout << "      " << PC;
	cout << "\n    MBR :\t\t\t   ";   binario(&MBR , 2); cout << "      " << (palavra)MBR;
 	cout << "\n    SP  :  ";		  binario(&SP  , 3); cout << "      " << SP;
	cout << "\n    LV  :  ";		  binario(&LV  , 3); cout << "      " << LV;
	cout << "\n    CPP :  ";		  binario(&CPP , 3); cout << "      " << CPP;
	cout << "\n    TOS :  ";		  binario(&TOS , 3); cout << "      " << TOS;
	cout << "\n    OPC :  ";	  	  binario(&OPC , 3); cout << "      " << OPC;
	cout << "\n    H   :  ";		  binario(&H   , 3); cout << "      " << H;
	cout << COR_CIANO;
	cout << "\n\n            ENDEREÇO DA PRÓXIMA MICROINSTRUÇÃO";
	cout << COR_BRANCO;
	cout << "\n    MPC :\t\t\t  ";	  binario(&MPC , 5); cout << "      "<< MPC;
	cout <<COR_MAGENTA;
	cout << "\n   ══════════════════════════════════════════════════";
	cout << COR_NORMAL;

	//Exibe a microinstrução que a ula está operando atualmente
	cout << COR_VERMELHO;
	cout << "\n              ◄♦♦♦  ";cout << COR_BRANCO; cout << "MICROINSTRUÇÃO ATUAL"; cout << COR_VERMELHO; cout <<" ♦♦♦►";  
	cout << COR_CIANO;
	cout << "\n        Addr    JAM    ULA         C      Mem   B";
	cout << COR_BRANCO;
	cout << "\n   "; binario(&MIR, 4);
	cout << COR_AMARELO;
	cout << "\n\n ██ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ █ ██\n";
	cout << COR_NORMAL;

	getchar();
}


//FUNÇÃO PARA PRINTAR OS VALORES EM BINÁRIO
//tipo 1: Imprime o binário de 4 bytes seguidos
//tipo 2: Imprime o binário de apenas um byte
//tipo 3: Imprime o binário de uma palavra
//tipo 4: Imprime o binário de uma microinstrução
//tipo 5: Imprime os 9 bits do MPC

void binario(void *valor, int tipo){
	printf("  ");
	switch (tipo) {
		case 1: {
			printf(" ");
			byte aux;
			byte* valorAux = (byte*)valor;
				
			for(int i = 3; i >= 0; i--){
				aux = *(valorAux + i);
				for(int j = 0; j < 8; j++){
					printf("%d", (aux >> 7) & 0b1);
					aux = aux << 1;
				}
				printf(" ");
			}
		}
		break;

		case 2: {
			byte aux;
			
			aux = *((byte*)(valor));
			for(int j = 0; j < 8; j++){
				printf("%d", (aux >> 7) & 0b1);
				aux = aux << 1;
			}
		}
		break;
		
		case 3: {
			palavra aux;
			
			aux = *((palavra*)(valor));
			for(int j = 0; j < 32; j++){
				printf("%d", (aux >> 31) & 0b1);
				aux = aux << 1;
			}
		}
		break;
		
		case 4: {
			microinstrucao aux;
		
			aux = *((microinstrucao*)(valor));
			for(int j = 0; j < 36; j++){
				if ( j == 9 || j == 12 || j == 20 || j == 29 || j == 32) cout << "  ";

				printf("%ld", (aux >> 35) & 0b1);
				aux = aux << 1;
			}
		}
		break;

		case 5: {
			palavra aux;
		
			aux = *((palavra*)(valor)) << 23;
			for(int j = 0; j < 9; j++){
				printf("%d", (aux >> 31) & 0b1);
				aux = aux << 1;
			}
		}
		break;

	}
}
