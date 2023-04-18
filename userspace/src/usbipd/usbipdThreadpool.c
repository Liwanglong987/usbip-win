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
			SetEvent(currentContainer->hEvent);
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

int AddToArray(devno_t devno, HANDLE HDEVHandle, HANDLE socketHandle, HANDLE hEvent) {
	SocketContainer* socketContainer = (SocketContainer*)malloc(sizeof(SocketContainer));
	if(socketContainer == NULL) {
		dbg("fail to mallo");
		return ERR_GENERAL;
	}
	socketContainer->socketHandle = socketHandle;
	socketContainer->hEvent = hEvent;
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
	int lastStep_reading = 0;
	BOOL writing = FALSE;
	Queue* firstQueue = GetFirstOne(devno);
	while(!interrupted)
	{
		if(firstQueue == NULL) {
			firstQueue = GetFirstOne(devno);
			continue;
		}
		else
		{
			devbuf_t* socketBuf = firstQueue->socketBuf;
			devbuf_t* hdevBuf = socketBuf->peer;

			if(!write_devbuf(hdevBuf, socketBuf)) {
				dbg("103");
				break;
			}

			if(socketBuf->requiredResponse == FALSE) {
				if(socketBuf->offp == socketBuf->offc) {
					dbg("108");
					Queue* toFree = Dequeue(devno);
					if(toFree != NULL) {
						dbg("109");
						free(toFree);
					}
					firstQueue = NULL;
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

				if(lastStep_reading != hdevBuf->step_reading) {
					dbg("112");
					if(lastStep_reading == 2 && hdevBuf->step_reading == 0) {
						Queue* toFree = Dequeue(devno);
						if(toFree != NULL) {
							dbg("115");
							free(toFree);
						}
						firstQueue = NULL;
						dbg("113");
					}
					lastStep_reading = hdevBuf->step_reading;
				}

			}
		}
	}
}
static void
signalhandler(int signal)
{
	interrupted = TRUE;
	SetEvent(hEvent);
}


void CALLBACK ThreadForProduceRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work)
{
	pvoid_t* pctx = (pvoid_t*)ctx;

	dbg("stub forwarding started");

	devbuf_t	buff_src, buff_dst;
	const char* desc_src, * desc_dst;
	BOOL	is_req_src;
	BOOL	swap_req_src, swap_req_dst;

	desc_src = "socket";
	desc_dst = "stub";
	is_req_src = TRUE;
	swap_req_src = TRUE;
	swap_req_dst = FALSE;
	HANDLE hdev_src = pctx->socketHandle;

	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(hEvent == NULL) {
		dbg("failed to create event");
		return;
	}
	devno_t devno = pctx->devno;
	HANDLE hdevHandle = open_stub_dev(devno);
	if(hdevHandle == INVALID_HANDLE_VALUE) {
		dbg("cannot open devno: %hhu", devno);
		return;
	}
	HANDLE hdev_dst = hdevHandle;
	if(!init_devbuf(&buff_src, desc_src, TRUE, swap_req_src, hdev_src, hEvent)) {
		CloseHandle(hEvent);
		dbg("failed to initialize %s buffer", desc_src);
		return;
	}
	if(!init_devbuf(&buff_dst, desc_dst, FALSE, swap_req_dst, hdev_dst, hEvent)) {
		CloseHandle(hEvent);
		dbg("failed to initialize %s buffer", desc_dst);
		cleanup_devbuf(&buff_src);
		return;
	}
	buff_src.peer = &buff_dst;
	buff_dst.peer = &buff_src;

	devbuf_t* rbuff = &buff_src;
	devbuf_t* wbuff = &buff_dst;
	int	res;
	while(!interrupted) {

		if(!rbuff->in_reading) {
			res = read_dev(rbuff, wbuff->swap_req);
			if(res < 0)
				break;
		}
		if(rbuff->finishRead == TRUE) {
			if(write_devbuf(wbuff, rbuff) == FALSE)
				break;
		}


		if(!wbuff->in_reading) {
			res = read_dev(wbuff, rbuff->swap_req);
			if(res < 0)
				break;
		}
		if(wbuff->finishRead == TRUE) {
			if(write_devbuf(rbuff, wbuff) == FALSE)
				break;
		}

		if(buff_src.invalid || buff_dst.invalid)
			break;
		if(buff_src.in_reading && buff_dst.in_reading &&
			(buff_src.in_writing || BUFREMAIN_C(&buff_dst) == 0) &&
			(buff_dst.in_writing || BUFREMAIN_C(&buff_src) == 0)) {
			WaitForSingleObjectEx(hEvent, INFINITE, TRUE);
			ResetEvent(hEvent);
		}
	}

	if(interrupted) {
		info("CTRL-C received\n");
	}
	signal(SIGINT, SIG_DFL);

	if(buff_src.in_reading)
		CancelIoEx(hdev_src, &buff_src.ovs[0]);
	if(buff_dst.in_reading)
		CancelIoEx(hdev_dst, &buff_dst.ovs[0]);

	while(buff_src.in_reading || buff_dst.in_reading || buff_src.in_writing || buff_dst.in_writing) {
		WaitForSingleObjectEx(hEvent, INFINITE, TRUE);
	}

	cleanup_devbuf(&buff_src);
	cleanup_devbuf(&buff_dst);
	CloseHandle(hEvent);

	closesocket(pctx->socketHandle);
	free(pctx);

	CloseThreadpoolWork(work);

	dbg("stub forwarding stopped");
	/*
	dbg("enter");
	pvoid_t* pvoid_data = (pvoid_t*)ctx;
	devno_t devno = pvoid_data->devno;
	HANDLE socketHandle = pvoid_data->socketHandle;

	HANDLE hdevHandle;
	DeviceContainer* existDeviceContainer;


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
	HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(hEvent == NULL) {
		dbg("failed to create event");
		return;
	}

	devbuf_t* pBuffOfSocket;
	if(!init_devbufStatic(&pBuffOfSocket, "socket", TRUE, TRUE, socketHandle, hEvent)) {
		CloseHandle(hEvent);
		dbg("failed to initialize %s buffer", "socket");
		return;
	}
	devbuf_t buffOfSocket = *pBuffOfSocket;

	devbuf_t* pBufferOfhdev;
	if(!init_devbufStatic(&pBufferOfhdev, "stub", FALSE, FALSE, hdevHandle, hEvent)) {
		CloseHandle(hEvent);
		cleanup_devbuf(pBufferOfhdev);
		dbg("failed to initialize %s buffer", "socket");
		return;
	}
	devbuf_t bufferOfhdev = *pBufferOfhdev;

	buffOfSocket.peer = &bufferOfhdev;
	bufferOfhdev.peer = &buffOfSocket;

	int ret = AddToArray(devno, hdevHandle, socketHandle, hEvent);
	if(ret != 0) {
		CloseHandle(hEvent);
		cleanup_devbuf(&buffOfSocket);
		cleanup_devbuf(&bufferOfhdev);
		return;
	}
	PTP_WORK consumerWork = CreateThreadpoolWork(ThreadForConsumerRequest, &devno, NULL);
	if(consumerWork == NULL) {
		dbg("failed to create thread pool work: error: %lx", GetLastError());
		free(hdevHandle);
		return ERR_GENERAL;
	}
	SubmitThreadpoolWork(consumerWork);

	while(TRUE)
	{
		if(!buffOfSocket.in_reading) {
			dbg("202");
			int ret = read_dev(&buffOfSocket, bufferOfhdev.swap_req);
			if(ret < 0) {
				dbg("203");
				break;
			}
		}
		if(buffOfSocket.offp > 0) {
			if(Contains(devno, &buffOfSocket) == FALSE) {
				dbg("205");
				Enqueue(devno, &buffOfSocket);
			}
		}

		BOOL writeError = write_devbuf(&buffOfSocket, &bufferOfhdev);
		if(writeError == FALSE) {
			break;
		}

		if(buffOfSocket.invalid || bufferOfhdev.invalid) {
			dbg("208");
			break;
		}

		if(buffOfSocket.in_reading && bufferOfhdev.in_reading &&
			(buffOfSocket.in_writing || BUFREMAIN_C(&bufferOfhdev) == 0) &&
			(bufferOfhdev.in_writing || BUFREMAIN_C(&buffOfSocket) == 0))
		{
			dbg("209");
			WaitForSingleObjectEx(hEvent, INFINITE, TRUE);
			ResetEvent(hEvent);
		}
	}
	*/

	dbg("break");
}


