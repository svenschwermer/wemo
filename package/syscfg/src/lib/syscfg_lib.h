/*    
 * © 2013 Belkin International, Inc. and/or its affiliates. All rights reserved.
 */

    
#ifndef _SYSCFG_LIB_H_
#define _SYSCFG_LIB_H_
    
#define SYSCFG_HASH_TABLE_SZ 128      // number of buckets in hash table
#define MAX_NAME_LEN         128      // max name len including namespace
                                      //      size eg "cisco-org:lan0:dhcp:ipaddr"

#define SYSCFG_SHM_FILE "/tmp/syscfg.shmid"

/*
 * Magic number within shared mem superblock for sanity check
 */
#define SYSCFG_SHM_MAGIC     0xDEADBEEF
/*
 * shared memory key unique to syscfg, used to retrieve shmid
 */
#define SYSCFG_SHM_PROJID    237
/*
 * shared memory region size - multiple of 4K PAGE_SIZE. 
 * TODO - currently set to 64kb. make it expandable
 * If using a file based persistent store, then SYSCFG_SHM_SIZE : DEFAULT_MAX_FILE_SZ needs to be tweaked
 *    if SYSCFG_SHM_SIZE is too small you will see ERR_MEM_ALLOC, and 
 *    if DEFAULT_MAX_FILE_SZ is too small you will see ERR_NO_SPACE.
 * It is preferable to be limited by file size.
 */
#define NUM_SHM_PAGES           25
#ifdef PAGE_SIZE
   #define SYSCFG_SHM_SIZE      ((NUM_SHM_PAGES)*(PAGE_SIZE))
#else
   #define SYSCFG_SHM_SIZE      ((NUM_SHM_PAGES)*(getpagesize()))
#endif


#define SYSCFG_STORE_PATH_SZ  128

/*
 * When using file based persistent store, this is the maximum size of the file, unless overrided
 */
#define DEFAULT_MAX_FILE_SZ   (48*1024)

/*
 * Number of free table buckets
 */
#define NUM_BUCKETS 7 

/*
 * Current version 0.1
 */
#define SYSCFG_SHM_VERSION_MAJOR 0
#define SYSCFG_SHM_VERSION_MINOR 1

typedef enum {
    STORE_FILE,
    STORE_MTD_DEVICE
} store_type_t;

typedef struct {
    store_type_t type;
    char         path[SYSCFG_STORE_PATH_SZ];
    int          hdr_size;
    long int     max_size;
} store_info_t;

typedef unsigned int shmoff_t;

/*
 * syscfg controlblock placed at the beginning of shared memory region
 */
typedef struct {
    int  magic;
    uint version;
    int  size;         // size of shared memory
    int   shmid;
    store_type_t store_type;
    char         store_path[SYSCFG_STORE_PATH_SZ];
    long int     max_store_size;
    long int     used_store_size;
#ifdef SC_POSIX_SEM
    sem_t read_lock;
    sem_t write_lock;
    sem_t commit_lock;
#elif SC_SYSV_SEM
    int semid;
#endif
} shm_cb;

typedef struct {
    int size;   // element size - one of 16b, 32b, 64b, 128b, 256b, 1024b
    int mf;     // multiply-factor - how many entries to pre-create
    shmoff_t head;
} shm_free_table;

typedef struct {
    int size;
    shmoff_t next;
} mm_item;


/******************************************************************************
 *                shm memory api
 ******************************************************************************/

#define MM_OVERHEAD (sizeof(mm_item))

#define MM_ITEM(ctx,offset)     ((mm_item *)(((char *)ctx)+offset))
#define MM_ITEM_NEXT(ctx,offset) (((mm_item *)(((char *)ctx)+offset))->next)
#define MM_ITEM_SIZE(ctx,offset) (((mm_item *)(((char *)ctx)+offset))->size)

/*
 * ft - free table. contains a linked list of mm_items
 */
typedef struct {
    int  db_size;     // size of data block within shm
    uint db_start;
    uint db_end;
    uint db_cur;
    shm_free_table  ft[NUM_BUCKETS];
} shm_mm;

typedef struct {
    uint name_sz;
    uint value_sz;
    shmoff_t next;   
} ht_entry;

#define HT_ENTRY(ctx,offset)         ((ht_entry *)(((char *)ctx)+offset))
#define HT_ENTRY_NAMESZ(ctx,offset)  (((ht_entry *)(((char *)ctx)+offset))->name_sz)
#define HT_ENTRY_VALUESZ(ctx,offset) (((ht_entry *)(((char *)ctx)+offset))->value_sz)
#define HT_ENTRY_NEXT(ctx,offset)    (((ht_entry *)(((char *)ctx)+offset))->next)

#define HT_ENTRY_NAME(ctx,offset)  ((char *)(((char *)ctx)+offset+sizeof(ht_entry)))
#define HT_ENTRY_VALUE(ctx,offset) ((char *)(((char *)ctx)+offset+sizeof(ht_entry)+HT_ENTRY_NAMESZ(ctx,offset)))

/* 
 * WARNING: if you add new elements into this struct, check to see if
 * offset calculation are still valid. Most often you need to fix them
 */
typedef struct {
    shm_cb          cb;
    shm_mm          mm;
    shmoff_t        ht[SYSCFG_HASH_TABLE_SZ];
    char           *data;
} syscfg_shm_ctx;

/*
 * shm_mm - shm memory management
 *   shm data segments is carved into chunks of 2^ sized elements
 *   shm_mm_alloc(sz) - will allocate a sz byte chunk closest to the chunk size
 */
static int make_mm_items (syscfg_shm_ctx *ctx, shm_free_table *ft);
int shm_malloc (syscfg_shm_ctx *ctx, int size, shmoff_t *out_offset);
void shm_free (syscfg_shm_ctx *ctx, shmoff_t offset);

static int   syscfg_shm_init ();
static void *syscfg_shm_create (store_info_t store_info, int *out_shmid);
static void *syscfg_shm_attach (int *out_shmid);
static int   syscfg_shm_getid ();
static void syscfg_shm_destroy (syscfg_shm_ctx *ctx);

static int _syscfg_getsz (long int *used_sz, long int *max_sz);
static int _syscfg_set (const char *ns, const char *name, const char *value, int nolock);
static int _syscfg_unset (const char *ns, const char *name, int nolock);
static char* _syscfg_get (const char *ns, const char *name);
static int _syscfg_getall (char *buf, int bufsz);
static void _syscfg_destroy ();

static void encrypt_str (const char *instr, char *outstr);

static inline int read_lock (syscfg_shm_ctx *ctx);
static inline int read_unlock (syscfg_shm_ctx *ctx);
static inline int write_lock (syscfg_shm_ctx *ctx);
static inline int write_unlock (syscfg_shm_ctx *ctx);
static inline int commit_lock (syscfg_shm_ctx *ctx);
static inline int commit_unlock (syscfg_shm_ctx *ctx);

#endif // _SYSCFG_LIB_H_
