// Read/Write request processors for PtoPar driver
// Copyright (C) 1999 by Walter Oney
// All rights reserved

#include "stddcls.h"
#include "driver.h"

VOID OnCancelReadWrite(PDEVICE_OBJECT fdo, PIRP Irp);

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS DispatchCreate(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchCreate
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	// Claim the remove lock in Win2K so that removal waits until the
	// handle closes. Don't do this in Win98, however, because this
	// device might be removed by surprise with handles open, whereupon
	// we'll deadlock in HandleRemoveDevice waiting for a close that
	// can never happen because we can't run the user-mode code that
	// would do the close.

	NTSTATUS status;
	status = IoAcquireRemoveLock(&pdx->RemoveLock, stack->FileObject);
	pdx->datoDD=0;
	if (NT_SUCCESS(status))
		{						// okay to open
		if (InterlockedIncrement(&pdx->handles) == 1)
			{					// first open handle
			}					// okay to open
		}					// first open handle
	return CompleteRequest(Irp, status, 0);
	}							// DispatchCreate

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS DispatchClose(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchClose
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	if (InterlockedDecrement(&pdx->handles) == 0)
		{						// no more open handles
		}						// no more open handles
	
	// Release the remove lock to match the acquisition done in DispatchCreate

	IoReleaseRemoveLock(&pdx->RemoveLock, stack->FileObject);

	return CompleteRequest(Irp, STATUS_SUCCESS, 0);
	}							// DispatchClose

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS DispatchReadNAND(PDEVICE_OBJECT fdo, PIRP Irp) {
   PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
   PUCHAR pDato = (PUCHAR) Irp->AssociatedIrp.SystemBuffer;
   /// Lógica:
   // Para probar el funcionamiento de cada NAND, hay que recordar primeramente
   // la tabla del NAND. Y, verificar que cada uno esté dando el resultado esperado.
   // Tabla:
   // A B Y
   // 0 0 1
   // 0 1 1
   // 1 0 1
   // 1 1 0
   // La lógica será ir por cada una de estas entradas y compararlas con la salida recibida.
   // Si alguna de las salidas no corresponde a lo esperado, se asumirá que ese NAND no funciona.
   // Y por lo tanto el bit correspondiente se prendera.

   // Conexiones:
   // D0 nand D1 = S3
   // D2 nand D3 = S4
   // D3 nand D5 = S5
   // D6 nand D7 = S7
										   // 76 54 32 10
   unsigned char datos_entrada[4] = {0x00, // 00 00 00 00
   									 0x55, // 01 01 01 01
   									 0xCC, // 10 10 10 10
   									 0xFF  // 11 11 11 11
									};
										  // 76 54 32 10
   unsigned char datos_salida[4] = {0xD8, // 10 11 10 00
   									0xD8, // 10 11 10 00
   									0xD8, // 10 11 10 00
   									0x00, // 00 00 00 00
									};

	const unsigned char status_number[4] = {3, 4, 5, 7};
	const unsigned char shift_status_amount[4] = {3, 3, 3, 4};
	
	const unsigned char kPuertoBase = 0x378;
	const unsigned char kStatusMask = 0x80;
	const unsigned char kImportantBits = 0xD8;
	unsigned char final_status = 0;

	for(int i = 0; i < 4; ++i) {
		WRITE_PORT_UCHAR((unsigned char *)(kPuertoBase), datos_entrada[i]);

		unsigned char dato;
   		dato = READ_PORT_UCHAR((unsigned char *)(kPuertoBase + 1)) ^ kStatusMask;

		// Turn off the bits we don't care about.
		dato &= kImportantBits;
		// If there's a difference between the bits, it will be shown as a 1.
		dato ^= datos_salida[i];

		for(int j = 0; j < 4; j++) {
			// Check if the S3, S4, S5 or S7 is on.
			unsigned char current_status = dato & (1 << status_number[j]);
			// Move it the amount of times required to have the following mapping:
			// S7 = b3, S5 = b2, S4 = b1, S3 = b0 
			current_status = current_status >> (shift_status_amount[j]);

			final_status |= current_status;
		}

	}
   Irp->IoStatus.Status = STATUS_SUCCESS;
   Irp->IoStatus.Information = 1;
   *pDato = final_status;
   return STATUS_SUCCESS;
}

