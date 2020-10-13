/*******************************************************************************
  MPLAB Harmony Application Source File
  
  Company:
    Microchip Technology Inc.
  
  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013-2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "app.h"


// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

/*****************************************
 * Buffer required for reading and sending
 * data over CDC
 *****************************************/
 uint8_t APP_MAKE_BUFFER_DMA_READY readBuffer[64];

/**********************
 * Switch Prompt.
 ***********************/
const uint8_t __attribute__((aligned(16))) switchPrompt[] = "\r\nPUSH BUTTON PRESSED";
uint8_t APP_MAKE_BUFFER_DMA_READY miString[] = "                                 ";

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;

//********************** LRSG v

#define TRANS_COUNT 8
#define EDO_COUNT 9

char chr=0;
int acum1=0;
int acum2=0;
int res=0;
enum Oper{Suma,Resta,Mult,Div};
enum Oper oper;

int edo=0;
int edoAnt=0;
int trans=0;
int miPrintf_flag=0;
int miStringCont=0;
char auxString[] = "                                 ";

					//0-->Inválida
					//6-->Digito
					//7-->Operador
int chrTrans[TRANS_COUNT]=
					{ 0,'(',')','=',  8, 27, 6 , 7};
int mtzTrans[EDO_COUNT][TRANS_COUNT]={
					{ 0, 1 , 0 , 0 , 0 , 0 , 0 , 0},
					{ 1, 1 , 1 , 1 , 99, 99, 2 , 1},
					{ 2, 2 , 2 , 2 , 99, 99, 3 , 4},
					{ 3, 2 , 2 , 2 , 99, 99, 2 , 2},
					{ 4, 4 , 4 , 4 , 99, 99, 5 , 4},
					{ 5, 5 , 7 , 5 , 99, 99, 6 , 5},
					{ 6, 5 , 5 , 5 , 99, 99, 5 , 5},
					{ 7, 7 , 7 , 8 , 99, 99, 7 , 7},
					{ 8, 0 , 0 , 0 , 0 , 0 , 0 , 0}};


void miPrintf(char* s, int cont) {
    int i;
    for (i=0;i<cont;i++)
        miString[i]=s[i];
    miPrintf_flag=1;
    miStringCont=cont;
}

int calcTrans(char chr) {
	int trans=0;
	if ((chr>='0')&&(chr<='9'))	//Digito
		return(6);
	switch (chr) {
		case'+':
		case'-':
		case'*':
		case'/':
				return(7);
	}
	for (trans=5;trans>0;trans--)
		if (chr==chrTrans[trans])
			break;
	return(trans);
}

int sigEdo(int edo, int trans) {
	return(mtzTrans[edo][trans]);
}

int ejecutaEdo(int edo) {
    static int i=0;
    static int negativoFlag=0;
    static int digitosCont=0;
    static int auxRes=0;
	switch(edo) {
		case 0:
				break;
		case 1:
                BSP_LEDOff( APP_USB_LED_1);
                BSP_LEDOff( APP_USB_LED_2);
                BSP_LEDOff( APP_USB_LED_3);
				acum1=0;
				miPrintf(&chr,1);
				break;
		case 2:
				miPrintf(&chr,1);
				acum1*=10;
				acum1+=(chr-'0');
				break;
		case 3:
				miPrintf(&chr,1);
				acum1*=10;
				acum1+=(chr-'0');
				return(2);
		case 4:
                BSP_LEDOn(  APP_USB_LED_1);
                BSP_LEDOff( APP_USB_LED_2);
                BSP_LEDOff( APP_USB_LED_3);
				miPrintf(&chr,1);
				switch (chr) {
					case'+':
							oper=Suma;
							break;
					case'-':
							oper=Resta;
							break;
					case'*':
							oper=Mult;
							break;
					case'/':
							oper=Div;
							break;
				}
				acum2=0;	//Preparar la entrada al estado 4
				break;
		case 5:
                BSP_LEDOff( APP_USB_LED_1);
                BSP_LEDOn(  APP_USB_LED_2);
                BSP_LEDOff( APP_USB_LED_3);
				miPrintf(&chr,1);
				acum2*=10;
				acum2=(chr-'0');
				break;
		case 6:
				miPrintf(&chr,1);
				acum2*=10;
				acum2+=(chr-'0');
				return(5);
		case 7:
                BSP_LEDOff( APP_USB_LED_1);
                BSP_LEDOff( APP_USB_LED_2);
                BSP_LEDOn(  APP_USB_LED_3);
				miPrintf(&chr,1);
				break;
		case 8:
                BSP_LEDOn( APP_USB_LED_1);
                BSP_LEDOn( APP_USB_LED_2);
                BSP_LEDOn( APP_USB_LED_3);
				switch(oper) {
					case Suma:
							res=acum1+acum2;
							break;
					case Resta:
							res=acum1-acum2;
							break;
					case Mult:
							res=acum1*acum2;
							break;
					case Div:
							if (acum2)
								res=acum1/acum2;
							else
								res=-1;
							break;
				}
				//printf("%d\n",res);
                if (res<0) {
                    negativoFlag=1;
                    res=-1*res;
                } else {
                    negativoFlag=0;
                }
                auxRes=res;
                digitosCont=0;
                do {
                    auxRes/=10;
                    digitosCont++;
                } while(auxRes);
                i=digitosCont;
                do {
                    auxString[negativoFlag+i]='0'+(res%10);
                    res/=10;
                } while(--i>=0);
                auxString[0]='=';
                if (negativoFlag) {
                    auxString[1]='-';
                }
                auxString[digitosCont+1+negativoFlag]=0x0D; //Carriage return
                miPrintf(&auxString[0],digitosCont+1+negativoFlag+1);
				return(0);
		case 99:
				//printf("\n<<<Captura cancelada>>>\n");
				return(0);	//Estado aceptor, rompe la rutina y marca estado de salida
	}
	return(edo);	//Para estados no aceptores regresar el estado ejecutado
}

