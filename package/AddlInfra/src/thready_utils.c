/***************************************************************************
* thready_utils.c
*
* Created by Belkin International, Software Engineering on 2014-06-24
* Copyright (c) 2014 Belkin International, Inc. and/or its
* affiliates. All rights reserved.
*
* Belkin International, Inc. retains all right, title and interest
* (including all intellectual property rights) in and to this computer
* program, which is protected by applicable intellectual property
* laws.  Unless you have obtained a separate written license from
* Belkin International, Inc., you are not authorized to utilize all or
* a part of this computer program for any purpose (including
* reproduction, distribution, modification, and compilation into
* object code) and you must immediately destroy or return to Belkin
* International, Inc all copies of this computer program.  If you are
* licensed by Belkin International, Inc., your rights to utilize this
* computer program are limited by the terms of that license.
*
* To obtain a license, please contact Belkin International, Inc.
*
* This computer program contains trade secrets owned by Belkin
* International, Inc.  and, unless unauthorized by Belkin
* International, Inc. in writing, you agree to maintain the
* confidentiality of this computer program and related information and
* to not disclose this computer program and related information to any
* other person or entity.
*
* THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
* BELKIN INTERNATIONAL, INC.  EXPRESSLY DISCLAIMS ALL WARRANTIES,
* EXPRESS OR IMPLIED, INCLUDING THE WARRANTIES OF MERCHANTIBILITY,
* FITNESS FOR A PARTICULAR PURPOSE, TITLE, AND NON-INFRINGEMENT.
*
*
***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "thready_utils.h"
#include <belkin_diag.h>  /* Run-time diagnostics.  Keep as last include. */

#define VERBOSE_DEBUG
#ifdef VERBOSE_DEBUG
#define DBG(format,...) \
  do { printf( "thready: " format, ## __VA_ARGS__ ); } while(0)
#else
#define DBG(...)
#endif

/** @file Utilities for managing thread names.
 * This uses thread-local storage (TLS) to store a thread's name.
 */

static pthread_key_t tname_key; /**< TLS key for name store */

/** token used by tpthread_once() single execution mechanism to
 * ensure something only happens once.  In this case, we're making
 * sure that our TLS key is created only once
 */
static pthread_once_t tname_key_once = PTHREAD_ONCE_INIT; 

/** @brief TLS destructor.
 * This gets called when a thread exits to clean up the space
 * allocated to hold its' name.
 * @param data Anonymous pointer provided tot he destructor.  It holds
 * whatever was passed to pthread_setspecific.  In our case that's the
 * buffer for reading the thread name.
 */
static void tname_destructor(void *data) { 
  DBG( "Destructor freeing name \"%s\".\n", (char *)data );
  free( data );
}

/** @brief Key creator callback.
 * This function is called via pthread_once to create the TLS entry
 * and to register the destructor function.
 */
static void make_tname_key() {
    (void) pthread_key_create(&tname_key, tname_destructor);
    DBG( "%s key now \"%ld\"\n", __FUNCTION__, (long)tname_key );
}

/** @brief Get existing or create new buffer.
 * Functions that need access to the name buffer use this so they
 * don't have to manage the creation of the buffer.
 * @return PTHREAD_NAME_SIZE sized character array.
 */
static char *get_tname_buff() {
  char *ptr;

  (void) pthread_once(&tname_key_once, make_tname_key);
  if ((ptr = pthread_getspecific(tname_key)) == NULL) {
    ptr = (char *)CALLOC( PTHREAD_NAME_SIZE + 1, 1 );
    if( ptr )
      (void) pthread_setspecific(tname_key, ptr);
  } /*else {
    DBG( "Found buffer for tid %lu: \"%s\"\n", 
         (unsigned long int)pthread_self(), ptr );
  } */
  return (char *)ptr;
}

/* See thready_utils.h for details. */
void tu_set_my_thread_name( const char* name ) {
  char *buff = get_tname_buff();

  if( buff ) {
    DBG( "Setting thread name to \"%s\" (tid:%lu).\n", 
            name, (unsigned int long)pthread_self() );
    strncpy( buff, name, PTHREAD_NAME_SIZE );
  } else
    DBG( "ERROR: Could not acquire thread name buffer\n" );
}

/* See thready_utils.h for details. */
const char *tu_get_my_thread_name( void ) {
  char *buff = get_tname_buff();
  if( !buff )
    DBG( "%s found no buffer (tid:%lu).\n", 
           __FUNCTION__, (unsigned int long)pthread_self() );
  return buff ? buff : "UNNAMED";
}

/* -------------------- End of file -------------------- */
