#ifndef _NVRAM_H
#define _NVRAM_H 	1

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/autoconf.h>

#ifdef CONFIG_DUAL_IMAGE

#define UBOOT_NVRAM	    0
#define RT2860_NVRAM    1
#define RTDEV_NVRAM    	2
#define CERT_NVRAM    	3
#define WAPI_NVRAM    	4
#define FAKE_NVRAM      5
#else
#define RT2860_NVRAM    0
#define RTDEV_NVRAM    	1
#define CERT_NVRAM    	2
#define WAPI_NVRAM    	3
#define FAKE_NVRAM      4
#endif


void nvram_create();
void nvram_init(int index);
void nvram_close(int index);
void nvram_destroy();

int nvram_set(int index, char *name, char *value);
char *nvram_get(int index, char *name, char *value, int val_size);
int nvram_bufset(int index, char *name, char *value);
char const *nvram_bufget(int index, char *name, char *value, int val_size);

void nvram_buflist(int index);
int nvram_commit(int index);
int nvram_clear(int index);
int nvram_erase(int index);

//int nvram_read_oem(char *buf, int len);
//int nvram_write_oem(char *buf, int len);

int getNvramNum(void);
unsigned int getNvramOffset(int index);
unsigned int getNvramBlockSize(int index);
char *getNvramName(int index);
unsigned int getNvramIndex(char *name);
void toggleNvramDebug(void);
#ifdef __cplusplus
}
#endif

#endif