//********************** LRSG 

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/************************************************
 * CDC COM1 Application Event Handler
 ************************************************/

void APP_USBDeviceCDCEventHandler
(
    USB_DEVICE_CDC_INDEX index ,
    USB_DEVICE_CDC_EVENT event ,
    void* pData,
    uintptr_t userData
)
{
...
}

/*************************************************
 * Application Device Layer Event Handler
 *************************************************/

void APP_USBDeviceEventCallBack ( USB_DEVICE_EVENT event, void * eventData, uintptr_t context )
{
...
}


// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

/************************************************
 * Application State Reset Function
 ************************************************/

bool APP_StateReset(void)
{
...
}

/************************************************
 * Switch Procesing routine
 ************************************************/
void APP_SwitchStateProcess(void)
{
...
}



// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
...
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks (void)
{
    /* Update the application state machine based
     * on the current state */
    int i; 
    /* Update the switch press */
    APP_SwitchStateProcess();

    switch(appData.state)
    {
        case APP_STATE_INIT:
            ...
            break;
        case APP_STATE_WAIT_FOR_CONFIGURATION:
            ...
            break;
        case APP_STATE_SCHEDULE_READ:
            ...
            break;
        case APP_STATE_WAIT_FOR_READ_COMPLETE:
        case APP_STATE_CHECK_SWITCH_PRESSED:
            ...
            break;
        case APP_STATE_SCHEDULE_WRITE:

            if(APP_StateReset())
            {
                break;
            }

            /* Setup the write */

            appData.writeTransferHandle = USB_DEVICE_CDC_TRANSFER_HANDLE_INVALID;
            appData.writeIsComplete = true;
            appData.state = APP_STATE_WAIT_FOR_WRITE_COMPLETE;

            if(appData.switchIsPressed)
            {
                /* If the switch was pressed, then send the switch prompt*/
                appData.switchIsPressed = false;
                USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0, &appData.writeTransferHandle,
                     switchPrompt, 23, USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
            }
            else
            {
                /* Else echo each received character by adding 1 */
                for(i=0; i<appData.numBytesRead; i++)
                {
                    if((appData.readBuffer[i] != 0x0A) && (appData.readBuffer[i] != 0x0D))
                    {
                    //    miString[i] = appData.readBuffer[i] + 1; miPrintf_flag=1; miStringCont=i+1;

                        //El código de la calculadora funciona con la sintaxis (1234+1)=
                        // el PIC32MZ regresará el resultado en justo después del caracter '='
                        chr=appData.readBuffer[i];
                        trans=calcTrans(chr);	//Calcular la transición según la entrada del teclado
                        if (trans) {			//Validar por transición valida (la transición 0 es inválida)
                            edoAnt=edo;					//Guardar el estado anterior
                            edo=sigEdo(edoAnt,trans);	//Calcular el siguiente estado
                            if (edoAnt!=edo)			//Solo si hay cambio de estado hay que ...
                                edo=ejecutaEdo(edo);	// ... ejecutar el nuevo estado y asignar estado de continuidad
                        }
                    }
                }
                if (miPrintf_flag) {
                    USB_DEVICE_CDC_Write(USB_DEVICE_CDC_INDEX_0, &appData.writeTransferHandle,
                                            miString, miStringCont, USB_DEVICE_CDC_TRANSFER_FLAGS_DATA_COMPLETE);
                    miPrintf_flag=0;
                    miStringCont=0;
                }
            }
            break;
        case APP_STATE_WAIT_FOR_WRITE_COMPLETE:
            ...
            break;
        case APP_STATE_ERROR:
            break;
        default:
            break;
    }
}

 

/*******************************************************************************
 End of File
 */

