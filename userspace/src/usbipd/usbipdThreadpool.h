#pragma

#include <winsock2.h>
#include "usbip_setupdi.h"
#include "usbip_forward.h"
#ifndef _USBIP_THREADPOOL_H_
#define _USBIP_THREADPOOL_H_

typedef struct {
	devno_t devno;
	HANDLE socketHandle;
} pvoid_t;

typedef struct t1 {
	HANDLE hEvent;
	HANDLE socketHandle;
	struct t1* Next;
} SocketContainer;

typedef struct t2
{
	devno_t devno;
	HANDLE HDEVHandle;
	SocketContainer* FirstSocketContainer;
	struct t2* Next;
} DeviceContainer;

typedef struct q1 {
	//data is from device when True 
	BOOL fromDevice;
	devbuf_t* socketBuf;
	devbuf_t* hdevBuf;
	struct q1* Next;
} Queue;

typedef struct q2 {
	devno_t devno;
	Queue* queue;
	struct q2* Next;
} Dictionary;

extern BOOL DeciveIsExist(devno_t devno, DeviceContainer** existDeviceContainer);

extern int AddToArray(devno_t devno, HANDLE HDEVHandle, HANDLE socketHandle, HANDLE hEvent);

extern int Enqueue(devno_t devno, BOOL toDevice, devbuf_t* socketBuf, devbuf_t* hdevBuf);

extern Queue* Dequeue(devno_t devno);
extern BOOL Contains(devno_t devno, devbuf_t* socketBuf);

extern void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void CALLBACK ThreadForConsumerRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void signalhandlerPool(int signal);
#endif
