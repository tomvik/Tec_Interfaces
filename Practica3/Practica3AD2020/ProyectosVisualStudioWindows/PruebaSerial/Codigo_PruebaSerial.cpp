#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

char TxDBuffer[10];
char RxDBuffer[10];
unsigned char chr;
long unsigned int aux;
int acum = 0;

DCB config;
COMMTIMEOUTS touts;

int main() {
    printf("Iniciando comunicacion ...\n");
    auto serial = CreateFile("COM4", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (serial != INVALID_HANDLE_VALUE) {
        printf("... vamos bien! ... \n");

        // Configurar protocolo y velocidad
        GetCommState(serial, &config);
        config.BaudRate = CBR_9600;
        config.fParity = 0;
        config.fBinary = 1;
        config.StopBits = ONESTOPBIT;
        config.ByteSize = 8;
        SetCommState(serial, &config);

        // Configurar "timeouts"
        touts.ReadTotalTimeoutConstant = 0;
        touts.ReadIntervalTimeout = 0;
        touts.ReadTotalTimeoutMultiplier = 0;
        SetCommTimeouts(serial, &touts);

        // Leer un dato
        aux = 0;
        acum = 0;
        chr = 0;
        const char kExitKey = 'q';
        bool in_loop = true;
        while (in_loop) {
            ReadFile(serial, &chr, 1, &aux, NULL);
            printf("%c", chr);
            if (kbhit()) {
                scanf(" %c", &chr);
                WriteFile(serial, &chr, 1, &aux, NULL);
                in_loop = chr != kExitKey;
            }
        }
        /*
        while (chr != '=') {
            ReadFile(serial, &chr, 1, &aux, NULL);
            if ((chr >= '0') && (chr <= '9')) {
                acum *= 10;
                acum += (chr - '0');
                WriteFile(serial, &chr, 1, &aux, NULL);
            } else {
                printf("Waiting...\n");
            }
            Sleep(100);
        }
        printf("Dato recibido: %d (%d)\n", acum, aux);
        */

        acum++;
        snprintf(TxDBuffer, sizeof(TxDBuffer), "%d", acum);

        // Escribir un dato
        WriteFile(serial, &TxDBuffer, strlen(TxDBuffer), &aux, NULL);

        CloseHandle(serial);
    } else {
        printf("Error: COM4 inaccesible\n");
    }
    return 0;
}

int8_t myPrintSet(void) {
    int8_t extra_bytes = 0;
    ++acum;
    appData.cdcWriteBuffer[0] = '=';
    if(acum >= 100) {
        appData.cdcWriteBuffer[1] = (acum/100) + '0';
        acum %= 100;
        appData.cdcWriteBuffer[2] = (acum/10) + '0';
        appData.cdcWriteBuffer[3] = (acum%10) + '0';
        extra_bytes = 5;
    } else if(acum >= 10) {
        appData.cdcWriteBuffer[1] = (acum/10) + '0';
        appData.cdcWriteBuffer[2] = (acum%10) + '0';
        extra_bytes = 4;
    } else {
        appData.cdcWriteBuffer[1] = acum + '0';
        extra_bytes = 3;
    }
    appData.cdcWriteBuffer[extra_bytes-1] = ' ';
    acum = -1;
    return extra_bytes;
}