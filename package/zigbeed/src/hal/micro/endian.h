/** @file hal/micro/endian.h
 * See @ref endian for detailed documentation.
 *
 * <!-- Copyright 2009 by Ember Corporation. All rights reserved.            --> 
 */

#ifndef __ENDIAN_H__
#define __ENDIAN_H__

/** @addtogroup endian
 * Functions that provide conversions from network to host byte
 * order.  Network byte order is big endian, so these APIs are only necessary
 * on platforms which have a natural little endian byte order.  On big-endian
 * platforms, the APIs are macro'd away to nothing.  See endian.h for source
 * code.
 *@{
 */

// If this is being compiled on a Mac then we need to disable the defines
// from its own internal _endian.h file since they conflict with our
// defines. This allows us to compile the 3xx tools for Mac.
#ifdef __APPLE__
  #undef NTOHL
  #undef NTOHS
  #undef HTONL
  #undef HTONS
#endif

#if BIGENDIAN_CPU == FALSE

  /** @brief Converts a short (16-bit) value from network to host byte order
   *
   */
  #ifndef NTOHS  // some platforms already define this
    int16u NTOHS(int16u val);
  #endif

  /** @brief Converts a long (32-bit) value from network to host byte order
   *
   */
  #ifndef NTOHL  // some platforms already define this
    int32u NTOHL(int32u val);
  #endif

#else // BIGENDIAN_CPU == TRUE

  #ifndef NTOHS  // some platforms already define this
    #define NTOHS(val)  (val)
  #endif
  #ifndef NTOHL  // some platforms already define this
    #define NTOHL(val)  (val)
  #endif

#endif

// The HTON and NTOH operations are the same for sane architectures (eg. big
// and little endian) so define the inverse functions if they don't exist
#ifndef HTONL
  #define HTONL NTOHL
#endif
#ifndef HTONS
  #define HTONS NTOHS
#endif

/**@}  // end of Endian Functions
 */
 
#endif //__ENDIAN_H__

