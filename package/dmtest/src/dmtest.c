#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "dmtest.h"

#ifdef DMALLOC  // This should be last include
#include <dmalloc.h>
#endif

int main( int argc, char **argv ) {
  printf( "dmtest starts.\n" );

#ifdef DMALLOC
# ifndef DMALLOC_ENV
  // Output of "dmalloc -n -v -l /var/log/dmalloc.log -i 1 all"
  static char *dm_config = "debug=0xcf4ed2b,inter=1,log=/var/log/dmalloc.log";
  printf( "Using static dmalloc configuration \"%s\".\n", dm_config );
  dmalloc_debug_setup(dm_config);
# else
  printf( "Using dmalloc settings from environment\n" );
# endif
#endif

#ifdef DMTEST_MT
  if( argc > 1 ) {
    for( int i = 1; i < argc; i++ ) {
      printf( "Launching thread for \"%s\".\n", argv[i]);
      if( launch_thread( argv[i] ) != 0 )
        perror( "launch" );
    }
  } else {
    printf( "Nothing to do.\n" );
    exit( -1 );
  }
#else
  test_func();
#endif
  printf( "damage done.\n" );

  pthread_exit(0);
}

