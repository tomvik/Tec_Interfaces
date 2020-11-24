#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <SETUPAPI.H>

//----------------------------------------------
#define RICH_VENDOR_ID 0x0000
#define RICH_USBHID_GENIO_ID 0x2019

#define INPUT_REPORT_SIZE 64
#define OUTPUT_REPORT_SIZE 64

#define TOTAL_SWITCHES 3
//----------------------------------------------

typedef struct _HIDD_ATTRIBUTES {
    ULONG Size;  // = sizeof (struct _HIDD_ATTRIBUTES)
    USHORT VendorID;
    USHORT ProductID;
    USHORT VersionNumber;
} HIDD_ATTRIBUTES, *PHIDD_ATTRIBUTES;

typedef VOID(__stdcall* PHidD_GetProductString)(HANDLE, PVOID, ULONG);
typedef VOID(__stdcall* PHidD_GetHidGuid)(LPGUID);
typedef BOOLEAN(__stdcall* PHidD_GetAttributes)(HANDLE, PHIDD_ATTRIBUTES);
typedef BOOLEAN(__stdcall* PHidD_SetFeature)(HANDLE, PVOID, ULONG);
typedef BOOLEAN(__stdcall* PHidD_GetFeature)(HANDLE, PVOID, ULONG);

//----------------------------------------------

HINSTANCE hHID = NULL;
PHidD_GetProductString HidD_GetProductString = NULL;
PHidD_GetHidGuid HidD_GetHidGuid = NULL;
PHidD_GetAttributes HidD_GetAttributes = NULL;
PHidD_SetFeature HidD_SetFeature = NULL;
PHidD_GetFeature HidD_GetFeature = NULL;
HANDLE DeviceHandle = INVALID_HANDLE_VALUE;

unsigned int moreHIDDevices = TRUE;
unsigned int HIDDeviceFound = FALSE;

unsigned int terminaAbruptaEInstantaneamenteElPrograma = 0;

#define kMatrixSize 2
#define kExitCommand 4

static float matrix_a[kMatrixSize][kMatrixSize];
static float matrix_b[kMatrixSize][kMatrixSize];
static float matrix_c[kMatrixSize][kMatrixSize];
static float* p_temp = NULL;

void Load_HID_Library(void) {
    hHID = LoadLibrary("HID.DLL");
    if (!hHID) {
        printf("Failed to load HID.DLL\n");
        return;
    }

    HidD_GetProductString = (PHidD_GetProductString)GetProcAddress(hHID, "HidD_GetProductString");
    HidD_GetHidGuid = (PHidD_GetHidGuid)GetProcAddress(hHID, "HidD_GetHidGuid");
    HidD_GetAttributes = (PHidD_GetAttributes)GetProcAddress(hHID, "HidD_GetAttributes");
    HidD_SetFeature = (PHidD_SetFeature)GetProcAddress(hHID, "HidD_SetFeature");
    HidD_GetFeature = (PHidD_GetFeature)GetProcAddress(hHID, "HidD_GetFeature");

    if (!HidD_GetProductString || !HidD_GetAttributes || !HidD_GetHidGuid || !HidD_SetFeature ||
        !HidD_GetFeature) {
        printf("Couldn't find one or more HID entry points\n");
        return;
    }
}

