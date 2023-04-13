#include "usbipd.h"

#include "usbip_network.h"
#include "usbipd_stub.h"
#include "usbip_setupdi.h"
#include "usbip_forward.h"
#include "usbipdThreadpool.h"

typedef struct {
	HANDLE	hdev;
	SOCKET	sockfd;
} forwarder_ctx_t;

static VOID CALLBACK
forwarder_stub(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PTP_WORK work)
{
	forwarder_ctx_t* pctx = (forwarder_ctx_t*)ctx;

	dbg("stub forwarding started");

	usbip_forward((HANDLE)pctx->sockfd, pctx->hdev, TRUE);

	closesocket(pctx->sockfd);
	CloseHandle(pctx->hdev);
	free(pctx);

	CloseThreadpoolWork(work);

	dbg("stub forwarding stopped");
}

static int
export_device(devno_t devno, SOCKET sockfd)
{
	HANDLE socketHandle = (HANDLE)sockfd;
	DeviceContainer* existDeviceContainer = NULL;
	BOOL isContainer = IsContains(devno, existDeviceContainer);
	HDEVSocketContainer* pHDEVSocketContainer = NULL;
	if(isContainer == FALSE || existDeviceContainer == NULL) {
		int ret = CreateNewContainer(socketHandle, devno, pHDEVSocketContainer);
		if(ret != 0) {
			closesocket(sockfd);
			dbg("failed to create new socketContainer by error: %d", ret);
			return ret;
		}
		DeviceContainer* newDeviceContainer = (DeviceContainer*)malloc(sizeof(DeviceContainer));
		if(newDeviceContainer == NULL) {
			closesocket(sockfd);
			free(pHDEVSocketContainer);
			dbg("failed to malloc before CreateThreadpoolWork");
			return ERR_GENERAL;
		}
		newDeviceContainer->devno = devno;
		newDeviceContainer->FirstSocketHDEVContainer = pHDEVSocketContainer;
		newDeviceContainer->Next = NULL;
		AddDeviceToArray(&newDeviceContainer);
	}
	else
	{
		HANDLE existHDEVHandle = existDeviceContainer->FirstSocketHDEVContainer->HDEVHandle;
		int ret = CreateContainerByOpenedHDEV(socketHandle, existHDEVHandle, pHDEVSocketContainer);
		if(ret != 0) {
			dbg("failed to create thread pool work: error: %dx", ret);
			return ret;
		}

		AddToArray(existDeviceContainer, pHDEVSocketContainer);
	}

	//Create produce Request Thread
	PTP_WORK producerWork = CreateThreadpoolWork(ThreadForProduceRequest, pHDEVSocketContainer, NULL);
	if(producerWork == NULL) {
		dbg("failed to create thread pool work: error: %lx", GetLastError());
		free(pHDEVSocketContainer);
		return ERR_GENERAL;
	}
	SubmitThreadpoolWork(producerWork);
	return 0;
}

int
recv_request_import(SOCKET sockfd)
{
	struct op_import_request req;
	struct usbip_usb_device	udev;
	devno_t	devno;
	int rc;

	memset(&req, 0, sizeof(req));

	rc = usbip_net_recv(sockfd, &req, sizeof(req));
	if(rc < 0) {
		dbg("usbip_net_recv failed: import request");
		return -1;
	}
	PACK_OP_IMPORT_REQUEST(0, &req);

	devno = get_devno_from_busid(req.busid);
	if(devno == 0) {
		dbg("invalid bus id: %s", req.busid);
		usbip_net_send_op_common(sockfd, OP_REP_IMPORT, ST_NODEV);
		return -1;
	}

	usbip_net_set_keepalive(sockfd);

	/* should set TCP_NODELAY for usbip */
	usbip_net_set_nodelay(sockfd);

	/* export device needs a TCP/IP socket descriptor */
	rc = export_device(devno, sockfd);
	if(rc < 0) {
		dbg("failed to export device: %s, err:%d", req.busid, rc);
		usbip_net_send_op_common(sockfd, OP_REP_IMPORT, ST_NA);
		return -1;
	}

	rc = usbip_net_send_op_common(sockfd, OP_REP_IMPORT, ST_OK);
	if(rc < 0) {
		dbg("usbip_net_send_op_common failed: %#0x", OP_REP_IMPORT);
		return -1;
	}

	build_udev(devno, &udev);
	usbip_net_pack_usb_device(1, &udev);

	rc = usbip_net_send(sockfd, &udev, sizeof(udev));
	if(rc < 0) {
		dbg("usbip_net_send failed: devinfo");
		return -1;
	}

	dbg("import request busid %s: complete", req.busid);

	return 0;
}