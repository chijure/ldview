#ifndef __TCDEFINES_H__
#define __TCDEFINES_H__

#ifndef NO
#define NO 0
#endif
#ifndef YES
#define YES (!NO)
#endif

#ifdef WIN32
// The following shouldn't be necessary here, but due to bugs in Microsoft's
// precompiled headers, it is.  The warning being disabled below is the one
// that warns about identifiers longer than 255 characters being truncated to
// 255 characters in the debug info.
#pragma warning(disable : 4786 4702)

#define _CRT_SECURE_NO_DEPRECATE
#define _CRT_NONSTDC_NO_DEPRECATE

#include <winsock2.h>
#include <crtdbg.h>

#undef min
#undef max

#pragma warning( disable : 4514 4127 )

#ifdef _BUILDING_TCFOUNDATION
#define TCExport __declspec(dllexport)
#elif defined _BUILDING_TCFOUNDATION_LIB || defined _TC_STATIC
#define TCExport
#else
#define TCExport __declspec(dllimport)
#endif

#else // WIN32

#define TCExport

#endif // WIN32

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

// NOTE: the following should be 1, 2, and 4 bytes each.  So on a 64-bit system,
// the following defines aren't appropriate, and something else needs to be
// substituted.

typedef unsigned char TCByte;
typedef unsigned char TCUChar;
typedef unsigned short TCUShort;
typedef unsigned int TCUInt;
typedef unsigned long TCULong;
typedef char TCChar;
typedef short TCShort;
typedef int TCInt;
typedef long TCLong;

// Define LDVIEW_DOUBLES to have LDView use doubles instead of floats.  Comment
// out the definition for floats.
//#define LDVIEW_DOUBLES

// I'm not sure if floats are 64 bits on a 64-bit system or not.  I know that
// TCFloat has to be 32 bits when LDVIEW_DOUBLES isn't defined in order for it
// to work.
#ifdef LDVIEW_DOUBLES
typedef double TCFloat;
#else // LDVIEW_DOUBLES
typedef float TCFloat;
#endif // LDVIEW_DOUBLES

// The following must always be defined to 32 bits.
typedef float TCFloat32;

#ifndef __THROW
#define __THROW
#endif //__THROW

#define TC_NO_UNICODE

#ifdef TC_NO_UNICODE
typedef char UCCHAR;
typedef char * UCSTR;
typedef const char * UCCSTR;
#define _UC(x) x
#else // TC_NO_UNICODE
typedef wchar_t UCCHAR;
typedef wchar_t * UCSTR;
typedef const wchar_t * UCCSTR;
#define _UC(x) L ## x
#endif // TC_NO_UNICODE

#endif // __TCDEFINES_H__
