#include <winsock2.h>
#include <signal.h>
#include "usbip_setupdi.h"
#include "usbipdThreadpool.h"
#include "usbipd_stub.h"
#include "usbip_forward.h"
#include "usbip_common.h"
#include "usbip_proto.h"

static DeviceContainer* DeviceContainerArray = NULL;
//static Dictionary* dictionary = NULL;

void signalhandlerPool(int signal)
{
	interrupted = TRUE;
	DeviceContainer* current = DeviceContainerArray;
	for(;;) {
		if(current == NULL) {
			break;
		}
		SocketQueue* currentContainer = current->FirstSocketContainer;
		for(;;) {
			if(currentContainer == NULL) {
				break;
			}
			SetEvent(currentContainer->socketBuf->hEventForReader);
			SetEvent(currentContainer->socketBuf->hEventForWriter);
			currentContainer = currentContainer->Next;
		}
		current = current->Next;
	}
}


static BOOL DeciveIsExist(devno_t devno, DeviceContainer** existDeviceContainer) {
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

static int AddToArray(devno_t devno, devbuf_t* socketBuf) {
	SocketQueue* socketQueue = (SocketQueue*)malloc(sizeof(SocketQueue));
	if(socketQueue == NULL) {
		dbg("fail to malloc memory");
		return -1;
	}
	socketQueue->socketBuf = socketBuf;
	socketQueue->Next = NULL;
	DeviceContainer** current = &DeviceContainerArray;
	for(;;) {
		if(*current == NULL) {
			DeviceContainer* newDeviceContainer = (DeviceContainer*)malloc(sizeof(DeviceContainer));
			if(newDeviceContainer == NULL) {
				dbg("fail to malloc");
				return ERR_GENERAL;
			}
			newDeviceContainer->devno = devno;
			newDeviceContainer->HDEVHandle = socketBuf->peer->hdev;
			newDeviceContainer->FirstSocketContainer = socketQueue;
			newDeviceContainer->Next = NULL;
			*current = newDeviceContainer;
			return 0;
		}
		else if((*current)->devno == devno)
		{
			SocketQueue** currentSocketContainer = &((*current)->FirstSocketContainer);
			for(;;) {
				if(*currentSocketContainer == NULL) {
					*currentSocketContainer = socketQueue;
					return 0;
				}
				currentSocketContainer = &((*currentSocketContainer)->Next);
			}
		}
		else
		{
			current = &((*current)->Next);
		}
	}

}
/*
BOOL Contains(devno_t devno, devbuf_t* socketBuf) {
	Dictionary* current = dictionary;
	for(;;) {
		if(current == NULL) {
			return FALSE;
		}
		if(current->devno == devno) {
			Queue* currentQueue = current->queue;
			for(;;) {
				if(currentQueue == NULL) {

					return FALSE;
				}
				else if(currentQueue->socketBuf->hdev == socketBuf->hdev)
				{
					return TRUE;
				}
				currentQueue = currentQueue->Next;
			}
		}
		current = current->Next;
	}
}
*/



/*
int Enqueue(devno_t devno, devbuf_t* socketBuf) {
	Queue* pNewQueue = (Queue*)malloc(sizeof(Queue));
	if(pNewQueue == NULL) {
		dbg("fail to malloc memory");
		return ERR_GENERAL;
	}
	pNewQueue->socketBuf = socketBuf;
	pNewQueue->Next = NULL;

	Dictionary** current = &dictionary;
	for(;;) {
		if((*current) == NULL) {
			Dictionary* newDictionary = (Dictionary*)malloc(sizeof(Dictionary));
			if(newDictionary == NULL) {
				free(pNewQueue);
				dbg("fail to malloc memory");
				return ERR_GENERAL;
			}
			newDictionary->devno = devno;
			newDictionary->queue = pNewQueue;
			newDictionary->Next = NULL;

			*current = newDictionary;
			return 0;
		}
		if((*current)->devno == devno) {
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

Queue* Dequeue(devno_t devno) {
	Dictionary** dic = &dictionary;
	for(;;) {
		if(*dic == NULL) {
			return NULL;
		}
		else if((*dic)->devno == devno)
		{
			Queue* ret = (*dic)->queue;
			if(ret != NULL) {
				(*dic)->queue = ret->Next;
			}
			return ret;
		}
		else
		{
			dic = &((*dic)->Next);
		}
	}
}
*/

SocketQueue* GetFirstOne(devno_t devno) {
	DeviceContainer** dic = &DeviceContainerArray;
	for(;;) {
		if(*dic == NULL) {
			return NULL;
		}
		else if((*dic)->devno == devno)
		{
			SocketQueue* ret = (*dic)->FirstSocketContainer;
			return ret;
		}
		else
		{
			dic = &((*dic)->Next);
		}
	}
}

static int DealWithBufpAndBufc(devbuf_t* rbuff) {
	if(rbuff->bufc->offc == rbuff->bufc->offp && rbuff->bufc->Next != NULL && rbuff->bufc->Next->step_reading == 3) {
		dbg("free%sbufc", rbuff->desc);
		buffer* oldBuf = rbuff->bufc;
		rbuff->bufc = rbuff->bufc->Next;
		freeBuffer(oldBuf);
		SetEvent(rbuff->peer->hEventForWriter);
	}
	if(rbuff->bufp->step_reading == 3) {
		dbg("create%sbufp", rbuff->desc);
		buffer* newbuf = createNewBuffer();
		if(newbuf == NULL) {
			return -1;
		}
		rbuff->bufp->Next = newbuf;
		rbuff->bufp = rbuff->bufp->Next;
		SetEvent(rbuff->hEventForReader);
	}
	return 0;
}

static  BOOL
read_write_dev(devbuf_t* rbuff, devbuf_t* wbuff, BOOL readOnly) {
	int	res;

	if(readOnly) {
		res = read_dev(rbuff, wbuff->swap_req);
		if(res < 0)
			return FALSE;
	}
	if(!readOnly) {
		if(!write_devbuf(wbuff, rbuff))
			return FALSE;
	}
	return TRUE;
}

void CALLBACK ThreadToRefreshBuffer(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work) {
	devbuf_t* socketBuffer = (devbuf_t*)ctx;
	devbuf_t* hdevBuffer = socketBuffer->peer;
	while(!interrupted)
	{
		if(DealWithBufpAndBufc(socketBuffer) == -1) {
			break;
		}
		if(DealWithBufpAndBufc(hdevBuffer) == -1) {
			break;
		}
	}
}

void CALLBACK ThreadForReader(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work) {
	devbuf_t* socketBuffer = (devbuf_t*)ctx;
	devbuf_t* hdevBuffer = socketBuffer->peer;
	HANDLE hevent = socketBuffer->hEventForReader;
	while(!interrupted)
	{
		if(read_dev(socketBuffer, hdevBuffer->swap_req) < 0)
			break;
		if(socketBuffer->invalid)
			break;
		if(socketBuffer->in_reading || socketBuffer->bufp->step_reading == 3)
		{
			WaitForSingleObjectEx(hevent, INFINITE, TRUE);
			ResetEvent(hevent);
		}
	}
}



void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work)
{
	dbg("stub forwarding started");
	pvoid_t* pvoid_data = (pvoid_t*)ctx;
	devno_t devno = pvoid_data->devno;
	HANDLE socketHandle = pvoid_data->socketHandle;

	HANDLE hdevHandle;
	DeviceContainer* existDeviceContainer;

	HANDLE hEventProducerForSocket = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE hEventConsumerForSocket = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE hEventProducerForHDEV = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE hEventConsumerForHDEV = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(hEventProducerForSocket == NULL ||
		hEventConsumerForSocket == NULL ||
		hEventProducerForHDEV == NULL ||
		hEventConsumerForHDEV == NULL) {
		dbg("failed to create event");
		return;
	}

	BOOL isContainer = DeciveIsExist(devno, &existDeviceContainer);
	if(isContainer) {
		hdevHandle = existDeviceContainer->HDEVHandle;
	}
	else
	{
		hdevHandle = open_stub_dev(devno);
		if(hdevHandle == INVALID_HANDLE_VALUE) {
			dbg("cannot open devno: %hhu", devno);
			return;
		}
	}

	devbuf_t* buffOfSocket;
	if(!init_devbufStatic(&buffOfSocket, "socket", TRUE, TRUE, socketHandle, hEventConsumerForSocket, hEventProducerForSocket)) {
		CloseHandle(hEventConsumerForSocket);
		CloseHandle(hEventProducerForSocket);
		dbg("failed to initialize %s buffer", "socket");
		return;
	}


	devbuf_t* bufferOfhdev;
	if(!init_devbufStatic(&bufferOfhdev, "stub", FALSE, FALSE, hdevHandle, hEventConsumerForHDEV, hEventProducerForHDEV)) {
		CloseHandle(hEventConsumerForHDEV);
		CloseHandle(hEventProducerForHDEV);
		cleanup_devbuf(bufferOfhdev);
		dbg("failed to initialize %s buffer", "socket");
		return;
	}


	buffOfSocket->peer = bufferOfhdev;
	bufferOfhdev->peer = buffOfSocket;

	int ret = AddToArray(devno, buffOfSocket);
	if(ret != 0) {
		/*	CloseHandle(hEventToProducer);
			CloseHandle(hEventToConsumer);*/
		cleanup_devbuf(buffOfSocket);
		cleanup_devbuf(bufferOfhdev);
		return;
	}
	PTP_WORK bufferRefreshThread = CreateThreadpoolWork(ThreadToRefreshBuffer, buffOfSocket, NULL);
	if(bufferRefreshThread == NULL) {
		dbg("failed to create thread pool work: error: %lx", GetLastError());
		free(hdevHandle);
		return;
	}
	SubmitThreadpoolWork(bufferRefreshThread);

	PTP_WORK consumerWork = CreateThreadpoolWork(ThreadForConsumerRequest, &devno, NULL);
	if(consumerWork == NULL) {
		dbg("failed to create thread pool work: error: %lx", GetLastError());
		free(hdevHandle);
		return;
	}
	SubmitThreadpoolWork(consumerWork);
	PTP_WORK readSocketWork = CreateThreadpoolWork(ThreadForReader, buffOfSocket, NULL);
	if(readSocketWork == NULL) {
		dbg("failed to create thread pool work: error: %lx", GetLastError());
		free(hdevHandle);
		return;
	}
	SubmitThreadpoolWork(readSocketWork);


	HANDLE hevent = buffOfSocket->hEventForWriter;
	while(!interrupted) {
		if(!write_devbuf(buffOfSocket, bufferOfhdev))
			break;

		if(buffOfSocket->in_writing ||
			bufferOfhdev->bufc->offc == bufferOfhdev->bufc->offp ||
			bufferOfhdev->bufc->step_reading != 3)
		{
			WaitForSingleObjectEx(hevent, INFINITE, TRUE);
			ResetEvent(hevent);
		}
	}

	if(interrupted) {
		info("CTRL-C received\n");
	}
	signal(SIGINT, SIG_DFL);

	if(buffOfSocket->in_reading)
		CancelIoEx(socketHandle, &buffOfSocket->ovs[0]);
	if(bufferOfhdev->in_reading)
		CancelIoEx(hdevHandle, &bufferOfhdev->ovs[0]);


	CloseThreadpoolWork(work);

	dbg("stub forwarding stopped");
	dbg("break");
}


