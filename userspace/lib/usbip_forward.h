#pragma

#ifndef _USBIP_FORWARD_H_
#define _USBIP_FORWARD_H_

typedef struct {
	char* buff;
	DWORD	bufmax;
	DWORD	offp, offc;
	int	step_reading;
	BOOL requireResponse;
} buffer;

typedef struct _devbuf {
	const char* desc;
	BOOL	is_req, swap_req;
	BOOL	invalid;
	/* asynchronous read is in progress */
	BOOL	in_reading;
	/* asynchronous write is in progress */
	BOOL	in_writing;
	/* step 1: reading header, 2: reading data */
	HANDLE	hdev;
	buffer* bufp, * bufc;
	struct _devbuf* peer;
	OVERLAPPED	ovs[2];
	/* completion event for read or write */
	HANDLE hEventForConsumer;
	HANDLE hEventForProducer;
} devbuf_t;

static volatile BOOL	interrupted;
extern void cleanup_devbuf(devbuf_t* buff);
extern void freeBuffer(buffer* buffer);
extern buffer* createNewBuffer();
extern BOOL init_devbufStatic(devbuf_t** buff, const char* desc, BOOL is_req, BOOL swap_req, HANDLE hdev, HANDLE hEventForConsumer, HANDLE hEventForProducer);
extern int read_dev(devbuf_t* rbuff, BOOL swap_req_write);
extern BOOL write_devbuf(devbuf_t* wbuff, devbuf_t* rbuff);
#endif