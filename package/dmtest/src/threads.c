#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>

#include "dmtest.h"

#ifdef DMALLOC  // This should be last include
#include <dmalloc.h>
#endif

static void init_buffer( char *buff, char data, size_t len ) {
  memset( buff, data, len );
  *(buff + len - 1) = 0;
}

void test_func( void ) {
  char data[] = { '\xde', '\xad', '\xba', '\xbe', 0 };
  char *buffer = NULL;

  if( (buffer = malloc( BUFF_SIZE)))
      init_buffer( buffer, 'Z', BUFF_SIZE );
  else
    HANDLE_ERR( "malloc buffer" );

  printf( "Before intentional overflow:\n" 
          "buffer: \"%s\"\n", buffer );

  strcat( &buffer[BUFF_SIZE - 1], data );

  printf( "After intentional overflow:\n" 
          "buffer: \"%s\"\n", buffer );

  free( buffer );
}


static void *runner( void *stuff ) {
  const char *name = (const char*)stuff;

  printf( "Thread \"%s\" starts\n", name );
  test_func();
  printf( "Thread \"%s\" exits\n", name );
  pthread_exit( NULL );
}


int launch_thread( const char* name ) {
  int status = 0;
  pthread_t t;
  pthread_attr_t attr;

  if( (status = pthread_attr_init(&attr)) == 0 )
    if( (status = pthread_create( &t, &attr, runner, (void *)name )) == 0 ) {
      printf( "Created thread %lu\n", t );
    } else
      handle_error_en( status, "pthread_create" );
  else
    handle_error_en( status, "pthread_attr_init" );

  return status;
}
