// DevQueue.h -- Declarations for custom IRP queuing package
// Copyright (C) 1999 by Walter Oney
// All rights reserved

#ifndef DEVQUEUE_H
#define DEVQUEUE_H

typedef VOID (__stdcall *PQNOTIFYFUNC)(PVOID);

typedef struct _DEVQUEUE {
	LIST_ENTRY head;
	KSPIN_LOCK lock;
	PDRIVER_STARTIO StartIo;
	LONG stallcount;
	PIRP CurrentIrp;
	KEVENT evStop;
	PQNOTIFYFUNC notify;
	PVOID notifycontext;
	NTSTATUS abortstatus;
	} DEVQUEUE, *PDEVQUEUE;

VOID NTAPI InitializeQueue(PDEVQUEUE pdq, PDRIVER_STARTIO StartIo);
VOID NTAPI StartPacket(PDEVQUEUE pdq, PDEVICE_OBJECT fdo, PIRP Irp, PDRIVER_CANCEL cancel);
PIRP NTAPI StartNextPacket(PDEVQUEUE pdq, PDEVICE_OBJECT fdo);
VOID NTAPI CancelRequest(PDEVQUEUE pdq, PIRP Irp);
VOID NTAPI CleanupRequests(PDEVQUEUE pdq, PFILE_OBJECT fop, NTSTATUS status);
VOID NTAPI StallRequests(PDEVQUEUE pdq);
VOID NTAPI RestartRequests(PDEVQUEUE pdq, PDEVICE_OBJECT fdo);
PIRP NTAPI GetCurrentIrp(PDEVQUEUE pdq);
BOOLEAN NTAPI CheckBusyAndStall(PDEVQUEUE pdq);
VOID NTAPI WaitForCurrentIrp(PDEVQUEUE pdq);
VOID NTAPI AbortRequests(PDEVQUEUE pdq, NTSTATUS status);
VOID NTAPI AllowRequests(PDEVQUEUE pdq);
NTSTATUS NTAPI AreRequestsBeingAborted(PDEVQUEUE pdq);
NTSTATUS NTAPI StallRequestsAndNotify(PDEVQUEUE pdq, PQNOTIFYFUNC notify, PVOID context);

#endif
