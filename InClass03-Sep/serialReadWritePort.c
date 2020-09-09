// Integrantes:
// Tomas Alejandro Lugo Salinas: A00819460
// Jesus Francisco Anaya Gonzalez: A00823445

#include "stdio.h"
#include "windows.h"

typedef void(__stdcall *lpOut32)(short, short);
typedef short(__stdcall *lpInp32)(short);
typedef BOOL(__stdcall *lpIsInpOutDriverOpen)(void);
typedef BOOL(__stdcall *lpIsXP64Bit)(void);

// Apuntadores a rutinas del DLL útiles para acceso al "Device Driver"
lpOut32 gfpOut32;
lpInp32 gfpInp32;
lpIsInpOutDriverOpen gfpIsInpOutDriverOpen;
lpIsXP64Bit gfpIsXP64Bit;

// Desplegar la informacion en los dos displays
void sendData(unsigned int puerto_base, unsigned char data) {
    // Mapeo de informacion a digitos.
    // 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F.
    unsigned char digitos[] = {0x3F, 0X06, 0X5B, 0X4F, 0X66, 0X6D, 0X7D, 0X07,
                               0X7F, 0X6F, 0x77, 0x7F, 0x39, 0X3F, 0X71};

    // Variables auxiliares.
    unsigned char aux;
    unsigned char casi_casi;
    const unsigned char MASK_OUT = 0x0B;

    // Limpiar informacion.
    // Apagar C0 y C1 -> C0 controla el shift y C1 el load.
    aux = gfpInp32(puerto_base + 2) ^ MASK_OUT;
    aux = aux & 0xFC;  // 0xFC -> 1111 1100
    gfpOut32(puerto_base + 2, aux ^ MASK_OUT);

    // Transformar de 8 bits a dos numeros hexadecimales.
    unsigned char high_data = data / 16;
    unsigned char low_data = data % 16;

    // Recorrer los primeros 8 bits del dato menos significativo
    for (int i = 0, aux = digitos[low_data]; i < 8; i++) {
        // Escribir el digito menos significativo.
        gfpOut32(puerto_base, aux);
        Sleep(10);

        // Activar Flanco Positivo en C0.
        aux = gfpInp32(puerto_base + 2) ^ MASK_OUT;
        aux = aux | 0x01;
        gfpOut32(puerto_base + 2, aux ^ MASK_OUT);
        Sleep(10);

        // Llevar a cero C0 para la siguiente iteracion.
        aux = gfpInp32(puerto_base + 2) ^ MASK_OUT;
        aux = aux & 0xFC;  // 0xFE -> 1111 1110;
        gfpOut32(puerto_base + 2, aux ^ MASK_OUT);

        // Recorrer el dato a la derecha para mandar el siguiente bit.
        aux = aux >> 1;
    }

    // Recorrer los 8 bits del dato mas significativo.
    for (int i = 0, aux = digitos[high_data]; i < 8; i++) {
        // Escribir el digito menos significativo.
        gfpOut32(puerto_base, aux);
        Sleep(10);

        // Activar Flanco Positivo en C0.
        aux = gfpInp32(puerto_base + 2) ^ MASK_OUT;
        aux = aux | 0x01;
        gfpOut32(puerto_base + 2, aux ^ MASK_OUT);
        Sleep(10);

        // Llevar a cero C0 para la siguiente iteracion.
        aux = gfpInp32(puerto_base + 2) ^ MASK_OUT;
        aux = aux & 0xFC;  // 0xFE -> 1111 1110;
        gfpOut32(puerto_base + 2, aux ^ MASK_OUT);

        // Recorrer el dato a la derecha para mandar el siguiente bit.
        aux = aux >> 1;
    }

    // Activar flanco positivo en C1 para cargar a los displays.
    aux = gfpInp32(puerto_base + 2) ^ MASK_OUT;
    aux = aux | 0x02;
    gfpOut32(puerto_base + 2, aux ^ MASK_OUT);
    Sleep(10);
}