int Open_Device(const USHORT desired_vendor_id, const USHORT desired_product_id) {
    HDEVINFO DeviceInfoSet;
    GUID InterfaceClassGuid;
    SP_DEVICE_INTERFACE_DATA DeviceInterfaceData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA pDeviceInterfaceDetailData;
    HIDD_ATTRIBUTES Attributes;
    DWORD Result;
    DWORD MemberIndex = 0;
    DWORD Required;

    // Validar si se "carg�" la biblioteca (DLL)
    if (!hHID) return (0);

    // Obtener el Globally Unique Identifier (GUID) para dispositivos HID
    HidD_GetHidGuid(&InterfaceClassGuid);
    // Sacarle a Windows la informaci�n sobre todos los dispositivos HID instalados y activos en el
    // sistema
    // ... almacenar esta informaci�n en una estructura de datos de tipo HDEVINFO
    DeviceInfoSet =
        SetupDiGetClassDevs(&InterfaceClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_INTERFACEDEVICE);
    if (DeviceInfoSet == INVALID_HANDLE_VALUE) return (0);

    // Obtener la interfaz de comunicaci�n con cada uno de los dispositivos para preguntarles
    // informaci�n espec�fica
    DeviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    while (!HIDDeviceFound) {
        // ... utilizando la variable MemberIndex ir preguntando dispositivo por dispositivo ...
        moreHIDDevices = SetupDiEnumDeviceInterfaces(DeviceInfoSet, NULL, &InterfaceClassGuid,
                                                     MemberIndex, &DeviceInterfaceData);
        if (!moreHIDDevices) {
            // ... si llegamos al fin de la lista y no encontramos al dispositivo ==> terminar y
            // marcar error
            SetupDiDestroyDeviceInfoList(DeviceInfoSet);
            return (0);  // No more devices found
        } else {
            // Necesitamos preguntar, a trav�s de la interfaz, el PATH del dispositivo, para eso ...
            // ... primero vamos a ver cu�ntos caracteres se requieren (Required)
            Result = SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData, NULL, 0,
                                                     &Required, NULL);
            pDeviceInterfaceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(Required);
            if (pDeviceInterfaceDetailData == NULL) {
                printf("Error en SetupDiGetDeviceInterfaceDetail\n");
                return (0);
            }
            // Ahora si, ya que el "buffer" fue preparado (pDeviceInterfaceDetailData{DevicePath}),
            // vamos a preguntar PATH
            pDeviceInterfaceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
            Result =
                SetupDiGetDeviceInterfaceDetail(DeviceInfoSet, &DeviceInterfaceData,
                                                pDeviceInterfaceDetailData, Required, NULL, NULL);
            if (!Result) {
                printf("Error en SetupDiGetDeviceInterfaceDetail\n");
                free(pDeviceInterfaceDetailData);
                return (0);
            }
            // Para este momento ya sabemos el PATH del dispositivo, ahora hay que preguntarle ...
            // ... su VID y PID, para ver si es con quien nos interesa comunicarnos
            printf("Found? ==> ");
            printf("Device: %s\n", pDeviceInterfaceDetailData->DevicePath);

            // Obtener un "handle" al dispositivo
            DeviceHandle =
                CreateFile(pDeviceInterfaceDetailData->DevicePath, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, (LPSECURITY_ATTRIBUTES)NULL,
                           OPEN_EXISTING, 0, NULL);

            if (DeviceHandle == INVALID_HANDLE_VALUE) {
                printf("���Error en el CreateFile!!!\n");
            } else {
                // Preguntar por los atributos del dispositivo
                Attributes.Size = sizeof(Attributes);
                Result = HidD_GetAttributes(DeviceHandle, &Attributes);
                if (!Result) {
                    printf("Error en HIdD_GetAttributes\n");
                    CloseHandle(DeviceHandle);
                    free(pDeviceInterfaceDetailData);
                    return (0);
                }
                // Analizar los atributos del dispositivo para verificar el VID y PID
                printf("MemberIndex=%d,VID=%04x,PID=%04x\n", MemberIndex, Attributes.VendorID,
                       Attributes.ProductID);
                if ((Attributes.VendorID == desired_vendor_id) &&
                    (Attributes.ProductID == desired_product_id)) {
                    printf("USB/HID GenIO ==> ");
                    printf("Device: %s\n", pDeviceInterfaceDetailData->DevicePath);
                    HIDDeviceFound = TRUE;
                } else
                    CloseHandle(DeviceHandle);
            }
            MemberIndex++;
            free(pDeviceInterfaceDetailData);
            if (HIDDeviceFound) {
                printf("Dispositivo HID solicitado ... ���localizado!!!, presione <ENTER>\n");
                getchar();
            }
        }
    }
    return (1);
}

