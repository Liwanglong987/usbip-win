#include <winsock2.h>
#include "usbip_setupdi.h"
#include "usbipdThreadpool.h"
#include "usbipd_stub.h"
#include "usbip_forward.h"
#include "usbip_common.h"
#include "usbip_proto.h"

static DeviceForConsumerThread* consumerThreadList = NULL;

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


void CALLBACK ThreadForConsumerRequest(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work)
{
	devno_t* pDevno = (devno_t*)ctx;
	if(AddNewConsumerThreadForDevice(*pDevno) == FALSE) {
		return;
	}
	devno_t devno = * pDevno;
	SocketQueue* currentSocketContainer = GetFirstOne(devno);
	int step = 0;
	while(!interrupted)
	{
		if(currentSocketContainer == NULL) {
			currentSocketContainer = GetFirstOne(devno);
			step = 0;
			if(currentSocketContainer != NULL) {
				dbg("getnewone");
			}
			continue;
		}
		else
		{
			devbuf_t* socketBuf = currentSocketContainer->socketBuf;
			devbuf_t* hdevBuf = socketBuf->peer;
			HANDLE handles[] = {
				hdevBuf->hEventForWriter,
				hdevBuf->hEventForReader
			};
			while(TRUE)
			{
				if(!write_devbuf(hdevBuf, socketBuf))
					break;


				if((hdevBuf->in_writing || (hdevBuf->bufp->step_reading != 0 && hdevBuf->bufp->step_reading != 3))) {
					if(read_dev(hdevBuf, socketBuf->swap_req) < 0)
						break;
				}

				if((socketBuf->bufc->step_reading != 3 || socketBuf->bufc->offp == socketBuf->bufc->offc) &&
					!hdevBuf->in_reading &&
					!hdevBuf->in_writing) {
					currentSocketContainer = currentSocketContainer->Next;
					break;
				}

				if(hdevBuf->in_reading && (hdevBuf->in_writing || socketBuf->bufc->step_reading != 3 || socketBuf->bufc->offp == socketBuf->bufc->offc)) {
					WaitForMultipleObjectsEx(2, handles, FALSE, INFINITE, TRUE);
					ResetEvent(hdevBuf->hEventForWriter);
					ResetEvent(hdevBuf->hEventForReader);
				}
			}
		}
	}
}