NTSTATUS DispatchRead(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchRead
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	PUCHAR pDato = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;


	// Port constants and variables.
#define S_BUSY 0x80
#define S_ACK 0x40
#define S_PAPER_END 0x20
#define S_SELECT_IN 0x10
#define S_nERROR 0x08

	const unsigned char puerto_base = 0x378;
	const unsigned char kMaskControl = 0x0B;
	const unsigned char activate_control_bit = 0x02;
	const unsigned char deactivate_control_mask = 0xFF ^ activate_control_bit;
	unsigned char control_dato = 0;

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
			control_data = READ_PORT_UCHAR((unsigned char *)(puerto_base + 2)) ^ kMaskControl;
			control_data &= deactivate_control_mask;
			WRITE_PORT_UCHAR((unsigned char *)(puerto_base + 2), control_data ^ kMaskControl);
            Sleep(50);
        } else if (i == 4) {
            // En el bit 4 nos aseguramos que el bit de control
            // deseado esté en 1, para leer el nibble más alto.
			control_data = READ_PORT_UCHAR((unsigned char *)(puerto_base + 2)) ^ kMaskControl;
			control_data |= activate_control_bit;
			WRITE_PORT_UCHAR((unsigned char *)(puerto_base + 2), control_data ^ kMaskControl);
            Sleep(50);
        }

        status = READ_PORT_UCHAR((unsigned char *)(puerto_base + 1)) ^ kStatusMask;

        // Si el bit que estamos leyendo en esta iteración es 1,
        // prende el bit de dato.
        if ((status & status_bit_mask[i]) != 0) {
            dato |= aux;
        }
    }
	
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 1;
	*pDato = dato;
	return STATUS_SUCCESS;
}							// DispatchRead

NTSTATUS  DispatchWriteP(PDEVICE_OBJECT fdo, PIRP Irp) {
  PUCHAR pDato = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
  unsigned char dato;
  unsigned char control;
  unsigned char digitos[]={0x3f,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x67,...,0x71};
  unsigned char i;
  PIO_STACK_LOCATION stack = IoGetNextIrpStackLocation(Irp);
  unsigned int contador = stack->Parameters.Read.Length;
  LARGE_INTEGER largeInteger; 
  control=READ_PORT_UCHAR((unsigned char *)0x37A);
  for (i=0;i<contador;i++) {
    dato=(UCHAR)*(pDato+i);
    control&=0xFE; //C0<--0
    WRITE_PORT_UCHAR((PUCHAR)0x37A, control);			//TODO
    WRITE_PORT_UCHAR((PUCHAR)0x378, digitos[dato%16]);
    control|=0x01; //C0<--1
    control&=0xFD; //C1<--0
    WRITE_PORT_UCHAR((PUCHAR)0x37A, control);			//TODO
    WRITE_PORT_UCHAR((PUCHAR)0x378, digitos[dato/16]);
    control|=0x02; //C1<--1
	KeDelayExecutionThread(KernelMode,FALSE,1000000);
  }
  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = contador;
  return STATUS_SUCCESS;
}

NTSTATUS DispatchWrite(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchWrite
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	PUCHAR pDato = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;

	// Port constants and variables.
	const unsigned char puerto_base = 0x378;
	const unsigned char kMaskControl = 0x0B;
	const unsigned char activate_control_bit = 0x01;
	const unsigned char deactivate_control_mask = 0XFE;
	unsigned char control_data = 0;
	
	// Switch control bit.
    control_data = READ_PORT_UCHAR((unsigned char *)(puerto_base + 2)) ^ kMaskControl;
	WRITE_PORT_UCHAR((unsigned char *)(puerto_base + 2), control_data & deactivate_control_mask);

	Sleep(50);

	// Write the data
	unsigned char dato = (unsigned char)*pDato;
	WRITE_PORT_UCHAR((unsigned char *)puerto_base, dato);

	Sleep(50);

	// Switch control bit.
	control_data = READ_PORT_UCHAR((unsigned char *)(puerto_base + 2)) ^ kMaskControl;
	WRITE_PORT_UCHAR((unsigned char *)(puerto_base + 2), control_data | activate_control_bit);

	Sleep(50);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 1;
	return STATUS_SUCCESS;
}							// DispatchWrite

#pragma LOCKEDCODE

VOID OnCancelReadWrite(IN PDEVICE_OBJECT fdo, IN PIRP Irp)
	{							// OnCancelReadWrite
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	CancelRequest(&pdx->dqReadWrite, Irp);
	}							// OnCancelReadWrite

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

VOID DpcForIsr(PKDPC Dpc, PDEVICE_OBJECT fdo, PIRP junk, PDEVICE_EXTENSION pdx)
	{							// DpcForIsr
	//PIRP nfyirp = UncacheControlRequest(pdx, &pdx->NotifyIrp);
	if (pdx->NotifyIrp)
		{					// complete notification IRP
		KdPrint((DRIVERNAME " ... Rich, DpcForIsr\n"));
		CompleteRequest(pdx->NotifyIrp, STATUS_SUCCESS, sizeof(ULONG));
		pdx->NotifyIrp=NULL;
		}					// complete notification IRP
	}							// DpcForIsr

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

