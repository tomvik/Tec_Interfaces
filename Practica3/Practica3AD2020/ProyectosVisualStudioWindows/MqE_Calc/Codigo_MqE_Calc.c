#include <conio.h>
#include <stdio.h>
#include <stdlib.h>

#define TRANS_COUNT 8
#define EDO_COUNT 9

char chr = 0;
int acum1 = 0;
int acum2 = 0;
int res = 0;
enum Oper { Suma, Resta, Mult, Div };
enum Oper oper;

int edo = 0;
int edoAnt = 0;
int trans = 0;

// 0-->Inv�lida
// 6-->Digito
// 7-->Operador
int chrTrans[TRANS_COUNT] = {0, '(', ')', '=', 8, 27, 6, 7};
int mtzTrans[EDO_COUNT][TRANS_COUNT] = {
    {0, 1, 0, 0, 0, 0, 0, 0},   
    {1, 1, 1, 1, 98, 99, 2, 1}, 
    {2, 2, 2, 2, 98, 99, 3, 4},
    {3, 2, 2, 2, 98, 99, 2, 2},  // No importa este
    {4, 4, 4, 4, 98, 99, 5, 4}, 
    {5, 5, 7, 5, 98, 99, 6, 5},
    {6, 5, 5, 5, 98, 99, 5, 5},  // No importa este 
    {7, 7, 7, 8, 98, 99, 7, 7}, 
    {8, 0, 0, 0, 0, 0, 0, 0}  // No importa este 
};

int calcTrans(char chr) {
    int trans = 0;
    if ((chr >= '0') && (chr <= '9'))  // Digito
        return (6);
    switch (chr) {
        case '+':
        case '-':
        case '*':
        case '/':
            return (7);
    }
    for (trans = 5; trans > 0; trans--)
        if (chr == chrTrans[trans]) break;
    return (trans);
}

int sigEdo(int edo, int trans) { return (mtzTrans[edo][trans]); }

int ejecutaEdo(int edo) {
	printf("estado: %d\n", edo);
    switch (edo) {
        case 0:
            break;
        case 1:
            acum1 = 0;
            printf("%c", chr);
            break;
        case 2:
            printf("%c", chr);
            acum1 *= 10;
            acum1 += (chr - '0');
            break;
        case 3:
            printf("%c", chr);
            acum1 *= 10;
            acum1 += (chr - '0');
            return (2);
        case 4:
            printf("%c", chr);
            switch (chr) {
                case '+':
                    oper = Suma;
                    break;
                case '-':
                    oper = Resta;
                    break;
                case '*':
                    oper = Mult;
                    break;
                case '/':
                    oper = Div;
                    break;
            }
            acum2 = 0;  // Preparar la entrada al estado 4
            break;
        case 5:
            printf("%c", chr);
            acum2 *= 10;
            acum2 = (chr - '0');
            break;
        case 6:
            printf("%c", chr);
            acum2 *= 10;
            acum2 += (chr - '0');
            return (5);
        case 7:
            printf("%c", chr);
            break;
        case 8:
            printf("%c", chr);
            switch (oper) {
                case Suma:
                    res = acum1 + acum2;
                    break;
                case Resta:
                    res = acum1 - acum2;
                    break;
                case Mult:
                    res = acum1 * acum2;
                    break;
                case Div:
                    if (acum2)
                        res = acum1 / acum2;
                    else
                        res = -1;
                    break;
            }
            printf("%d\n", res);
            return (0);
        case 99:
            printf("\n<<<Captura cancelada>>>\n");
            return (0);  // Estado aceptor, rompe la rutina y marca estado de salida
    }
    return (edo);  // Para estados no aceptores regresar el estado ejecutado
}

int main() {
    while (chr != '.') {    // El caracter '.' termina la ejecuci�n del programa
        if (kbhit()) {      // Buscando entrada del teclado
            chr = getch();  // Tomando entrada del teclado
            if (chr != '.') {
                trans = calcTrans(chr);  // Calcular la transici�n seg�n la entrada del teclado
                if (trans) {  // Validar por transici�n valida (la transici�n 0 es inv�lida)
                    edoAnt = edo;                 // Guardar el estado anterior
                    edo = sigEdo(edoAnt, trans);  // Calcular el siguiente estado
                    if (edoAnt != edo)            // Solo si hay cambio de estado hay que ...
                        edo = ejecutaEdo(edo);    // ... ejecutar el nuevo estado y asignar 
												  // estado de continuidad
                }
            }
        }
    }
    printf("\nFin!!!\n");
	return 0;
}