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

typedef struct HDEVSocket {
	HANDLE HDEVHandle;
	HANDLE socketHandle;
	HANDLE hEvent;
	struct HDEVSocket* Next;
} HDEVSocketContainer;

typedef struct DefineSocketListOfDevice
{
	devno_t devno;
	HDEVSocketContainer* FirstSocketHDEVContainer;
	struct DefineSocketListOfDevice* Next;
} DeviceContainer;

typedef struct QueueList {
	HANDLE socketHandle;
	struct QueueList* Next;
} Queue;

typedef struct dictionaryOfQueue {
	Queue* queue;
	HANDLE hdevHandle;
	struct dictionaryOfQueue* Next;
} Dictionary;

extern int Enqueue(HANDLE hdevHandle, HANDLE socketHandle);

extern BOOL isFirst(HANDLE hdevHandle, HANDLE socketHandle);

extern void Dequeue(Dictionary* currentDicKeyValuePair);

extern BOOL IsContains(devno_t devno, DeviceContainer** existDeviceContainer);



extern void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void signalhandlerPool(int signal);
#endif
