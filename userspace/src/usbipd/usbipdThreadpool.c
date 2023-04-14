#include <winsock2.h>
#include <signal.h>
#include "usbip_setupdi.h"
#include "usbipdThreadpool.h"
#include "usbipd_stub.h"
#include "usbip_forward.h"
#include "usbip_common.h"

static DeviceContainer* DeviceContainerArray = NULL;
static Dictionary* dictionary = NULL;

void signalhandlerPool(int signal)
{
	interrupted = TRUE;
	DeviceContainer* current = DeviceContainerArray;
	for(;;) {
		if(current == NULL) {
			break;
		}
		HDEVSocketContainer* currentContainer = current->FirstSocketHDEVContainer;
		for(;;) {
			if(currentContainer == NULL) {
				break;
			}
			SetEvent(currentContainer->hEvent);
			currentContainer = currentContainer->Next;
		}
		current = current->Next;
	}
}

void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work) {
	dbg("enter");
	pvoid_t* pvoid_data = (pvoid_t*)ctx;
	devno_t devno = pvoid_data->devno;
	HANDLE socketHandle = pvoid_data->socketHandle;

	HANDLE hdevHandle;
	DeviceContainer* existDeviceContainer = NULL;
	BOOL isContainer = IsContains(devno, &existDeviceContainer);
	if(isContainer) {
		hdevHandle = existDeviceContainer->FirstSocketHDEVContainer->HDEVHandle;
	}
	else
	{
		hdevHandle = open_stub_dev(devno);
		if(hdevHandle == INVALID_HANDLE_VALUE) {
			dbg("cannot open devno: %hhu", devno);
			return ERR_ACCESS;
		}
	}
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
	if(!init_devbuf(&bufferOfhdev, "stub", FALSE, FALSE, hdevHandle, hEvent)) {
		CloseHandle(hEvent);
		cleanup_devbuf(&buffOfSocket);
		dbg("failed to initialize %s buffer", "socket");
		return ERR_GENERAL;
	}

	buffOfSocket.peer = &bufferOfhdev;
	bufferOfhdev.peer = &buffOfSocket;

	HDEVSocketContainer* pHDEVSocketContainer = (HDEVSocketContainer*)malloc(sizeof(HDEVSocketContainer));
	if(pHDEVSocketContainer == NULL) {
		CloseHandle(hEvent);
		cleanup_devbuf(&buffOfSocket);
		cleanup_devbuf(&bufferOfhdev);
		dbg("fail to malloc to HDEVSocketContainer");
		return ERR_GENERAL;
	}
	pHDEVSocketContainer->HDEVHandle = hdevHandle;
	pHDEVSocketContainer->socketHandle = socketHandle;
	pHDEVSocketContainer->hEvent = hEvent;
	pHDEVSocketContainer->Next = NULL;
	int ret = AddToArray(devno, pHDEVSocketContainer);
	if(ret != 0) {
		return;
	}
	while(TRUE)
	{
		dbg("1");
			BOOL isFirstOne = isFirst(bufferOfhdev.hdev, buffOfSocket.hdev);
		if(!buffOfSocket.in_reading) {
			int ret = read_dev(&buffOfSocket, bufferOfhdev.swap_req);
			if(ret < 0)
				break;
		}
		else if(buffOfSocket.step_reading != 0) {
			if(isFirstOne == TRUE) {
				dbg("2");

				if(!write_devbuf(&bufferOfhdev, &buffOfSocket)) {
					break;
				}
			}
			else
			{
				dbg("3");
				Enqueue(bufferOfhdev.hdev, buffOfSocket.hdev);
			}
		}
		if(!bufferOfhdev.in_reading && isFirstOne == TRUE) {
			dbg("4");
			int ret = read_dev(&bufferOfhdev, buffOfSocket.swap_req);
			if(ret < 0) {
				break;
			}
		}
		if(!write_devbuf(&buffOfSocket, &bufferOfhdev)) {
			dbg("5");
			break;
		}
		dbg("6");
		if(buffOfSocket.invalid || bufferOfhdev.invalid)
			break;
		dbg("7");
		if(buffOfSocket.in_reading && bufferOfhdev.in_reading &&
			(buffOfSocket.in_writing || BUFREMAIN_C(&bufferOfhdev) == 0) &&
			(bufferOfhdev.in_writing || BUFREMAIN_C(&buffOfSocket) == 0)) {
			WaitForSingleObjectEx(hEvent, INFINITE, TRUE);
			ResetEvent(hEvent);
		}
	}
}