void Close_Device(void) {
    if (DeviceHandle != NULL) {
        CloseHandle(DeviceHandle);
        DeviceHandle = NULL;
    }
}

void Get_Desired_Ids(PUSHORT desired_vendor_id, PUSHORT desired_product_id) {
    printf("Input the desired 4 digit Vendor id in hex and use capital letters\n");
    printf("\tVendor id: 0x");
    scanf_s("%X", desired_vendor_id);
    printf("\nInput the desired 4 digit Product id in hex and use capital letters\n");
    printf("\tProduct id: 0x");
    scanf_s("%X", desired_product_id);
    printf("\n The vendor id is: 0x%04X and the product id is: 0x%04X", *desired_product_id,
           *desired_vendor_id);
}

void Fill_Matrices() {
    printf("Now you'll input the matrix data of the two matrices.\n");
    printf(
        "It will start on the upper left corner, finishing the first row, and then continuing with "
        "the next ones.\n");

    for (int row = 0; row < kMatrixSize; ++row) {
        for (int col = 0; col < kMatrixSize; ++col) {
            printf("Matrix A, row %d and column %d: ", row + 1, col + 1);
            scanf_s("%f", &matrix_a[row][col]);
        }
    }

    for (int row = 0; row < kMatrixSize; ++row) {
        for (int col = 0; col < kMatrixSize; ++col) {
            printf("Matrix B, row %d and column %d: ", row + 1, col + 1);
            scanf_s("%f", &matrix_b[row][col]);
        }
    }

    printf("The Matrix A looks like:\n");
    for (int row = 0; row < kMatrixSize; ++row) {
        for (int col = 0; col < kMatrixSize; ++col) {
            printf("%0.02f ", matrix_a[row][col]);
        }
        printf("\n");
    }

    printf("The Matrix B looks like:\n");
    for (int row = 0; row < kMatrixSize; ++row) {
        for (int col = 0; col < kMatrixSize; ++col) {
            printf("%0.02f ", matrix_b[row][col]);
        }
        printf("\n");
    }
}

void Get_Execution_Case(PUSHORT execution_case) {
    printf("What do you wish to do with the matrices A and B?\n");
    printf("1. Multiply them (A * B)\n");
    printf("2. Add them (A + B)\n");
    printf("3. Substract them (A - B)\n");
    printf("4. Quit the program\n");
    printf("Please enter a single digit representing the option you wish to perform\n");

    printf("\tDesired option: ");
    scanf_s("%x", execution_case);

    printf("\n");
}

void Get_Led_And_State(unsigned char* num_led, unsigned char* led_state) {
    printf("Which led do you wish to modify? enter the digit of the led 1, 2 or 3?\n");

    printf("\tLed: ");
    scanf_s("%x", num_led);

    printf("Do you wish to turn it on or off? enter the digit of the state: 1 = on, 2 = off\n");

    printf("\tState: ");
    scanf_s("%x", led_state);

    printf("\n");
}

