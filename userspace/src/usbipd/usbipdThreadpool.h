#pragma

#include <winsock2.h>
#include "usbip_setupdi.h"
#include "usbip_forward.h"
#ifndef _USBIP_THREADPOOL_H_
#define _USBIP_THREADPOOL_H_

typedef struct HDEVSocket {
	HANDLE HDEVHandle;
	devbuf_t* HDEVbuffer;
	HANDLE socketHandle;
	devbuf_t* socketBuffer;
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

extern void Enqueue(HANDLE hdevHandle, HANDLE socketHandle);

extern BOOL isFirst(HANDLE hdevHandle, HANDLE socketHandle);

extern void Dequeue(HANDLE hdevHandle);

extern BOOL IsContains(devno_t devno, DeviceContainer* existDeviceContainer);

extern void AddToArray(DeviceContainer* existDeviceContainer, HDEVSocketContainer* newHDEVSocketContainer);

extern void AddDeviceToArray(DeviceContainer* newDeviceContainer);

extern int CreateNewContainer(HANDLE socketHandle, devno_t devno, HDEVSocketContainer* pHDEVSocketContainer);

extern int CreateContainerByOpenedHDEV(HANDLE socketHandle, HANDLE hdevHandle, HDEVSocketContainer* pHDEVSocketContainer);

extern void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

extern void CALLBACK ThreadToCustomerRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work);

#endif
