/*
 MB95560 Series I/O register declaration file V01L02
 ALL RIGHTS RESERVED, COPYRIGHT (C) FUJITSU SEMICONDUCTOR LIMITED 2010
 LICENSED MATERIAL - PROGRAM PROPERTY OF FUJITSU SEMICONDUCTOR LIMITED
*/

#if defined(__CPU_MB95F562H__) || \
    defined(__CPU_MB95F562K__) || \
    defined(__CPU_MB95F563H__) || \
    defined(__CPU_MB95F563K__) || \
    defined(__CPU_MB95F564H__) || \
    defined(__CPU_MB95F564K__) 
#ifdef __FASM__
#include "mb95560_a.inc"
#else
#include "mb95560.h"
#endif
#else
#error "The I/O register file of the specified CPU option does not exist"
#endif
