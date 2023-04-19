#pragma

#ifndef _USBIP_FORWARD_H_
#define _USBIP_FORWARD_H_

typedef enum {
	Reading = 0,
	Writing=1,
	Space=2
} RWStatus;

typedef struct {
	char* buff;
	int offp;
	int bufMax;
	int offc;
	BOOL parseFinish;
	RWStatus RWStatus;
	BOOL requiredResponse;
}bufferContainer;

typedef struct _devbuf {
	const char* desc;
	BOOL	is_req, swap_req;
	BOOL	invalid;
	HANDLE	hdev;
	bufferContainer* bufp, * bufc;

	struct _devbuf* peer;
	OVERLAPPED	ovs[2];
	/* completion event for read or write */
	HANDLE hEventForConsumer;
	HANDLE hEventForProducer;
} devbuf_t;

extern void cleanup_devbuf(devbuf_t* buff);
static volatile BOOL	interrupted;

#endif