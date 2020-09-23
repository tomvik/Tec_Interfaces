// DevQueue.cpp -- Custom IRP queuing support
// Copyright (C) 1999 by Walter Oney
// All rights reserved

#include "stddcls.h"
#include "DevQueue.h"

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI AbortRequests(PDEVQUEUE pdq, NTSTATUS status)
	{							// AbortRequests
	}							// AbortRequests

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI AllowRequests(PDEVQUEUE pdq)
	{							// AllowRequests
	}							// AllowRequests

///////////////////////////////////////////////////////////////////////////////

NTSTATUS AreRequestsBeingAborted(PDEVQUEUE pdq)
	{							// AreRequestsBeingAborted
	return pdq->abortstatus;
	}							// AreRequestsBeingAborted

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI CancelRequest(PDEVQUEUE pdq, PIRP Irp)
	{							// CancelRequest
	}							// CancelRequest

///////////////////////////////////////////////////////////////////////////////

BOOLEAN NTAPI CheckBusyAndStall(PDEVQUEUE pdq)
	{							// CheckBusyAndStall
	KIRQL oldirql;
	KeAcquireSpinLock(&pdq->lock, &oldirql);
	BOOLEAN busy = pdq->CurrentIrp != NULL;
	if (!busy)
		InterlockedIncrement(&pdq->stallcount);
	KeReleaseSpinLock(&pdq->lock, oldirql);
	return busy;
	}							// CheckBusyAndStall

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI CleanupRequests(PDEVQUEUE pdq, PFILE_OBJECT fop, NTSTATUS status)
	{							// CleanupRequests
	}							// CleanupRequests

///////////////////////////////////////////////////////////////////////////////

PIRP NTAPI GetCurrentIrp(PDEVQUEUE pdq)
	{							// GetCurrentIrp
	return pdq->CurrentIrp;
	}							// GetCurrentIrp

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI InitializeQueue(PDEVQUEUE pdq, PDRIVER_STARTIO StartIo)
	{							// InitializeQueue
	InitializeListHead(&pdq->head);
	KeInitializeSpinLock(&pdq->lock);
	pdq->StartIo = StartIo;
	pdq->stallcount = 1;
	pdq->CurrentIrp = NULL;
	KeInitializeEvent(&pdq->evStop, NotificationEvent, FALSE);
	pdq->abortstatus = (NTSTATUS) 0;
	pdq->notify = NULL;
	pdq->notifycontext = 0;
	}							// InitializeQueue

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI RestartRequests(PDEVQUEUE pdq, PDEVICE_OBJECT fdo)
	{							// RestartRequests
	if (InterlockedDecrement(&pdq->stallcount) > 0)
		return;
	ASSERT(pdq->stallcount == 0); // guard against excessive restart calls
	StartNextPacket(pdq, fdo);
	}							// RestartRequests

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI StallRequests(PDEVQUEUE pdq)
	{							// StallRequests
	InterlockedIncrement(&pdq->stallcount);
	}							// StallRequests

///////////////////////////////////////////////////////////////////////////////

NTSTATUS NTAPI StallRequestsAndNotify(PDEVQUEUE pdq, PQNOTIFYFUNC notify, PVOID context)
	{							// StallRequestsAndNotify
	NTSTATUS status;
	KIRQL oldirql;
	KeAcquireSpinLock(&pdq->lock, &oldirql);

	if (pdq->notify)
		status = STATUS_INVALID_DEVICE_REQUEST;
	else
		{						// valid request
		InterlockedIncrement(&pdq->stallcount);
		if (pdq->CurrentIrp)
			{					// device is busy
			pdq->notify = notify;
			pdq->notifycontext = context;
			status = STATUS_PENDING;
			}					// device is busy
		else
			status = STATUS_SUCCESS; // device is idle
		}						// valid request

	KeReleaseSpinLock(&pdq->lock, oldirql);
	return status;
	}							// StallRequestsAndNotify

///////////////////////////////////////////////////////////////////////////////

