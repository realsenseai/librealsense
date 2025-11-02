/* config.h.  Manual config for MSVC.  */

#ifndef _MSC_VER
#warn "msvc/config.h shouldn't be included for your development environment."
#error "Please make sure the msvc/ directory is removed from your build path."
#endif

/* Visual Studio 2013 or later is required */
#if(_MSC_VER < 1800)
#error "Visual Studio 2013 or later is required."
#endif

/* Visual Studio 2013 does not support __func__ */
#if(_MSC_VER < 1900)
#define __func__ __FUNCTION__
#endif

/* Visual Studio 2015 and later defines timespec */
#if(_MSC_VER >= 1900)
#define _TIMESPEC_DEFINED 1
#endif

/* Disable: warning C4127: conditional expression is constant */
#pragma warning(disable : 4127)
/* Disable: warning C4200: nonstandard extension used : zero-sized array in struct/union */
#pragma warning(disable : 4200)
/* Disable: warning C4201: nonstandard extension used : nameless struct/union */
#pragma warning(disable : 4201)
/* Disable: warning C4324: structure was padded due to __declspec(align()) */
#pragma warning(disable : 4324)
/* Disable: warning C4996: 'GetVersionExA': was declared deprecated */
#pragma warning(disable : 4996)

#if defined(_PREFAST_)
/* Disable "Banned API" errors when using the MS's WDK OACR/Prefast */
#pragma warning(disable : 28719)
/* Disable "The function 'InitializeCriticalSection' must be called from within a try/except block" */
#pragma warning(disable : 28125)
#endif

/* Define to the attribute for default visibility. */
#define DEFAULT_VISIBILITY /**/

/* Define to 1 to start with debug message logging enabled. */
/* #undef ENABLE_DEBUG_LOGGING */

/* Define to 1 to enable message logging. */
#define ENABLE_LOGGING 0

/* Define to 1 if compiling for a Windows platform. */
#define PLATFORM_WINDOWS 1

/* Define to the attribute for enabling parameter checks on printf-like
   functions. */
#define PRINTF_FORMAT(a, b) /**/

/* Define to 1 to output logging messages to the systemwide log. */
/* #undef USE_SYSTEM_LOGGING_FACILITY */
#define USE_SYSTEM_LOGGING_FACILITY 1