BOOL IsContains(devno_t devno, DeviceContainer** existDeviceContainer) {
	DeviceContainer* current = DeviceContainerArray;
	for(;;) {
		if(current == NULL) {
			return FALSE;
		}
		if(current->devno == devno)
		{
			*existDeviceContainer = current;
			return TRUE;
		}
		current = current->Next;
	}
}

int AddToArray(devno_t devno, HDEVSocketContainer* newHDEVSocketContainer) {

	DeviceContainer** current = &DeviceContainerArray;
	for(;;) {
		if(*current == NULL) {
			DeviceContainer* newDeviceContainer = (DeviceContainer*)malloc(sizeof(DeviceContainer));
			if(newDeviceContainer == NULL) {
				dbg("fail to malloc");
				return ERR_GENERAL;
			}
			newDeviceContainer->devno = devno;
			newDeviceContainer->FirstSocketHDEVContainer = newHDEVSocketContainer;
			newDeviceContainer->Next = NULL;
			*current = newDeviceContainer;
			return 0;
		}
		else if((*current)->devno == devno)
		{
			HDEVSocketContainer** currentContainer = &((*current)->FirstSocketHDEVContainer);
			for(;;) {
				if(*current == NULL) {
					*current = newHDEVSocketContainer;
					return;
				}
				currentContainer = &((*currentContainer)->Next);
			}
		}
		else
		{
			current = &((*current)->Next);
		}
	}

}

static Dictionary* InitNewDictionary(HANDLE hdevHandle, Queue* newQueue) {
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

int Enqueue(HANDLE hdevHandle, HANDLE socketHandle) {
	Queue* pNewQueue = (Queue*)malloc(sizeof(Queue));
	if(pNewQueue == NULL) {
		dbg("fail to malloc memory");
		return ERR_GENERAL;
	}
	pNewQueue->socketHandle = socketHandle;
	pNewQueue->Next = NULL;

	Dictionary** current = &dictionary;
	for(;;) {
		if((*current) == NULL) {
			Dictionary* newDictionary = InitNewDictionary(hdevHandle, pNewQueue);
			if(newDictionary == NULL)
			{
				free(pNewQueue);
				return ERR_GENERAL;
			}
			*current = newDictionary;
			return 0;
		}
		if((*current)->hdevHandle == hdevHandle) {
			Queue** currentQueue = &((*current)->queue);
			for(;;) {
				if(*currentQueue == NULL) {
					*currentQueue = pNewQueue;
					return 0;
				}
				currentQueue = &((*currentQueue)->Next);
			}
		}
		current = &((*current)->Next);
	}
	return 0;
}

BOOL isFirst(HANDLE hdevHandle, HANDLE socketHandle) {
	Dictionary** current = &dictionary;
	for(;;) {
		if((*current) == NULL) {
			return FALSE;
		}
		if((*current)->hdevHandle == hdevHandle) {
			if((*current)->queue == NULL) {
				return FALSE;
			}
			if((*current)->queue->socketHandle == hdevHandle) {
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
		current = &((*current)->Next);
	}
}

void Dequeue(Dictionary* currentDicKeyValuePair) {
	Queue* currentQueue = currentDicKeyValuePair->queue;
	currentDicKeyValuePair->queue = currentDicKeyValuePair->queue->Next;
	free(currentQueue);
}

