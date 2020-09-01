#include "stdio.h"
#include "windows.h"

#define S_BUSY 0x80
#define S_ACK 0x40
#define S_PAPER_END 0x20
#define S_SELECT_IN 0x10
#define S_nERROR 0x08

typedef void(__stdcall *lpOut32)(short, short);
typedef short(__stdcall *lpInp32)(short);
typedef BOOL(__stdcall *lpIsInpOutDriverOpen)(void);
typedef BOOL(__stdcall *lpIsXP64Bit)(void);

// Apuntadores a rutinas del DLL útiles para acceso al "Device Driver"
lpOut32 gfpOut32;
lpInp32 gfpInp32;
lpIsInpOutDriverOpen gfpIsInpOutDriverOpen;
lpIsXP64Bit gfpIsXP64Bit;

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

void ByteToDisplayHex(const WORD byte, WORD* lower_nibble, WORD* high_nibble) {
    // Convert the value of the byte to its representation in the seven segment display.
}

void SendToPortDisplay(short port, WORD dato, WORD deactivate_bit) {
    const WORD deactivate_mask = 0xFF ^ deactivate_bit;
    const WORD mask_control = 0x0B;

    WORD control_data = gfpInp32(port + 2) ^ mask_control;

    gfpOut32(port + 2, (control_data & deactivate_mask) ^ mask_control);
    Sleep(100);

    gfpOut32(port, dato);
    Sleep(100);

    gfpOut32(port + 2, (control_data | deactivate_bit) ^ mask_control);
    Sleep(100);
}

void WriteByteToHexInDisplay(short port, WORD dato) {
    WORD dato_low, dato_high;
    ByteToDisplayHex(dato, &dato_low, &dato_high);
    SendToPortDisplay(port, dato_low, 0x01);
    SendToPortDisplay(port, dato_high, 0x02);
}

WORD ReadPortInput(short port, WORD deactivate_bit) {
    const WORD deactivate_mask = 0xFF ^ deactivate_bit;
    const WORD mask_control = 0x0B;
    const WORD status_control = 0x80;
    unsigned char mask_status[] = {S_BUSY, S_PAPER_END, S_SELECT_IN, S_nERROR,
                                   S_BUSY, S_PAPER_END, S_SELECT_IN, S_nERROR};
    unsigned char status;
    unsigned char dato = 0;
    unsigned int i;
    unsigned int aux = 0x01;
    unsigned int control;

    for (i = 0; i < 8; i++, aux <<= 1) {
        if (i == 0) {
            control = gfpInp32(port + 2) ^ mask_control;
            control = control & deactivate_mask;
            gfpOut32(port + 2, control ^ mask_control);
            Sleep(100);
        } else if (i == 4) {
            control = gfpInp32(port + 2) ^ mask_control;
            control = control | deactivate_bit;
            gfpOut32(port + 2, control ^ mask_control);
            Sleep(100);
        }

        status = gfpInp32(port + 1) ^ status_control;

        if ((status & mask_status[i]) != 0) dato = dato | aux;
    }

    return (dato);  // b7b6b5b4b3b2b1b0
}

WORD Read16bitsPort(short port) {
    const WORD mask_control = 0x0B;

    WORD control_data = gfpInp32(port + 2) ^ mask_control;

    gfpOut32(port + 2, (control_data & 0xFB) ^ mask_control);
    Sleep(100);

    WORD upper_byte = ReadPortInput(port, 0x08);

    gfpOut32(port + 2, (control_data | 0x04) ^ mask_control);
    Sleep(100);

    WORD low_byte = ReadPortInput(port, 0x08);

    return (upper_byte << 8) + low_byte;
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
