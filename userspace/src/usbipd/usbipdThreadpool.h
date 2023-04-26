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
	HANDLE hEventForProducer;
	HANDLE hEventForConsumer;
	HANDLE socketHandle;
	struct t1* Next;
} SocketContainer;

typedef struct q1 {
	devbuf_t* socketBuf;
	struct q1* Next;
} SocketQueue;

typedef struct t2
{
	devno_t devno;
	HANDLE HDEVHandle;
	SocketQueue* FirstSocketContainer;
	struct t2* Next;
} DeviceContainer;


//typedef struct q2 {
//	devno_t devno;
//	Queue* queue;
//	struct q2* Next;
//} Dictionary;

typedef struct deviceConsumerSign {
	devno_t devno;
	struct deviceConsumerSign* Next;
} DeviceForConsumerThread;

extern SocketQueue* GetFirstOne(devno_t devno);
extern void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void CALLBACK ThreadForConsumerRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void signalhandlerPool(int signal);


#endif
