/*
 * $Header$
 *
 * (c) STanda @ ARAnyM development team 2001-2003
 *
 * GPL
 */

#include "tools.h"

#if SIZEOF_VOID_P != 4 || DEBUG_NON32BIT

// the instance for the memptr <-> void* conversions
// usefull for systems where the sizeof(void*) != 4
NativeTypeMapper<void*> memptrMapper;

#endif  // SIZEOF_VOID_P != 4