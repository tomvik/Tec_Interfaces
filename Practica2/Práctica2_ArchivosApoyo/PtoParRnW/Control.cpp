// Control.cpp -- IOCTL handlers for PtoPar driver
// Copyright (C) 1999 by Walter Oney
// All rights reserved

#include "stddcls.h"
#include "driver.h"
#include "ioctls.h"

NTSTATUS CacheControlRequest(PDEVICE_EXTENSION pdx, PIRP Irp, PIRP* pIrp);
VOID OnCancelPendingIoctl(PDEVICE_OBJECT fdo, PIRP Irp);
NTSTATUS OnCompletePendingIoctl(PDEVICE_OBJECT junk, PIRP Irp, PDEVICE_EXTENSION pdx);
VOID CleanupControlRequests(PDEVICE_EXTENSION pdx, NTSTATUS status, PFILE_OBJECT fop);

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

NTSTATUS DispatchControl(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// DispatchControl
	PAGED_CODE();
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;

	NTSTATUS status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(status))
		return CompleteRequest(Irp, status, 0);
	ULONG info = 0;

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG cbin = stack->Parameters.DeviceIoControl.InputBufferLength;
	ULONG cbout = stack->Parameters.DeviceIoControl.OutputBufferLength;
	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

	switch (code)
		{						// process request

	case IOCTL_GET_VERSION_BUFFERED:				// code == 0x800
		{						// IOCTL_GET_VERSION_BUFFERED

		// TODO insert code here to handle this IOCTL, which uses METHOD_BUFFERED
		if (cbout < sizeof(ULONG)) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		PULONG pversion = (PULONG) Irp->AssociatedIrp.SystemBuffer;
		*pversion = 0x00020003;	//v2.3
		info = sizeof(ULONG);
		break;
		}						// IOCTL_GET_VERSION_BUFFERED

	case IOCTL_GET_VERSION_DIRECT:				// code == 0x801
		{						// IOCTL_GET_VERSION_DIRECT

		// TODO insert code here to handle this IOCTL, which uses METHOD_OUT_DIRECT
		if (cbout < sizeof(ULONG)) {
			status = STATUS_INVALID_BUFFER_SIZE;
			break;
		}
		PULONG pversion = (PULONG) MmGetSystemAddressForMdl(Irp->MdlAddress);
		*pversion = 0x00020001;	//v2.1
		info = sizeof(ULONG);
		break;
		}						// IOCTL_GET_VERSION_DIRECT

	case IOCTL_GET_VERSION_NEITHER:				// code == 0x802
		{						// IOCTL_GET_VERSION_NEITHER

		// TODO insert code here to handle this IOCTL, which uses METHOD_NEITHER

		break;
		}						// IOCTL_GET_VERSION_NEITHER

	case IOCTL_WAIT_NOTIFY:				// code == 0x803
		{						// IOCTL_WAIT_NOTIFY
		KdPrint((DRIVERNAME " ... ITESM:PIC, DispatchControl(WAIT_NOTIFY)\n"));
		if (cbout < sizeof(ULONG))
			status = STATUS_INVALID_PARAMETER;
		else
			//status = CacheControlRequest(pdx, Irp, &pdx->NotifyIrp);
			pdx->NotifyIrp = Irp;
            IoMarkIrpPending(Irp);
			IoSetCancelRoutine(Irp, OnCancelPendingIoctl);
			IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
            status = STATUS_PENDING;
		break;
		}						// IOCTL_WAIT_NOTIFY

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;

		}						// process request

	IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
	return status == STATUS_PENDING ? status : CompleteRequest(Irp, status, info);
	}							// DispatchControl

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

VOID AbortPendingIoctls(PDEVICE_EXTENSION pdx, NTSTATUS status)
	{							// AbortPendingIoctls
	}							// AbortPendingIoctls

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

NTSTATUS CacheControlRequest(PDEVICE_EXTENSION pdx, PIRP Irp, PIRP* pIrp)
	{							// CacheControlRequest
	return STATUS_SUCCESS;
	}							// CacheControlRequest

///////////////////////////////////////////////////////////////////////////////

#pragma PAGEDCODE

VOID CleanupControlRequests(PDEVICE_EXTENSION pdx, NTSTATUS status, PFILE_OBJECT fop)
	{							// CleanupControlRequests
	}							// CleanupControlRequests

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

VOID OnCancelPendingIoctl(PDEVICE_OBJECT fdo, PIRP Irp)
	{							// OnCancelPendingIoctl
	KIRQL oldirql = Irp->CancelIrql;
	IoReleaseCancelSpinLock(DISPATCH_LEVEL);
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fdo->DeviceExtension;
	//__asm {int 3};
	// Complete the IRP

	Irp->IoStatus.Status = STATUS_CANCELLED;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	pdx->NotifyIrp=NULL;
	}							// OnCancelPendingIoctl

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

NTSTATUS OnCompletePendingIoctl(PDEVICE_OBJECT junk, PIRP Irp, PDEVICE_EXTENSION pdx)
	{							// OnCompletePendingIoctl
	return STATUS_SUCCESS;
	}							// OnCompletePendingIoctl

///////////////////////////////////////////////////////////////////////////////

#pragma LOCKEDCODE

PIRP UncacheControlRequest(PDEVICE_EXTENSION pdx, PIRP* pIrp)
	{							// UncacheControlRequest
	return(*pIrp);
	}							// UncacheControlRequest

#pragma LOCKEDCODE				// force inline functions into nonpaged code

