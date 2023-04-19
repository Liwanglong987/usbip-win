#include "usbip_windows.h"

#include <signal.h>
#include <stdlib.h>

#include "usbip_proto.h"
#include "usbip_network.h"
#include "usbip_forward.h"
#include "usbipdThreadpool.h"

static BOOL
setup_rw_overlapped(devbuf_t* buff)
{
	int	i;

	for(i = 0; i < 2; i++) {
		memset(&buff->ovs[i], 0, sizeof(OVERLAPPED));
		buff->ovs[i].hEvent = (HANDLE)buff;
	}
	return TRUE;
}

BOOL
init_devbufStatic(devbuf_t** buff, const char* desc, BOOL is_req, BOOL swap_req, HANDLE hdev, HANDLE hEventForConsumer, HANDLE hEventForProducer)
{
	devbuf_t* newBuff = (devbuf_t*)malloc(sizeof(devbuf_t));
	char* buff = (char*)malloc(1024);
	bufferContainer* buffContainer = (bufferContainer*)malloc(sizeof(bufferContainer));
	if(newBuff == NULL || buff == NULL || buffContainer == NULL) {
		dbg("fail to malloc");
		return FALSE;
	}
	buffContainer->buff = buff;
	buffContainer->bufMax = 1024;
	buffContainer->offp = 0;
	buffContainer->offc = 0;
	buffContainer->step = WaitingForRead;
	buffContainer->RWStatus = Space;
	buffContainer->requiredResponse = FALSE;

	newBuff->bufp = buffContainer;
	newBuff->bufc = buffContainer;
	newBuff->hdev = hdev;
	newBuff->desc = desc;
	newBuff->invalid = FALSE;
	newBuff->is_req = is_req;
	newBuff->swap_req = swap_req;
	newBuff->hEventForConsumer = hEventForConsumer;
	newBuff->hEventForProducer = hEventForProducer;
	if(!setup_rw_overlapped(newBuff)) {
		free(newBuff->bufp);
		free(newBuff);
		return FALSE;
	}
	*buff = newBuff;
	return TRUE;
}

int read_dev(devbuf_t* rbuff, BOOL swap_req_write)
{
	struct usbip_header* hdr;
	unsigned long	xfer_len, iso_len, len_data;

	if(rbuff->bufp->offp < sizeof(struct usbip_header)) {
		rbuff->bufp->step = ReadingHeader;
		if(!read_devbuf(rbuff, sizeof(struct usbip_header) - rbuff->bufp->offp))
			return -1;
		return 0;
	}

	hdr = (struct usbip_header*)BUFHDR_P(rbuff);
	if(rbuff->step_reading == 1) {
		if(rbuff->swap_req)
			swap_usbip_header_endian(hdr, TRUE);
		rbuff->step_reading = 2;
	}

	xfer_len = get_xfer_len(rbuff->is_req, hdr);
	iso_len = get_iso_len(rbuff->is_req, hdr);

	len_data = xfer_len + iso_len;
	if(BUFREAD_P(rbuff) < len_data + sizeof(struct usbip_header)) {
		DWORD	nmore = (DWORD)(len_data + sizeof(struct usbip_header)) - BUFREAD_P(rbuff);

		if(!read_devbuf(rbuff, nmore))
			return -1;
		return 0;
	}

	if(rbuff->swap_req && iso_len > 0)
		swap_iso_descs_endian((char*)(hdr + 1) + xfer_len, hdr->u.ret_submit.number_of_packets);

	DBG_USBIP_HEADER(hdr);

	if(swap_req_write) {
		if(iso_len > 0)
			swap_iso_descs_endian((char*)(hdr + 1) + xfer_len, hdr->u.ret_submit.number_of_packets);
		swap_usbip_header_endian(hdr, FALSE);
	}

	if(hdr->base.command == USBIP_CMD_SUBMIT && ((hdr->u.cmd_submit.setup[0] & 0x80) == 0)) {
		rbuff->requiredResponse = TRUE;
	}
	rbuff->offhdr += (sizeof(struct usbip_header) + len_data);
	if(rbuff->bufp == rbuff->bufc)
		rbuff->bufmaxc = rbuff->offp;
	rbuff->step_reading = 0;
	rbuff->finishRead = TRUE;
	return 1;
}
