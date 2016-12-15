#ifndef __SYSEVENTD_H_
#define __SYSEVENTD_H_
/*
 * Copyright (c) 2008 by Cisco Systems, Inc. All Rights Reserved.
 *
 * This work is subject to U.S. and international copyright laws and
 * treaties. No part of this work may be used, practiced, performed,
 * copied, distributed, revised, modified, translated, abridged, condensed,
 * expanded, collected, compiled, linked, recast, transformed or adapted
 * without the prior written consent of Cisco Systems, Inc. Any use or
 * exploitation of this work without authorization could subject the
 * perpetrator to criminal and civil liability.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>
#include "syseventd.h"

/*
 * get id assigned to a thread
 */
inline int thread_get_id(pthread_key_t key) {
   void* spec = pthread_getspecific(key);
   if (NULL != spec) {
      return( ((worker_thread_private_info_t *)spec)->id);
   } else {
      return(-1);
   }
}

/*
 * For a thread, set its current state
 */
inline void thread_set_state(pthread_key_t key, int state) {
   int id = thread_get_id(key);
   if (-1 != id && id > 0 && id <= NUM_WORKER_THREAD) {
      thread_stat_info[id-1].state = state;
   }
}

/*
 * For a thread, increment the number of times it has woken up from the semaphore
 */
inline void thread_activated(pthread_key_t key) {
   int id = thread_get_id(key);
   if (-1 != id && id > 0 && id <= NUM_WORKER_THREAD) {
      thread_stat_info[id-1].num_activation++;
   }
}

/*
 * get read side of pipe assigned to a thread
 */
inline int thread_get_private_pipe_read(pthread_key_t key) {
   void* spec = pthread_getspecific(key);
   if (NULL != spec) {
      return( ((worker_thread_private_info_t *)spec)->fd);
   } else {
      return(-1);
   }
}

/*
 * Procedure     : trim
 * Purpose       : trims a string
 * Parameters    :
 *    in         : A string to trim
 * Return Value  : The trimmed string
 * Note          : This procedure will change the input sting in situ
 */
char *trim(char *in)
{
   // trim the front of the string
   if (NULL == in) {
      return(NULL);
   }
   char *start = in;
   while(isspace(*start)) {
      start++;
   }
   // trim the end of the string

   char *end = start+strlen(start);
   end--;
   while(isspace(*end)) {
      *end = '\0';
      end--;
   }
   return(start);
}

/*
 * Procedure     : sysevent_strdup
 * Purpose       : strdup
 * Parameters    :
 *    s          : The sting to duplicate
 *    file       : The file of the procedure that called sysevent_realloc
 *    line       : the line number of the call to sysevent_realloc
 * Return Value  : A pointer to the allocated store or NULL
 */
void *sysevent_strdup(const char* s, char* file, int line)
{
   char *store = strdup(s);

   SE_INC_LOG(ALLOC_FREE,
      printf("+allocating: %s @ %8p %s:%d\n", s, store, file, line);
   )
   return(store);
}

/*
 * Procedure     : sysevent_malloc
 * Purpose       : malloc
 * Parameters    :
 *    size       : The number of bytes to malloc
 *    file       : The file of the procedure that called sysevent_realloc
 *    line       : the line number of the call to sysevent_realloc
 * Return Value  : A pointer to the allocated store or NULL
 */
void *sysevent_malloc(size_t size, char* file, int line) 
{
   void *store = malloc(size);

   SE_INC_LOG(ALLOC_FREE,
      printf("+allocating: %6d @ %8p %s:%d\n", size, store, file, line);
   )
   if (NULL == store) {
      SE_INC_LOG(ALLOC_FREE,
         printf("allocation failure. %s:%d\n", file, line);
      )
   }
   return(store);
}

/*
 * Procedure     : sysevent_realloc
 * Purpose       : realloc
 * Parameters    :
 *    ptr        : A pointer to the store to realloc
 *    size       : The number of bytes to realloc
 *    file       : The file of the procedure that called sysevent_realloc
 *    line       : the line number of the call to sysevent_realloc
 * Return Value  : A pointer to the allocated store or NULL
 */
void *sysevent_realloc(void* ptr, size_t size, char* file, int line) 
{
   void* orig = ptr;
   void *store = realloc(ptr, size);

   SE_INC_LOG(ALLOC_FREE,
      printf("+reallocating %8p: %6d @ %8p %s:%d\n", orig, size, store, file, line);
   )
   if (NULL == store) {
      SE_INC_LOG(ALLOC_FREE,
         printf("reallocation failure. %s:%d\n", file, line);
      )
   }
   return(store);
}

/*
 * Procedure     : sysevent_free
 * Purpose       : free
 * Parameters    :
 *    addr       : The address of the pointer to the store to free
 *    file       : The file of the procedure that called sysevent_realloc
 *    line       : the line number of the call to sysevent_realloc
 * Return Value  : A pointer to the allocated store or NULL
 */
void sysevent_free(void **addr, char* file, int line)
{
   if (NULL != *addr) {

      void* orig = *addr;
      SE_INC_LOG(ALLOC_FREE,
         printf("-freeing: %8p %s:%d\n", orig, file, line);
      )
      free(*addr);
      *addr = NULL;
   } else {
      SE_INC_LOG(ERROR,
         printf("Warning. Attempt to free NULL store %s:%d\n", file, line);
      )
   }
}

#endif // __SYSEVENTD_H_
