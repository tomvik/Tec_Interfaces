#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

bool Write(char *string_dato) {
    const unsigned char dato = atoi(string_dato);
    DWORD aux = 0;

    HANDLE device = CreateFile("\\\\.\\PTOPARRnW", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (device != INVALID_HANDLE_VALUE) {
        WriteFile(device, &dato, 1, &aux, NULL);
        if (CloseHandle(device) == 0) {
            return false;
        }
    } else {
        return false;
    }

    return aux == 1;
}

bool Read() {
    unsigned char dato = 0;
    DWORD aux = 0;

    HANDLE device = CreateFile("\\\\.\\PTOPARRnW", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

    if (device != INVALID_HANDLE_VALUE) {
        if (ReadFile(device, &dato, 1, &aux, NULL)) {
            printf("Read the data: %x\n", dato);
        } else {
            return false;
        }
        if (CloseHandle(device) == 0) {
            return false;
        }
    } else {
        return false;
    }

    return aux == 1;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        printf("Error : de parametros.\n\n\n\n\n");
        return 1;
    }
    if (!strcmp(argv[1], "writeData") && argc == 3) {
        if (Write(argv[2])) {
            printf("Se logró escribir de manera exitosa el dato: %s\n", argv[2]);
        } else {
            return 2;
        }
    } else if (!strcmp(argv[1], "readData") && argc == 2) {
        if (Read()) {
            printf("Se logró leer de manera exitosa.\n");
        } else {
            return 3;
        }
    }
    return 0;
}
