#include <winsock2.h>
#include <signal.h>
#include "usbip_setupdi.h"
#include "usbipdThreadpool.h"
#include "usbipd_stub.h"
#include "usbip_forward.h"
#include "usbip_common.h"
#include "usbip_proto.h"

static DeviceContainer* DeviceContainerArray = NULL;
static Dictionary* dictionary = NULL;
static DeviceForConsumerThread* consumerThreadList = NULL;
void signalhandlerPool(int signal)
{
	interrupted = TRUE;
	DeviceContainer* current = DeviceContainerArray;
	for(;;) {
		if(current == NULL) {
			break;
		}
		SocketContainer* currentContainer = current->FirstSocketContainer;
		for(;;) {
			if(currentContainer == NULL) {
				break;
			}
			SetEvent(currentContainer->hEventForConsumer);
			SetEvent(currentContainer->hEventForProducer);
			currentContainer = currentContainer->Next;
		}
		current = current->Next;
	}
}


BOOL DeciveIsExist(devno_t devno, DeviceContainer** existDeviceContainer) {
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

int AddToArray(devno_t devno, HANDLE HDEVHandle, HANDLE socketHandle, HANDLE hEventForProducer, HANDLE hEventForConsumer) {
	SocketContainer* socketContainer = (SocketContainer*)malloc(sizeof(SocketContainer));
	if(socketContainer == NULL) {
		dbg("fail to mallo");
		return ERR_GENERAL;
	}
	socketContainer->socketHandle = socketHandle;
	socketContainer->hEventForConsumer = hEventForConsumer;
	socketContainer->hEventForProducer = hEventForProducer;
	socketContainer->Next = NULL;

	DeviceContainer** current = &DeviceContainerArray;
	for(;;) {
		if(*current == NULL) {
			DeviceContainer* newDeviceContainer = (DeviceContainer*)malloc(sizeof(DeviceContainer));
			if(newDeviceContainer == NULL) {
				dbg("fail to malloc");
				return ERR_GENERAL;
			}
			newDeviceContainer->devno = devno;
			newDeviceContainer->HDEVHandle = HDEVHandle;
			newDeviceContainer->FirstSocketContainer = socketContainer;
			newDeviceContainer->Next = NULL;
			*current = newDeviceContainer;
			return 0;
		}
		else if((*current)->devno == devno)
		{
			SocketContainer** currentSocketContainer = &((*current)->FirstSocketContainer);
			for(;;) {
				if(*currentSocketContainer == NULL) {
					*currentSocketContainer = socketContainer;
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

//if same device has added,return false;if not added but fail to malloc, return false; if add successfully, return true.
static BOOL AddNewConsumerThreadForDevice(devno_t devno) {
	DeviceForConsumerThread** current = &consumerThreadList;
	for(;;) {
		if(*current == NULL) {
			DeviceForConsumerThread* newThread = (DeviceForConsumerThread*)malloc(sizeof(DeviceForConsumerThread));
			if(newThread == NULL) {
				dbg("fail to malloc");
				return FALSE;
			}
			newThread->devno = devno;
			newThread->Next = NULL;
			*current = newThread;
			return TRUE;
		}
		if((*current)->devno == devno) {
			dbg("has exist");
			return FALSE;
		}
		current = &((*current)->Next);
	}
}

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

Queue* GetFirstOne(devno_t devno) {
	Dictionary** dic = &dictionary;
	for(;;) {
		if(*dic == NULL) {
			return NULL;
		}
		else if((*dic)->devno == devno)
		{
			Queue* ret = (*dic)->queue;
			return ret;
		}
		else
		{
			dic = &((*dic)->Next);
		}
	}
}

void CALLBACK ThreadForConsumerRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work)
{
	devno_t* pDevno = (devno_t*)ctx;
	if(AddNewConsumerThreadForDevice(*pDevno) == FALSE) {
		return;
	}
	devno_t devno = * pDevno;
	Queue* firstQueue = GetFirstOne(devno);
	while(!interrupted)
	{
		if(firstQueue == NULL) {
			firstQueue = GetFirstOne(devno);
			if(firstQueue != NULL) {
				dbg("getnewone");
			}
			continue;
		}
		else
		{
			devbuf_t* socketBuf = firstQueue->socketBuf;
			devbuf_t* hdevBuf = socketBuf->peer;
			HANDLE hEvent = hdevBuf->hEventForConsumer;

			if(socketBuf->bufc->step_reading == 3 && socketBuf->bufc->offp > socketBuf->bufc->offc && !hdevBuf->in_writing) {
				if(!WriteFileEx(hdevBuf->hdev, socketBuf->bufc->buff + socketBuf->bufc->offc, socketBuf->bufc->offp - socketBuf->bufc->offc, &wbuff->ovs[1], write_completion)) {
					dbg("failed to write sock: err: 0x%lx", GetLastError());
					return FALSE;
				}
				wbuff->in_writing = TRUE;
			}

			if(socketBuf->bufp->requireResponse == FALSE) {
				if(BUFREMAIN_C(socketBuf) == 0) {
					dbg("108");
					Queue* toFree = Dequeue(devno);
					if(toFree != NULL) {
						dbg("109");
						free(toFree);
					}
					firstQueue = NULL;
					continue;
				}
			}
			else
			{
				dbg("110");
				if(!hdevBuf->in_reading) {
					dbg("111");
					int ret = read_dev(hdevBuf, socketBuf->swap_req);
					if(ret < 0) {
						dbg("read data from device fail once");
						break;
					}
				}

				if(hdevBuf->finishRead == TRUE) {
					dbg("112");
					Queue* toFree = Dequeue(devno);
					if(toFree != NULL) {
						dbg("115");
						free(toFree);
					}
					firstQueue = NULL;
					continue;
				}

			}

			if(hdevBuf->in_reading || hdevBuf->in_writing) {
				WaitForSingleObjectEx(hEvent, INFINITE, TRUE);
				ResetEvent(hEvent);
			}

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

	HANDLE hEventToProducer = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE hEventToConsumer = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(hEventToProducer == NULL || hEventToConsumer == NULL) {
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
	if(!init_devbufStatic(&buffOfSocket, "socket", TRUE, TRUE, socketHandle, hEventToConsumer, hEventToProducer)) {
		CloseHandle(hEventToConsumer);
		CloseHandle(hEventToProducer);
		dbg("failed to initialize %s buffer", "socket");
		return;
	}


	devbuf_t* bufferOfhdev;
	if(!init_devbufStatic(&bufferOfhdev, "hdev", FALSE, FALSE, hdevHandle, hEventToConsumer, hEventToProducer)) {
		CloseHandle(hEventToConsumer);
		CloseHandle(hEventToProducer);
		cleanup_devbuf(bufferOfhdev);
		dbg("failed to initialize %s buffer", "socket");
		return;
	}


	buffOfSocket->peer = bufferOfhdev;
	bufferOfhdev->peer = buffOfSocket;

	int ret = AddToArray(devno, hdevHandle, socketHandle, hEventToProducer, hEventToConsumer);
	if(ret != 0) {
		CloseHandle(hEventToProducer);
		CloseHandle(hEventToConsumer);
		cleanup_devbuf(buffOfSocket);
		cleanup_devbuf(bufferOfhdev);
		return;
	}
	PTP_WORK consumerWork = CreateThreadpoolWork(ThreadForConsumerRequest, &devno, NULL);
	if(consumerWork == NULL) {
		dbg("failed to create thread pool work: error: %lx", GetLastError());
		free(hdevHandle);
		return ERR_GENERAL;
	}
	SubmitThreadpoolWork(consumerWork);

	int	res;
	while(!interrupted) {
		if(!read_write_dev(buffOfSocket, bufferOfhdev, TRUE))
			break;
		if(!read_write_dev(bufferOfhdev, buffOfSocket, FALSE))
			break;

		if(buffOfSocket->invalid || bufferOfhdev->invalid)
			break;
		if(buffOfSocket->in_reading && bufferOfhdev->in_reading &&
			(buffOfSocket->in_writing || bufferOfhdev->bufc->step_reading != 3) &&
			(bufferOfhdev->in_writing || buffOfSocket->bufc->step_reading != 3)) {
			WaitForSingleObjectEx(hEventToProducer, INFINITE, TRUE);
			ResetEvent(hEventToProducer);
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

	while(buffOfSocket->in_reading || bufferOfhdev->in_reading || buffOfSocket->in_writing || bufferOfhdev->in_writing) {
		WaitForSingleObjectEx(hEventToProducer, INFINITE, TRUE);
	}

	cleanup_devbuf(buffOfSocket);
	cleanup_devbuf(bufferOfhdev);
	CloseHandle(hEventToProducer);


	CloseThreadpoolWork(work);

	dbg("stub forwarding stopped");
	dbg("break");
}


