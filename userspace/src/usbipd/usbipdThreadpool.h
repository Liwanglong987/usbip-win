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

typedef struct t2
{
	devno_t devno;
	HANDLE HDEVHandle;
	SocketContainer* FirstSocketContainer;
	struct t2* Next;
} DeviceContainer;

typedef struct q1 {
	devbuf_t* socketBuf;
	struct q1* Next;
} Queue;

typedef struct q2 {
	devno_t devno;
	Queue* queue;
	struct q2* Next;
} Dictionary;

typedef struct deviceConsumerSign {
	devno_t devno;
	struct deviceConsumerSign* Next;
} DeviceForConsumerThread;


extern BOOL DeciveIsExist(devno_t devno, DeviceContainer** existDeviceContainer);

extern int AddToArray(devno_t devno, HANDLE HDEVHandle, HANDLE socketHandle, HANDLE hEventForProducer, HANDLE hEventForConsumer);

extern int Enqueue(devno_t devno, devbuf_t* socketBuf);

extern Queue* Dequeue(devno_t devno);

extern BOOL Contains(devno_t devno, devbuf_t* socketBuf);

extern void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void CALLBACK ThreadForConsumerRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void signalhandlerPool(int signal);

extern BOOL init_devbufStatic(devbuf_t** buff, const char* desc, BOOL is_req, BOOL swap_req, HANDLE hdev, HANDLE hEventForConsumer, HANDLE hEventForProducer);
#endif
