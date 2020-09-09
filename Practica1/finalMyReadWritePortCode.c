#include <stdbool.h>
#include <string.h>
#include "stdio.h"
#include "windows.h"

#define S_BUSY 0x80
#define S_ACK 0x40
#define S_PAPER_END 0x20
#define S_SELECT_IN 0x10
#define S_nERROR 0x08

// Mapeo de informaci[on ]
// 0 1 2 3 4 5 6 7 8 9 A b C d E F
const short Digitos[] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07,
                         0x7F, 0x67, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
const WORD kMaskControl = 0x0B;
const WORD kStatusMask = 0x80;

typedef void(__stdcall *lpOut32)(short, short);
typedef short(__stdcall *lpInp32)(short);
typedef BOOL(__stdcall *lpIsInpOutDriverOpen)(void);
typedef BOOL(__stdcall *lpIsXP64Bit)(void);

// Apuntadores a rutinas del DLL útiles para acceso al "Device Driver"
lpOut32 gfpOut32;
lpInp32 gfpInp32;
lpIsInpOutDriverOpen gfpIsInpOutDriverOpen;
lpIsXP64Bit gfpIsXP64Bit;

void WriteInDisplays(short puerto_base, short data, bool is_high) {
    // Variables auxiliares.
    WORD aux, casi_casi;

    // Variables para hacer la mascara dependiendo si es el display alto o bajo.
    // FE apagara el bit C0 y el FD apagara el bit C1.
    WORD and_ = (is_high ? 0XFD : 0XFE);
    WORD or_ = (is_high ? 0x02 : 0x01);

    // Leer lo que hay en el puerto de control y hacer XOR con la mascara.
    // Este XOR permite invertir los bits que tenian inversion por hardware para leerlos
    // correctamente.
    aux = gfpInp32(puerto_base + 2) ^ kMaskControl;

    // Apagamos el bit de control correspondiente.
    casi_casi = aux & and_;
    gfpOut32(puerto_base + 2, casi_casi ^ kMaskControl);
    Sleep(50);

    // Mandamos el dato.
    gfpOut32(puerto_base, data);
    Sleep(50);

    // Mandamos la transicion positiva.
    casi_casi = aux | or_;
    gfpOut32(puerto_base + 2, casi_casi ^ kMaskControl);
}

WORD ReadDipSwitch(short port, WORD deactivate_bit) {
    // Haz una mascara para desactivar el bit de control deseado.
    // En este caso, será el C3.
    const WORD deactivate_mask = 0xFF ^ deactivate_bit;

    // Un arreglo de mascaras para la lectura de cada bit individualmente.
    // Por ejemplo, el bit de Busy es equivalente al bit 0 y al bit 4 del dato.
    unsigned char status_bit_mask[] = {S_BUSY, S_PAPER_END, S_SELECT_IN, S_nERROR,
                                       S_BUSY, S_PAPER_END, S_SELECT_IN, S_nERROR};

    // Variables auxiliares.
    unsigned char status;
    unsigned int i;
    unsigned int aux = 0x01;
    unsigned int control;

    // Variable donde se guardará el dato.
    unsigned char dato = 0;

    // Se itera 8 veces por los 8 bits.
    for (i = 0; i < 8; i++, aux <<= 1) {
        if (i == 0) {
            // En el bit 0 nos aseguramos que el bit de control
            // deseado esté en 0, para leer el nibble más bajo.
            control = gfpInp32(port + 2) ^ kMaskControl;
            control = control & deactivate_mask;
            gfpOut32(port + 2, control ^ kMaskControl);
            Sleep(100);
        } else if (i == 4) {
            // En el bit 4 nos aseguramos que el bit de control
            // deseado esté en 1, para leer el nibble más alto.
            control = gfpInp32(port + 2) ^ kMaskControl;
            control = control | deactivate_bit;
            gfpOut32(port + 2, control ^ kMaskControl);
            Sleep(100);
        }

        status = gfpInp32(port + 1) ^ kStatusMask;

        // Si el bit que estamos leyendo en esta iteración es 1,
        // prende el bit de dato.
        if ((status & status_bit_mask[i]) != 0) {
            dato |= aux;
        }
    }

    // Regresa el dato en el formato:
    // b7 b6 b5 b4 b3 b2 b1 b0
    return (dato);
}

short Transform(char digito) {
    if (digito >= '0' && digito <= '9') {
        return digito - '0';
    } else if (digito >= 'A' && digito <= 'F') {
        return digito - 'A' + 10;
    } else if (digito >= 'a' && digito <= 'f') {
        return digito - 'a' + 10;
    }
    return -1;
}

void Write(short iPort, char *argv[]) {
    // Verificar que el numero hexadecimal dado este en el formato correcto.
    // Ejemplo 0A, A, 7, F7, 10, fa, af, etc.
    int size = strlen(argv[2]);
    if (size > 2) {
        printf("Error se debe proporcionar ya sea 1 o 2 digitos hexadecimales.\n");
        return;
    }

    // Transformar los dos digitos hexadecimales a numeros.
    short low_data = Transform((size > 1 ? argv[2][1] : argv[2][0]));
    short high_data = Transform((size > 1 ? argv[2][0] : '0'));

    // El formato es incorrecto.
    if (low_data == -1 || high_data == -1) {
        printf("Error se proporcionaron los dos digitos pero no estan en formato hexadecimal.\n");
        return;
    }
    // Transformar de indices a los bits correspondientes para poder imprimir en el display.
    low_data = Digitos[low_data];
    high_data = Digitos[high_data];

    // Imprimir los numeros en los dos displays.
    WriteInDisplays(iPort, low_data, false);
    WriteInDisplays(iPort, high_data, true);
}

