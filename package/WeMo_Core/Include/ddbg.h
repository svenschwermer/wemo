/** ddbg
 * Define some simple debug loggin macros for use in netcam environment.
 */

#ifndef _DDBG_H_
#define _DDBG_H_

#ifndef DDBGFMT_PREFIX
// A dash of debugging macro sugar

// Note: Function returns pointer to internal static buffer.
// Not reentrant, not thread-safe.
static inline const char* _ddbg_timestamp() {
  time_t now = time(NULL);
  // Buff must be large enough to hold formatted timestampt
  static char buff[24] = {'\0'};

  if( -1 != now ) {
    strftime( buff, sizeof( buff ), "%Y-%m-%d %H:%M:%S", gmtime( &now ) );
  }
  return &buff[0];
}

#define DDBGFMT_PREFIX ">>>[%s] %s@%s:%d "
#define DDBG_ARGS_PREFIX _ddbg_timestamp(), __FILE__, __FUNCTION__, __LINE__
#define DDBGFMT(f) DDBGFMT_PREFIX f "\n"
#define DDBG(fmt, ...) printf( DDBGFMT(fmt), DDBG_ARGS_PREFIX, ## __VA_ARGS__ )
#endif

#endif

/* -------------------- End of ddbg.h -------------------- */

