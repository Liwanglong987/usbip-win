#pragma

#ifndef _USBIP_FORWARD_H_
#define _USBIP_FORWARD_H_

#define BUFREAD_P(devbuf)	((devbuf)->offp - (devbuf)->offhdr)
#define BUFREADMAX_P(devbuf)	((devbuf)->bufmaxp - (devbuf)->offp)
#define BUFREMAIN_C(devbuf)	((devbuf)->bufmaxc - (devbuf)->offc)
#define BUFHDR_P(devbuf)	((devbuf)->bufp + (devbuf)->offhdr)
#define BUFCUR_P(devbuf)	((devbuf)->bufp + (devbuf)->offp)
#define BUFCUR_C(devbuf)	((devbuf)->bufc + (devbuf)->offc)

typedef struct _devbuf {
	const char* desc;
	BOOL	is_req, swap_req;
	BOOL	invalid;
	/* asynchronous read is in progress */
	BOOL	in_reading;
	/* asynchronous write is in progress */
	BOOL	in_writing;
	/* step 1: reading header, 2: reading data */
	int	step_reading;
	HANDLE	hdev;
	char* bufp, * bufc;	/* bufp: producer, bufc: consumer */
	DWORD	offhdr;		/* header offset for producer */
	DWORD	offp, offc;	/* offp: producer offset, offc: consumer offset */
	DWORD	bufmaxp, bufmaxc;
	struct _devbuf* peer;
	OVERLAPPED	ovs[2];
	/* completion event for read or write */
	HANDLE	hEvent;
} devbuf_t;

void usbip_forward(HANDLE hdev_src, HANDLE hdev_dst, BOOL inbound);
BOOL init_devbuf(devbuf_t* buff, const char* desc, BOOL is_req, BOOL swap_req, HANDLE hdev, HANDLE hEvent);
BOOL read_write_dev(devbuf_t* rbuff, devbuf_t* wbuff);
int read_dev(devbuf_t* rbuff, BOOL swap_req_write);
#endif