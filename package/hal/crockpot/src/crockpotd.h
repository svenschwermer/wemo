#ifndef _CROCKPOTD_H_
#define _CROCKPOTD_H_

#ifndef CROCKPOTD_PRIVATE
#error "Information in this header is private to the HAL layer, application code should include it"
#endif

#ifndef FALSE
#define FALSE     0
#endif

#ifndef TRUE
#define TRUE      (!FALSE)
#endif

#define CROCKPOTD_PORT	2666

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct {
   u16      Cmd;
	int		Err;
} MsgHeader;

#define CMD_RESPONSE				0x8000

#define CMD_EXIT					1
#define CMD_EXITING				(CMD_EXIT | CMD_RESPONSE)

#define CMD_GET_MODE				2
#define CMD_MODE					(CMD_GET_MODE | CMD_RESPONSE)

#define CMD_SET_MODE				3
#define CMD_MODE_SET				(CMD_SET_MODE | CMD_RESPONSE)

#define CMD_GET_COOK_TIME		4
#define CMD_COOK_TIME			(CMD_GET_COOK_TIME | CMD_RESPONSE)

#define CMD_SET_COOK_TIME		5
#define CMD_COOK_TIME_SET		(CMD_SET_COOK_TIME | CMD_RESPONSE)

#define CMD_GET_FW_VERSION		6
#define CMD_FW_VERSION			(CMD_GET_FW_VERSION | CMD_RESPONSE)


#define MAX_VERSION_LEN	16
typedef struct {
	MsgHeader Hdr;
	union {
		CROCKPOT_MODE Mode;
		int IntData;
		char Version[MAX_VERSION_LEN];	// Vx.xx<null>
	} u;
} CrockpotdMsg;

// a type to make sockaddr/sockaddr_in translations a bit easier
typedef union {
   struct sockaddr s;
   struct sockaddr_in i;
   #define ADDR   i.sin_addr.s_addr
   #define PORT   i.sin_port
} AdrUnion;

#endif	// _CROCKPOTD_H_

