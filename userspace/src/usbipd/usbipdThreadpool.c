#include <winsock2.h>
#include "usbip_setupdi.h"
#include "usbipdThreadpool.h"
#include "usbipd_stub.h"
#include "usbip_forward.h"
#include "usbip_common.h"

static DeviceContainer* DeviceContainerArray = NULL;
static Dictionary* dictionary = NULL;

void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work) {
	HDEVSocketContainer* currentHDEVSocketContainer = (HDEVSocketContainer*)ctx;
	devbuf_t* rbuff = currentHDEVSocketContainer->socketBuffer;
	devbuf_t* wbuff = currentHDEVSocketContainer->HDEVbuffer;
	int res;
	while(TRUE)
	{
		if(!rbuff->in_reading) {
			if((res = read_dev(rbuff, wbuff->swap_req)) < 0)
				return;
			if(res == 0)
				return TRUE;
		}
		if(rbuff->step_reading == 2) {
			Enqueue(rbuff->hdev, wbuff->hdev);
		}
		if(isFirst(rbuff->hdev, wbuff->hdev)) {
			write_devbuf(wbuff, rbuff);
		}
	}
}

BOOL IsContains(devno_t devno, DeviceContainer* existDeviceContainer) {
	DeviceContainer* current = DeviceContainerArray;
	for(;;) {
		if(current == NULL) {
			return FALSE;
		}
		if(current->devno == devno)
		{
			existDeviceContainer = current;
			return TRUE;
		}
		current = current->Next;
	}
}

void AddDeviceToArray(DeviceContainer* newDeviceContainer) {
	if(DeviceContainerArray == NULL) {
		DeviceContainerArray = newDeviceContainer;
		return;
	}
	DeviceContainer* current = DeviceContainerArray;
	for(;;) {
		if(current->Next == NULL) {
			current->Next = newDeviceContainer;
			return;
		}
		current = current->Next;
	}
}

void AddToArray(DeviceContainer* existDeviceContainer, HDEVSocketContainer* newHDEVSocketContainer) {
	HDEVSocketContainer* current = existDeviceContainer->FirstSocketHDEVContainer;
	for(;;) {
		if(current == NULL) {
			current = newHDEVSocketContainer;
		}
		current = current->Next;
	}
}

int CreateNewContainer(HANDLE socketHandle, devno_t devno, HDEVSocketContainer* pHDEVSocketContainer) {
	HANDLE hdevHandle = open_stub_dev(devno);
	if(hdevHandle == INVALID_HANDLE_VALUE) {
		dbg("cannot open devno: %hhu", devno);
		return ERR_ACCESS;
	}
	int ret = CreateContainerByOpenedHDEV(socketHandle, hdevHandle, pHDEVSocketContainer);
	if(ret != 0) {
		CloseHandle(hdevHandle);
	}
	return 0;
}

int CreateContainerByOpenedHDEV(HANDLE socketHandle, HANDLE hdevHandle, HDEVSocketContainer* pHDEVSocketContainer)
{
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(hEvent == NULL) {
		dbg("failed to create event");
		return ERR_GENERAL;
	}

	devbuf_t buffOfSocket;
	if(!init_devbuf(&buffOfSocket, "socket", TRUE, TRUE, socketHandle, hEvent)) {
		CloseHandle(hEvent);
		dbg("failed to initialize %s buffer", "socket");
		return ERR_NETWORK;
	}

	devbuf_t bufferOfhdev;
	if(!init_devbuf(&bufferOfhdev, "hdev", TRUE, TRUE, hdevHandle, hEvent)) {
		CloseHandle(hEvent);
		free(&buffOfSocket);
		dbg("failed to initialize %s buffer", "socket");
		return ERR_GENERAL;
	}
	HDEVSocketContainer* newHDEVSocketContainer = (HDEVSocketContainer*)malloc(sizeof(HDEVSocketContainer));
	if(newHDEVSocketContainer == NULL) {
		CloseHandle(hEvent);
		free(&buffOfSocket);
		free(&bufferOfhdev);
		dbg("fail to malloc to HDEVSocketContainer");
		return ERR_GENERAL;
	}
	newHDEVSocketContainer->HDEVbuffer = &bufferOfhdev;
	newHDEVSocketContainer->HDEVHandle = hdevHandle;
	newHDEVSocketContainer->socketBuffer = &buffOfSocket;
	newHDEVSocketContainer->socketHandle = socketHandle;
	newHDEVSocketContainer->Next = NULL;
	pHDEVSocketContainer = newHDEVSocketContainer;
}

Dictionary* InitNewDictionary(HANDLE hdevHandle, Queue* newQueue) {
	Dictionary* newDictionary = (Dictionary*)malloc(sizeof(Dictionary));
	if(newDictionary == NULL) {
		dbg("fail to malloc memory");
		return NULL;
	}
	newDictionary->hdevHandle = hdevHandle;
	newDictionary->queue = newQueue;
	newDictionary->Next = NULL;
	return newDictionary;
}

void Enqueue(HANDLE hdevHandle, HANDLE socketHandle) {
	Queue* pNewQueue = (Queue*)malloc(sizeof(Queue));
	if(pNewQueue == NULL) {
		dbg("fail to malloc memory");
		return;
	}
	pNewQueue->socketHandle = socketHandle;
	pNewQueue->Next = NULL;

	if(dictionary == NULL) {
		Dictionary* newDictionary = InitNewDictionary(hdevHandle, pNewQueue);
		if(newDictionary == NULL)
		{
			free(pNewQueue);
			return;
		}
		dictionary = newDictionary;
		return;
	}
	Dictionary* current = dictionary;
	for(;;) {
		if(current->hdevHandle == hdevHandle) {
			if(current->queue == NULL) {
				current->queue = &pNewQueue;
				return;
			}
			Queue* currentQueue = current->queue;
			for(;;) {
				if(currentQueue->Next == NULL) {
					currentQueue->Next = &pNewQueue;
					return;
				}
				currentQueue = currentQueue->Next;
			}
		}
		if(current->Next == NULL) {
			Dictionary* newDictionary = InitNewDictionary(hdevHandle, pNewQueue);
			if(newDictionary == NULL)
			{
				free(pNewQueue);
				return;
			}
			current->Next = newDictionary;
			return;
		}
		current = current->Next;
	}
}

BOOL isFirst(HANDLE hdevHandle, HANDLE socketHandle) {
	if(dictionary == NULL) {
		return FALSE;
	}
	Dictionary* current = dictionary;
	for(;;) {
		if(current->hdevHandle == hdevHandle) {
			if(current->queue == NULL) {
				return FALSE;
			}
			if(current->queue->socketHandle == hdevHandle) {
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
		if(current->Next == NULL) {
			return FALSE;
		}
		current = current->Next;
	}
}

void Dequeue(Dictionary* currentDicKeyValuePair) {
	Queue* currentQueue = currentDicKeyValuePair->queue;
	currentDicKeyValuePair->queue = currentDicKeyValuePair->queue->Next;
	free(currentQueue);
}