WORD ReadSerial16bitsPort(short port) {
    const WORD mask_control = 0x0B;
    const WORD mask_status = 0x80;

    // S7 is the input data (address 0x80)
    // C2 is connected to SH/LD' (address 0x04)
    // C3 is connected to clock (address 0x08)
    const WORD s7 = 0x80;
    const WORD c2 = 0x04;
    const WORD c3 = 0x08;

    // algorithm:
    // Load data to SN74LS165
    // Stop loading data to SN74LS165
    // for 16 times, going from 15 to 0:
    //   Read the S7 data
    //   make a clock cycle of C3

    unsigned char status;
    unsigned char dato = 0;
    unsigned int i;
    unsigned int aux;
    unsigned int control;

    // C2 should be 0 when reading data of the switches
    // and at 1 when shifting the data.
    // Once C2 is 1, the data is shifted on a positive transition of C3.
    control = gfpInp32(port + 2) ^ mask_control;
    control &= (0xFF ^ c2);
    control &= (0xFF ^ c3);
    gfpOut32(port + 2, control ^ mask_control);
    Sleep(100);

    // Disable the load.
    control |= c2;

    for (i = 15, aux = 0x01 << 15; i >= 0; --i, aux >>= 1) {
        // Read Status port, and check S7.
        status = gfpInp32(port + 1) ^ mask_status;
        if (status & c2) {
            dato |= aux;
        }

        // Shift the data.
        control |= c3;
        gfpOut32(port + 2, control ^ mask_control);
        Sleep(100);
        control &= (0xFF ^ c3);
        gfpOut32(port + 2, control ^ mask_control);
        Sleep(100);
    }

    return dato;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        // Enviar un mensaje de error al usuario, cuando no ha introducido los parámetros
        // correctos
        printf(
            "Error : faltan parametros\n\n***** Uso correcto *****\n\nmyReadWritePort read "
            "<ADDRESS> \no \nmyReadWritePort write <ADDRESS> <DATA>\n\n\n\n\n");
    } else {
        // Cargar el DLL a nuestro espacio de memoria
        HINSTANCE hInpOutDll;
        hInpOutDll = LoadLibrary(
            "D:\\lugol\\Documents\\TEC\\Tareas_"
            "Fall2020\\Interfaces\\Lab\\Practica1\\src\\CodigoFuente\\inpoutx64.dll");  // Ruta
                                                                                        // y
                                                                                        // nombre
                                                                                        // del
                                                                                        // DLL
        // En la práctica #2 modificaremos el DLL para una funcionalidad adicional
        // hInpOutDll =
        // LoadLibrary("D:\\Ricardo\\ITESM\\IEC\\PuertoParalelo\\Practica2\\Debug\\DLL_Practica2.DLL");
        // //Ruta y nombre del DLL
        if (hInpOutDll != NULL) {
            gfpOut32 = (lpOut32)GetProcAddress(hInpOutDll,
                                               "Out32");  // Debería ser Out64 pero el DLL tiene
                                                          // el "system call" "hardcoded" a 32
            gfpInp32 = (lpInp32)GetProcAddress(hInpOutDll,
                                               "Inp32");  // Debería ser Inp64 pero el DLL tiene
                                                          // el "system call" "hardcoded" a 32
            gfpIsInpOutDriverOpen =
                (lpIsInpOutDriverOpen)GetProcAddress(hInpOutDll, "IsInpOutDriverOpen");

            if (gfpIsInpOutDriverOpen()) {
                if (!strcmp(argv[1], "read")) {
                    short iPort = atoi(argv[2]);
                    printf("Dato leido del puerto %s ==> %d \n\n\n\n", argv[2],
                           ReadSerial16bitsPort(iPort));
                } else if (!strcmp(argv[1], "write")) {
                    short iPort = atoi(argv[2]);
                    WORD dato = atoi(argv[2]);
                    sendData(iPort, dato);
                    printf("Wrote: %d", dato);
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