int Touch_Device(const USHORT execution_case) {
    DWORD BytesRead = 0;
    DWORD BytesWritten = 0;
    unsigned char reporteEntrada[INPUT_REPORT_SIZE + 1];
    unsigned char reporteSalida[OUTPUT_REPORT_SIZE + 1];
    int status = 0;

    if (DeviceHandle == NULL)  // Validar que haya comunicacion con el dispositivo
        return 0;

    switch (execution_case) {
        case 1:
            reporteSalida[0] = 0x00;
            reporteSalida[1] = 0x83;
            reporteSalida[2] = kMatrixSize;
            reporteSalida[3] = kMatrixSize;

            status =
                WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
            if (!status) {
                printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);
                return status;
            }
            printf("Sent the dimensions of the matrices\n");

            const int last_index = 2;
            const int size_float = 4;

            for (int row = 0; row < kMatrixSize; ++row) {
                reporteSalida[0] = 0x00;
                reporteSalida[1] = 0x84;
                for (int col = 0; col < kMatrixSize; ++col) {
                    p_temp = (float*)&reporteSalida[last_index + (col * size_float)];
                    *p_temp = matrix_a[row][col];
                }
                status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1,
                                   &BytesWritten, NULL);
                if (!status) {
                    printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);
                    return status;
                }
                printf("The row %d of Matrix A was sent.\n", row + 1);
            }

            for (int row = 0; row < kMatrixSize; ++row) {
                reporteSalida[0] = 0x00;
                reporteSalida[1] = 0x84;
                for (int col = 0; col < kMatrixSize; ++col) {
                    p_temp = (float*)&reporteSalida[last_index + (col * size_float)];
                    *p_temp = matrix_b[row][col];
                }
                status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1,
                                   &BytesWritten, NULL);
                if (!status) {
                    printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);
                    return status;
                }
                printf("The row %d of Matrix B was sent.\n", row + 1);
            }

            for (int row = 0; row < kMatrixSize; ++row) {
                reporteSalida[0] = 0x00;
                reporteSalida[1] = 0x85;

                status = WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1,
                                   &BytesWritten, NULL);
                if (!status) {
                    printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);
                    return status;
                }
                printf("Sent petition for result\n");

                memset(&reporteEntrada, 0, INPUT_REPORT_SIZE + 1);
                status =
                    ReadFile(DeviceHandle, reporteEntrada, INPUT_REPORT_SIZE + 1, &BytesRead, NULL);
                if (!status) {
                    printf("Error en el ReadFile: %d\n", GetLastError());
                    return status;
                }

                for (int col = 0; col < kMatrixSize; ++col) {
                    p_temp = (float*)&reporteEntrada[last_index + (col * size_float)];
                    printf("%x %x %f\n", reporteEntrada[0], reporteEntrada[1], *p_temp);
                    matrix_c[row][col] = *p_temp;
                }
                printf("The row %d of Matrix C was received.\n", row + 1);
            }

            printf("The Matrix C looks like:\n");
            for (int row = 0; row < kMatrixSize; ++row) {
                for (int col = 0; col < kMatrixSize; ++col) {
                    printf("%0.02f ", matrix_c[row][col]);
                }
                printf("\n");
            }
            break;
        case 2:
            reporteSalida[0] = 0x00;
            reporteSalida[1] = 0x81;
            reporteSalida[2] = 0;

            status =
                WriteFile(DeviceHandle, reporteSalida, OUTPUT_REPORT_SIZE + 1, &BytesWritten, NULL);
            if (!status) {
                printf("Error en el WriteFile %d %d\n", GetLastError(), BytesWritten);
            } else {
                printf(
                    "Se enviaron %d bytes al dispositivo preguntando por el estado de los "
                    "switches\n",
                    BytesWritten);
                memset(&reporteEntrada, 0, INPUT_REPORT_SIZE + 1);
                status =
                    ReadFile(DeviceHandle, reporteEntrada, INPUT_REPORT_SIZE + 1, &BytesRead, NULL);
                if (!status) {
                    printf("Error en el ReadFile: %d\n", GetLastError());
                } else {
                    const unsigned char switches = (unsigned char)reporteEntrada[2];

                    for (int current = 0; current < TOTAL_SWITCHES; ++current) {
                        printf("\tSwitch %d: %x\n", current + 1,
                               ((~switches) & (1 << current)) >> current);
                    }
                }
            }
            break;
        default:
            break;
    }
    printf("\n\n");
    return status;
}

void main() {
    Load_HID_Library();
    static USHORT desired_vendor_id = 0;
    static USHORT desired_product_id = 0;
    static USHORT execution_case = 0;

    Get_Desired_Ids(&desired_vendor_id, &desired_product_id);
    if (Open_Device(desired_vendor_id, desired_product_id)) {
        printf("Vamos bien\n");
        Fill_Matrices();
        while ((!_kbhit()) && (!terminaAbruptaEInstantaneamenteElPrograma)) {
            Get_Execution_Case(&execution_case);
            if (execution_case == kExitCommand) break;
            Touch_Device(execution_case);
        }
    } else {
        printf(">:(\n");
    }
    Close_Device();
}