#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

HANDLE *serial;
unsigned char TxDBuffer[10];
unsigned char RxDBuffer[10];
unsigned char chr;
int aux;
int acum=0;

DCB config;
COMMTIMEOUTS touts;

void main() {
	printf("Iniciando comunicación ...\n");
	serial=CreateFile("COM4",GENERIC_WRITE|GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (serial!=INVALID_HANDLE_VALUE) {
		printf("... vamos bien! ... \n");

		//Configurar protocolo y velocidad
		GetCommState(serial,&config);
		config.BaudRate=CBR_9600;
		config.fParity=0;
		config.fBinary=1;
		config.StopBits=ONESTOPBIT;
		config.ByteSize=8;
		SetCommState(serial,&config);

		//Configurar "timeouts"
		touts.ReadTotalTimeoutConstant=0;
		touts.ReadIntervalTimeout=0;
		touts.ReadTotalTimeoutMultiplier=0;
		SetCommTimeouts(serial,&touts);

		//Leer un dato
		aux=0;
		acum=0;
		chr=0;
		while (chr!='=') {
			ReadFile(serial,&chr,1,&aux,NULL);
			if ((chr>='0')&&(chr<='9')) {
				acum *= 10;
				acum += (chr-'0');
				WriteFile(serial,&chr,1,&aux,NULL);
			}
		}
		printf("Dato recibido: %d (%d)\n",acum,aux);

		acum++;
		sprintf(TxDBuffer,"%d",acum);

		//Escribir un dato
		WriteFile(serial,&TxDBuffer,strlen(TxDBuffer),&aux,NULL);

		
		CloseHandle(serial);
	} else {
		printf("Error: COM4 inaccesible\n");
	}
}