BOOLEAN OnInterrupt(PKINTERRUPT InterruptObject, PDEVICE_EXTENSION pdx)
	{							// OnInterrupt
	IoRequestDpc(pdx->DeviceObject, NULL, pdx);
	return TRUE;
	}							// OnInterrupt

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS StartDevice(PDEVICE_OBJECT fdo, PCM_PARTIAL_RESOURCE_LIST raw, PCM_PARTIAL_RESOURCE_LIST translated)
	{							// StartDevice
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	NTSTATUS status;

	// Identify the I/O resources we're supposed to use.
	
	ULONG vector;
	KIRQL irql;
	KINTERRUPT_MODE mode;
	KAFFINITY affinity;
	BOOLEAN irqshare;
	BOOLEAN gotinterrupt = FALSE;

	PHYSICAL_ADDRESS portbase;
	BOOLEAN gotport = FALSE;
	UCHAR imr,control;
	
	if (!translated)
		return STATUS_DEVICE_CONFIGURATION_ERROR;		// no resources assigned??

	PCM_PARTIAL_RESOURCE_DESCRIPTOR resource = translated->PartialDescriptors;
	ULONG nres = translated->Count;
	for (ULONG i = 0; i < nres; ++i, ++resource)
		{						// for each resource
		switch (resource->Type)
			{					// switch on resource type

		case CmResourceTypePort:
			portbase = resource->u.Port.Start;
			pdx->nports = resource->u.Port.Length;
			pdx->mappedport = (resource->Flags & CM_RESOURCE_PORT_IO) == 0;
			gotport = TRUE;
			break;
	
		case CmResourceTypeInterrupt:
			irql = (KIRQL) resource->u.Interrupt.Level;
			vector = resource->u.Interrupt.Vector;
			affinity = resource->u.Interrupt.Affinity;
			mode = (resource->Flags == CM_RESOURCE_INTERRUPT_LATCHED)
				? Latched : LevelSensitive;
			irqshare = resource->ShareDisposition == CmResourceShareShared;
			gotinterrupt = TRUE;
			break;

		default:
			KdPrint((DRIVERNAME " - Unexpected I/O resource type %d\n", resource->Type));
			break;
			}					// switch on resource type
		}						// for each resource

	// Verify that we got all the resources we were expecting

	if (!(TRUE
		&& gotinterrupt
		&& gotport
		))
		{
		KdPrint((DRIVERNAME " - Didn't get expected I/O resources\n"));
		return STATUS_DEVICE_CONFIGURATION_ERROR;
		}

	if (pdx->mappedport)
		{						// map port address for RISC platform
		pdx->portbase = (PUCHAR) MmMapIoSpace(portbase, pdx->nports, MmNonCached);
		if (!pdx->mappedport)
			{
			KdPrint((DRIVERNAME " - Unable to map port range %I64X, length %X\n", portbase, pdx->nports));
			return STATUS_INSUFFICIENT_RESOURCES;
			}
		}						// map port address for RISC platform
	else
		pdx->portbase = (PUCHAR) portbase.QuadPart;

	// TODO Temporarily prevent device from interrupt if that's possible.
	// Enable device for interrupts when IoConnectInterrupt returns

	status = IoConnectInterrupt(&pdx->InterruptObject, (PKSERVICE_ROUTINE) OnInterrupt,
		(PVOID) pdx, NULL, vector, irql, irql, mode, irqshare, affinity, FALSE);
	imr=READ_PORT_UCHAR((UCHAR *)0x21);
	WRITE_PORT_UCHAR((UCHAR *)0x21,imr|0x80);
	control=READ_PORT_UCHAR((UCHAR *)0x37A);
	WRITE_PORT_UCHAR((UCHAR *)0x37A,control|0x10);
	if (!NT_SUCCESS(status))
		{
		KdPrint((DRIVERNAME " - IoConnectInterrupt failed - %X\n", status));
		if (pdx->portbase && pdx->mappedport)
			MmUnmapIoSpace(pdx->portbase, pdx->nports);
		pdx->portbase = NULL;
		return status;
		}

	return STATUS_SUCCESS;
	}							// StartDevice

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

VOID StartIo(IN PDEVICE_OBJECT fdo, IN PIRP Irp)
	{							// StartIo
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(status))
	{
		CompleteRequest(Irp, status, 0);
		return;
		}

	// TODO Initiate read request. Be sure to set the "busy" flag before starting
	// an operation that might generate an interrupt.

	}							// StartIo

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

VOID StopDevice(IN PDEVICE_OBJECT fdo, BOOLEAN oktouch /* = FALSE */)
	{							// StopDevice
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	if (pdx->InterruptObject)
		{						// disconnect interrupt

		// TODO prevent device from generating more interrupts if possible

		IoDisconnectInterrupt(pdx->InterruptObject);
		pdx->InterruptObject = NULL;
		}						// disconnect interrupt

	if (pdx->portbase && pdx->mappedport)
		MmUnmapIoSpace(pdx->portbase, pdx->nports);
	pdx->portbase = NULL;
	}							// StopDevice