void Read(short iPort) {
    // El bit c3 de control es el encargado de seleccionar el nibble a leer.
    const WORD control_c3 = 0x08;

    // Leemos el dato en byte.
    WORD data_in = ReadDipSwitch(iPort, control_c3);

    printf("El dato leido tiene un valor de: %d que en hexadecimal es: %X\n", data_in, data_in);

    // Transformar el byte a nibbles.
    short low_data = data_in & 0x0F;
    short high_data = (data_in >> 4) & 0x0F;

    // Transformar de indices a los bits correspondientes para poder imprimir en el display.
    low_data = Digitos[low_data];
    high_data = Digitos[high_data];

    // Imprimir los numeros en los dos displays.
    WriteInDisplays(iPort, low_data, false);
    WriteInDisplays(iPort, high_data, true);
}

void theBeep(unsigned int freq) {
    // Esta rutina hace acceso al puerto de I/O del "timer" conectado a la bocina
    gfpOut32(0x43, 0xB6);                   // Configuración del modo de operación del "timer"
    gfpOut32(0x42, (freq & 0xFF));          // Programación de la frecuencia (LOW Byte)
    gfpOut32(0x42, (freq >> 9));            // Programación de la frecuencia (HIGH Byte)
    Sleep(10);                              // Dejar que suene
    gfpOut32(0x61, gfpInp32(0x61) | 0x03);  // Activar la salida de la bocina (compuerta AND)
}

void StopTheBeep() {
    gfpOut32(0x61, (gfpInp32(0x61) & 0xFC));  // Desactivar la salida de la bocina (compuerta AND)
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        // Enviar un mensaje de error al usuario, cuando no ha introducido los parámetros correctos
        printf(
            "Error : faltan parametros\n\n***** Uso correcto *****\n\nmyReadWritePort read "
            "<ADDRESS> \no \nmyReadWritePort write <ADDRESS> <DATA>\n\n\n\n\n");
    } else {
        // Cargar el DLL a nuestro espacio de memoria
        HINSTANCE hInpOutDll;
        hInpOutDll = LoadLibrary(
            "D:\\Ricardo\\ITESM\\IEC\\PuertoParalelo\\Practica1\\Debug\\InpOutx64.DLL");  // Ruta y
                                                                                          // nombre
                                                                                          // del DLL
        // En la práctica #2 modificaremos el DLL para una funcionalidad adicional
        // hInpOutDll =
        // LoadLibrary("D:\\Ricardo\\ITESM\\IEC\\PuertoParalelo\\Practica2\\Debug\\DLL_Practica2.DLL");
        // //Ruta y nombre del DLL
        if (hInpOutDll != NULL) {
            gfpOut32 = (lpOut32)GetProcAddress(
                hInpOutDll,
                "Out32");  // Debería ser Out64 pero el DLL tiene el "system call" "hardcoded" a 32
            gfpInp32 = (lpInp32)GetProcAddress(
                hInpOutDll,
                "Inp32");  // Debería ser Inp64 pero el DLL tiene el "system call" "hardcoded" a 32
            gfpIsInpOutDriverOpen =
                (lpIsInpOutDriverOpen)GetProcAddress(hInpOutDll, "IsInpOutDriverOpen");

            if (gfpIsInpOutDriverOpen()) {
                // Indicar con sonidos que se ha logrado el acceso al DDL y al "device driver"
                theBeep(2000);
                Sleep(200);
                theBeep(1000);
                Sleep(300);
                theBeep(2000);
                Sleep(250);
                StopTheBeep();

                if (!strcmp(argv[1], "read")) {
                    short iPort = atoi(argv[2]);
                    WORD wData = gfpInp32(iPort);  // Leer el puerto
                    printf("Dato leido del puerto %s ==> %d \n\n\n\n", argv[2], wData);
                } else if (!strcmp(argv[1], "write")) {
                    if (argc < 4) {
                        printf("Error en los argumentos ingresados");
                        printf(
                            "\n***** Uso correcto *****\n\nmyReadWritePort read <ADDRESS> \no "
                            "\nmyReadWritePort write <ADDRESS> <DATA>\n\n\n\n\n");
                    } else {
                        short iPort = atoi(argv[2]);
                        WORD wData = atoi(argv[3]);
                        gfpOut32(iPort, wData);
                        printf("data written to %s\n\n\n", argv[2]);
                    }
                } else if (!strcmp(argv[1], "writeData") && argc == 3) {
                    short iPort = 0xD000;
                    Write(iPort, argv);
                } else if (!strcmp(argv[1], "readData") && argc == 2) {
                    short iPort = 0xD000;
                    Read(iPort);
                }
            } else {
                printf("Unable to open InpOutx64 Driver!\n");
            }

            // Fin del programa ...
            FreeLibrary(hInpOutDll);  //.. descargar DLL
            return 0;
        } else {
            printf("No puede encontrar el DLL InpOutx64!!! xP\n");
            return -1;
        }
    }
    return -2;
}
