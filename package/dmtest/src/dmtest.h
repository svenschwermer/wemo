#ifndef __DMTEST_H

#define HANDLE_ERR(what) \
  do {                   \
    perror( what );      \
    exit( -1 );          \
  } while(0)

#define handle_error_en(en, msg) \
  do {                           \
    errno = en;                  \
    perror(msg);                 \
    exit(EXIT_FAILURE);          \
  } while (0)

#define BUFF_SIZE (20)
#define SLEEP_TIME (5)

extern void test_func( void );
extern int launch_thread( const char* );

#endif // __DMTEST_H