PIRP NTAPI StartNextPacket(PDEVQUEUE pdq, PDEVICE_OBJECT fdo)
	{							// StartNextPacket
	KIRQL oldirql;
	KeAcquireSpinLock(&pdq->lock, &oldirql);

	// Nullify the current IRP pointer after remembering the current one.
	// We'll return the current IRP pointer as our return value so that
	// a DPC routine has a way to know whether an active request got
	// aborted.

	PIRP CurrentIrp = (PIRP) InterlockedExchangePointer(&pdq->CurrentIrp, NULL);

	// If we just finished processing a request, set the event on which
	// WaitForCurrentIrp may be waiting in some other thread.

	if (CurrentIrp)
		KeSetEvent(&pdq->evStop, 0, FALSE);

	// If someone is waiting for notification that this IRP has finished,
	// we'll provide the notification after we release the spin lock. We shouldn't
	// find the queue unstalled if there is a notification routine in place, by
	// the way.

	PQNOTIFYFUNC notify = pdq->notify;
	PVOID notifycontext = pdq->notifycontext;
	pdq->notify = NULL;

	// Start the next IRP

	while (!pdq->stallcount && !pdq->abortstatus && !IsListEmpty(&pdq->head))
		{						// start next packet
		PLIST_ENTRY next = RemoveHeadList(&pdq->head);
		PIRP Irp = CONTAINING_RECORD(next, IRP, Tail.Overlay.ListEntry);

		// (After Hanrahan & Peretz in part) Nullify the cancel pointer in this IRP. If it was
		// already NULL, someone is trying to cancel this IRP right now. Reinitialize
		// the link pointers so the cancel routine's call to RemoveEntryList won't
		// do anything harmful and look for another IRP. The cancel routine will
		// take over as soon as we release the spin lock

		if (!IoSetCancelRoutine(Irp, NULL))
			{					// IRP being cancelled right now
			ASSERT(Irp->Cancel);	// else CancelRoutine shouldn't be NULL!
			InitializeListHead(&Irp->Tail.Overlay.ListEntry);
			continue;			// with "start next packet"
			}					// IRP being cancelled right now

		pdq->CurrentIrp = Irp;
		KeReleaseSpinLockFromDpcLevel(&pdq->lock);
		(*pdq->StartIo)(fdo, Irp);
		KeLowerIrql(oldirql);
		return CurrentIrp;
		}						// start next packet

	KeReleaseSpinLock(&pdq->lock, oldirql);

	if (notify)
		(*notify)(notifycontext);

	return CurrentIrp;
	}							// StartNextPacket

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI StartPacket(PDEVQUEUE pdq, PDEVICE_OBJECT fdo, PIRP Irp, PDRIVER_CANCEL cancel)
	{							// StartPacket
	KIRQL oldirql;
	KeAcquireSpinLock(&pdq->lock, &oldirql);

	ASSERT(Irp->CancelRoutine == NULL); // maybe left over from a higher level?

	// If the device has been removed by surprise, complete IRP immediately. Do not
	// pass GO. Do not collect $200.

	NTSTATUS abortstatus = pdq->abortstatus;
	if (abortstatus)
		{						// aborting all requests now
		KeReleaseSpinLock(&pdq->lock, oldirql);
		Irp->IoStatus.Status = abortstatus;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		}						// aborting all requests now

	// If the device is busy with another request, or if the queue has
	// been stalled due to some PnP or power event, just put the new IRP
	// onto the queue and set a cancel routine pointer.

	else if (pdq->CurrentIrp || pdq->stallcount)
		{						// queue this irp

		// (After Peretz) See if this IRP was cancelled before it got to us. If so,
		// make sure either we or the cancel routine completes it

		IoSetCancelRoutine(Irp, cancel);
		if (Irp->Cancel && IoSetCancelRoutine(Irp, NULL))
			{					// IRP has already been cancelled
			KeReleaseSpinLock(&pdq->lock, oldirql);
			Irp->IoStatus.Status = STATUS_CANCELLED;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			}					// IRP has already been cancelled
		else
			{					// queue IRP
			InsertTailList(&pdq->head, &Irp->Tail.Overlay.ListEntry);
			KeReleaseSpinLock(&pdq->lock, oldirql);
			}					// queue IRP
		}						// queue this irp

	// If the device is idle and not stalled, pass the IRP to the StartIo
	// routine associated with this queue

	else
		{						// start this irp
		pdq->CurrentIrp = Irp;
		KeReleaseSpinLock(&pdq->lock, DISPATCH_LEVEL);
		(*pdq->StartIo)(fdo, Irp);
		KeLowerIrql(oldirql);
		}						// start this irp
	}							// StartPacket

///////////////////////////////////////////////////////////////////////////////

VOID NTAPI WaitForCurrentIrp(PDEVQUEUE pdq)
	{							// WaitForCurrentIrp

	// First reset the event that StartNextPacket sets each time.

	KeClearEvent(&pdq->evStop);

	// Under protection of our spin lock, check to see if there's a current IRP.
	// Since whoever called us should also have stalled requests, no-one can sneak
	// in after we release the spin lock and start a new request behind our back.

	ASSERT(pdq->stallcount != 0);	// should be stalled now!
	
	KIRQL oldirql;
	KeAcquireSpinLock(&pdq->lock, &oldirql);
	BOOLEAN mustwait = pdq->CurrentIrp != NULL;
	KeReleaseSpinLock(&pdq->lock, oldirql);

	if (mustwait)
		KeWaitForSingleObject(&pdq->evStop, Executive, KernelMode, FALSE, NULL);
	}							// WaitForCurrentIrp
