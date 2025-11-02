dnl These m4 macros are whitespace sensitive and break if moved around much.
m4_define([LU_VERSION_H], m4_include([libusb/version.h]))
m4_define([LU_DEFINE_VERSION_ATOM],
	[m4_define([$1], m4_bregexp(LU_VERSION_H,
	[^#define\s*$1\s*\([0-9]*\).*], [\1]))])
m4_define([LU_DEFINE_VERSION_RC_ATOM],
	[m4_define([$1], m4_bregexp(LU_VERSION_H,
	[^#define\s*$1\s*"\(-rc[0-9]*\)".*], [\1]))])
dnl The m4_bregexp() returns (only) the numbers following the #define named
dnl in the first macro parameter. m4_define() then defines the name for use
dnl in AC_INIT.

LU_DEFINE_VERSION_ATOM([LIBUSB_MAJOR])
LU_DEFINE_VERSION_ATOM([LIBUSB_MINOR])
LU_DEFINE_VERSION_ATOM([LIBUSB_MICRO])
LU_DEFINE_VERSION_RC_ATOM([LIBUSB_RC])

AC_PREREQ([2.69])
AC_INIT([libusb-1.0], [LIBUSB_MAJOR[.]LIBUSB_MINOR[.]LIBUSB_MICRO[]LIBUSB_RC], [libusb-devel@lists.sourceforge.net], [libusb-1.0], [http://libusb.info])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([libusb/core.c])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_PROG_CXX
AC_C_INLINE
AM_INIT_AUTOMAKE
LT_INIT
LT_LANG([Windows Resource])

dnl Library versioning
dnl These numbers should be tweaked on every release. Read carefully:
dnl http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
dnl http://sourceware.org/autobook/autobook/autobook_91.html
lt_current=3
lt_revision=0
lt_age=3
LT_LDFLAGS="-version-info ${lt_current}:${lt_revision}:${lt_age} -no-undefined"

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

EXTRA_CPPFLAGS=
EXTRA_CFLAGS=

dnl check for -std=gnu11 compiler support (optional)
dnl note that we don't just check if the compiler accepts '-std=x11'
dnl but also that it supports the _Thread_local keyword because some compilers
dnl (e.g. gcc 4.8) accept the command line option but do not implement TLS
saved_CFLAGS="${CFLAGS}"
CFLAGS="-std=gnu11"
AC_MSG_CHECKING([if $CC supports -std=gnu11])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([_Thread_local int x;], [x = 42;])],
	[AC_MSG_RESULT([yes])
	 c_dialect=gnu],
	[AC_MSG_RESULT([no])
	 c_dialect=])
if test "x$c_dialect" != xgnu; then
	dnl fallback check for -std=c11 compiler support (required)
	CFLAGS="-std=c11"
	AC_MSG_CHECKING([if $CC supports -std=c11])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([_Thread_local int x;], [x = 42;])],
		[AC_MSG_RESULT([yes])],
		[AC_MSG_RESULT([no])
		 AC_MSG_ERROR([compiler with C11 support is required to build libusb])])
	c_dialect=c
fi
CFLAGS="${saved_CFLAGS}"

AC_DEFINE([_GNU_SOURCE], [1], [Enable GNU extensions.])
AC_DEFINE([DEFAULT_VISIBILITY], [__attribute__ ((visibility ("default")))], [Define to the attribute for default visibility.])
AC_DEFINE([PRINTF_FORMAT(a, b)], [__attribute__ ((__format__ (__printf__, a, b)))], [Define to the attribute for enabling parameter checks on printf-like functions.])

create_import_lib=
is_android_linux=
AC_MSG_CHECKING([operating system])
case $host in
*-darwin*)
	AC_MSG_RESULT([Darwin/Mac OS X])
	backend=darwin
	platform=posix
	;;
*-haiku*)
	AC_MSG_RESULT([Haiku])
	backend=haiku
	platform=posix
	;;
wasm32-**)
	AC_MSG_RESULT([Emscripten])
	backend=emscripten
	platform=posix
	;;
*-linux* | *-uclinux*)
	dnl on Android Linux, some functions are in different places
	case $host in
	*-linux-android*)
		AC_MSG_RESULT([Android Linux])
		is_android_linux=yes
		;;
	*)
		AC_MSG_RESULT([Linux])
		;;
	esac
	backend=linux
	platform=posix
	;;
*-netbsd*)
	AC_MSG_RESULT([NetBSD])
	backend=netbsd
	platform=posix
	;;
*-openbsd*)
	AC_MSG_RESULT([OpenBSD])
	backend=openbsd
	platform=posix
	;;
*-solaris*)
	AC_MSG_RESULT([SunOS])
	backend=sunos
	platform=posix
	;;
*-cygwin*)
	AC_MSG_RESULT([Windows (using Cygwin)])
	backend=windows
	platform=windows
	EXTRA_CFLAGS="-mwin32"
	;;
*-mingw* | *msys*)
	AC_MSG_RESULT([Windows])
	backend=windows
	platform=windows
	test "x$enable_shared" = xyes && create_import_lib=yes
	EXTRA_CFLAGS="-fno-omit-frame-pointer"
	EXTRA_LDFLAGS="-static-libgcc"
	;;
*)
	AC_MSG_RESULT([Null])
	AC_MSG_WARN([The host being compiled for is not supported.])
	AC_MSG_WARN([The library may compile but will not function in any useful manner.])
	backend=null
	platform=posix
	;;
esac

if test "x$platform" = xposix; then
	AC_DEFINE([PLATFORM_POSIX], [1], [Define to 1 if compiling for a POSIX platform.])
	AC_CHECK_TYPES([nfds_t], [], [], [[#include <poll.h>]])
	if test "x$backend" != xemscripten; then
		# pipe2 is detected as present on Emscripten, but it isn't actually ported and always
		# returns an error. https://github.com/emscripten-core/emscripten/issues/14824
		AC_CHECK_FUNCS([pipe2])
	fi
	dnl Some compilers do not support the '-pthread' option so check for it here
	saved_CFLAGS="${CFLAGS}"
	CFLAGS="-Wall -Werror -pthread"
	AC_MSG_CHECKING([if $CC recognizes -pthread])
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
		[AC_MSG_RESULT([yes])
		 AC_SUBST(THREAD_CFLAGS, [-pthread])],
		[AC_MSG_RESULT([no])])
	CFLAGS="${saved_CFLAGS}"
	dnl Android Linux and Darwin provide pthread functions directly in libc
	dnl glibc also provides some pthread functions directly, so search for a thread-specific function
	AC_SEARCH_LIBS([pthread_create], [pthread],
		[test "x$ac_cv_search_pthread_create" != "xnone required" && AC_SUBST(THREAD_LIBS, [-lpthread])],
		[], [])
	dnl Check for new-style atomic builtins. We first check without linking to -latomic.
	AC_MSG_CHECKING(whether __atomic_load_n is supported)
	AC_LINK_IFELSE([AC_LANG_SOURCE([[
	#include <stdint.h>
	int main() {
		struct {
			uint64_t *v;
		} x;
		return (int)__atomic_load_n(x.v, __ATOMIC_ACQUIRE) &
		       (int)__atomic_add_fetch(x.v, (uint64_t)1, __ATOMIC_ACQ_REL);
	}]])], GCC_ATOMIC_BUILTINS_SUPPORTED=yes, GCC_ATOMIC_BUILTINS_SUPPORTED=no)
	AC_MSG_RESULT($GCC_ATOMIC_BUILTINS_SUPPORTED)
	if test "x$GCC_ATOMIC_BUILTINS_SUPPORTED" != xyes; then
		AC_SEARCH_LIBS([__atomic_fetch_add_4], [atomic])
	fi
elif test "x$platform" = xwindows; then
	AC_DEFINE([PLATFORM_WINDOWS], [1], [Define to 1 if compiling for a Windows platform.])
else
	AC_MSG_ERROR([Unknown platform])
fi

case $backend in
darwin)
	AC_CHECK_FUNCS([pthread_threadid_np])
	LIBS="${LIBS} -lobjc -Wl,-framework,IOKit -Wl,-framework,CoreFoundation -Wl,-framework,Security"
	AC_CHECK_HEADERS([IOKit/usb/IOUSBHostFamilyDefinitions.h])
	;;
haiku)
	LIBS="${LIBS} -lbe"
	;;
linux)
	AC_SEARCH_LIBS([clock_gettime], [rt], [], [], [])
	AC_CHECK_FUNCS([pthread_setname_np])
	AC_ARG_ENABLE([udev],
		[AS_HELP_STRING([--enable-udev], [use udev for device enumeration and hotplug support (recommended) [default=yes]])],
		[use_udev=$enableval], [use_udev=yes])
	if test "x$use_udev" = xyes; then
		dnl system has udev. use it or fail!
		AC_CHECK_HEADER([libudev.h], [], [AC_MSG_ERROR([udev support requested but libudev header not installed])])
		AC_CHECK_LIB([udev], [udev_new], [], [AC_MSG_ERROR([udev support requested but libudev not installed])])

		# We can build umockdev tests (if available)
		m4_ifdef([PKG_PROG_PKG_CONFIG],[
			PKG_PROG_PKG_CONFIG
		   	PKG_CHECK_MODULES([UMOCKDEV], [umockdev-1.0 >= 0.16.0], [ac_have_umockdev=yes], [ac_have_umockdev=no])
		   	PKG_CHECK_MODULES([UMOCKDEV_HOTPLUG], [umockdev-1.0 >= 0.17.7], [ac_umockdev_hotplug=yes], [ac_umockdev_hotplug=no])
			if test $ac_umockdev_hotplug = yes; then
			   AC_DEFINE([UMOCKDEV_HOTPLUG], [1], [UMockdev hotplug code is not racy])
			fi
		], [])
	else
		AC_CHECK_HEADERS([asm/types.h])
		AC_CHECK_HEADER([linux/netlink.h], [], [AC_MSG_ERROR([Linux netlink header not found])])
		AC_CHECK_HEADER([sys/socket.h], [], [AC_MSG_ERROR([Linux socket header not found])])
	fi
	;;
sunos)
	LIBS="${LIBS} -ldevinfo"
	;;
windows)
	AC_CHECK_TYPES([struct timespec], [], [], [[#include <time.h>]])
	AC_DEFINE([_WIN32_WINNT], [_WIN32_WINNT_VISTA], [Define to the oldest supported Windows version.])
	LT_LDFLAGS="${LT_LDFLAGS} -avoid-version"
	;;
emscripten)
	AC_SUBST(EXEEXT, [.html])
	# Note: LT_LDFLAGS is not enough here because we need link flags for executable.
	AM_LDFLAGS="${AM_LDFLAGS} --bind -s ASYNCIFY -s ASSERTIONS -s ALLOW_MEMORY_GROWTH -s INVOKE_RUN=0 -s EXPORTED_RUNTIME_METHODS=['callMain']"
	;;
*)
	dnl no special handling required
	;;
esac

dnl headers not available on all platforms but required on others
AC_CHECK_HEADERS([sys/time.h])

dnl check availability of clock_gettime(), except don't bother on Darwin, because the result is not used.
if test "x$platform" = xposix && test "x$backend" != xdarwin; then
	AC_CHECK_FUNCS([clock_gettime], [have_clock_gettime=yes], [AC_MSG_ERROR([clock_gettime() is required on this platform])])

	if test "x$have_clock_gettime" = xyes; then
		dnl the clock_gettime() function needs certain clock IDs defined
		AC_CHECK_DECL([CLOCK_MONOTONIC], [], [AC_MSG_ERROR([C library headers missing definition for CLOCK_MONOTONIC])], [[#include <time.h>]])
		dnl use the monotonic clock for condition variable timed waits if possible
		AC_CHECK_FUNCS([pthread_condattr_setclock], [need_clock_realtime=], [need_clock_realtime=yes])
		if test "x$need_clock_realtime" = xyes; then
			AC_CHECK_DECL([CLOCK_REALTIME], [], [AC_MSG_ERROR([C library headers missing definition for CLOCK_REALTIME])], [[#include <time.h>]])
		fi
	fi
fi

dnl eventfd support
if test "x$backend" = xlinux || test "x$backend" = xsunos; then
	AC_ARG_ENABLE([eventfd],
		[AS_HELP_STRING([--enable-eventfd], [use eventfd for signalling [default=auto]])],
		[use_eventfd=$enableval],
		[use_eventfd=auto])
	if test "x$use_eventfd" != xno; then
		AC_CHECK_HEADER([sys/eventfd.h], [eventfd_h=yes], [eventfd_h=])
		if test "x$eventfd_h" = xyes; then
			AC_CHECK_DECLS([EFD_NONBLOCK, EFD_CLOEXEC], [eventfd_h_ok=yes], [eventfd_h_ok=], [[#include <sys/eventfd.h>]])
			if test "x$eventfd_h_ok" = xyes; then
				AC_CHECK_FUNC([eventfd], [eventfd_ok=yes], [eventfd_ok=])
				if test "x$eventfd_ok" = xyes; then
					AC_DEFINE([HAVE_EVENTFD], [1], [Define to 1 if the system has eventfd functionality.])
				elif test "x$use_eventfd" = xyes; then
					AC_MSG_ERROR([eventfd() function not found; glibc 2.9+ required])
				fi
			elif test "x$use_eventfd" = xyes; then
				AC_MSG_ERROR([eventfd header not usable; glibc 2.9+ required])
			fi
		elif test "x$use_eventfd" = xyes; then
			AC_MSG_ERROR([eventfd header not available; glibc 2.9+ required])
		fi
	fi
	AC_MSG_CHECKING([whether to use eventfd for signalling])
	if test "x$use_eventfd" = xno; then
		AC_MSG_RESULT([no (disabled by user)])
	elif test "x$eventfd_h" != xyes; then
		AC_MSG_RESULT([no (header not available)])
	elif test "x$eventfd_h_ok" != xyes; then
		AC_MSG_RESULT([no (header not usable)])
	elif test "x$eventfd_ok" != xyes; then
		AC_MSG_RESULT([no (functions not available)])
	else
		AC_MSG_RESULT([yes])
	fi
fi

dnl timerfd support
if test "x$backend" = xlinux || test "x$backend" = xsunos; then
	AC_ARG_ENABLE([timerfd],
		[AS_HELP_STRING([--enable-timerfd], [use timerfd for timing [default=auto]])],
		[use_timerfd=$enableval],
		[use_timerfd=auto])
	if test "x$use_timerfd" != xno; then
		AC_CHECK_HEADER([sys/timerfd.h], [timerfd_h=yes], [timerfd_h=])
		if test "x$timerfd_h" = xyes; then
			AC_CHECK_DECLS([TFD_NONBLOCK, TFD_CLOEXEC], [timerfd_h_ok=yes], [timerfd_h_ok=], [[#include <sys/timerfd.h>]])
			if test "x$timerfd_h_ok" = xyes; then
				AC_CHECK_FUNC([timerfd_create], [timerfd_ok=yes], [timerfd_ok=])
				if test "x$timerfd_ok" = xyes; then
					AC_DEFINE([HAVE_TIMERFD], [1], [Define to 1 if the system has timerfd functionality.])
				elif test "x$use_timerfd" = xyes; then
					AC_MSG_ERROR([timerfd_create() function not found; glibc 2.9+ required])
				fi
			elif test "x$use_timerfd" = xyes; then
				AC_MSG_ERROR([timerfd header not usable; glibc 2.9+ required])
			fi
		elif test "x$use_timerfd" = xyes; then
			AC_MSG_ERROR([timerfd header not available; glibc 2.9+ required])
		fi
	fi
	AC_MSG_CHECKING([whether to use timerfd for timing])
	if test "x$use_timerfd" = xno; then
		AC_MSG_RESULT([no (disabled by user)])
	elif test "x$timerfd_h" != xyes; then
		AC_MSG_RESULT([no (header not available)])
	elif test "x$timerfd_h_ok" != xyes; then
		AC_MSG_RESULT([no (header not usable)])
	elif test "x$timerfd_ok" != xyes; then
		AC_MSG_RESULT([no (functions not available)])
	else
		AC_MSG_RESULT([yes])
	fi
fi

dnl Message logging
AC_ARG_ENABLE([log],
	[AS_HELP_STRING([--disable-log], [disable all logging])],
	[log_enabled=$enableval],
	[log_enabled=yes])
if test "x$log_enabled" != xno; then
	AC_DEFINE([ENABLE_LOGGING], [1], [Define to 1 to enable message logging.])
fi

AC_ARG_ENABLE([debug-log],
	[AS_HELP_STRING([--enable-debug-log], [start with debug message logging enabled [default=no]])],
	[debug_log_enabled=$enableval],
	[debug_log_enabled=no])
if test "x$debug_log_enabled" != xno; then
	AC_DEFINE([ENABLE_DEBUG_LOGGING], [1], [Define to 1 to start with debug message logging enabled.])
fi

AC_ARG_ENABLE([system-log],
	[AS_HELP_STRING([--enable-system-log], [output logging messages to the systemwide log, if supported by the OS [default=no]])],
	[system_log_enabled=$enableval],
	[system_log_enabled=no])
if test "x$system_log_enabled" != xno; then
	AC_DEFINE([USE_SYSTEM_LOGGING_FACILITY], [1], [Define to 1 to output logging messages to the systemwide log.])
	if test "x$backend" != xwindows && test "x$is_android_linux" != xyes; then
		dnl Check if syslog is available in standard C library
		AC_CHECK_HEADER([syslog.h], [syslog_h=yes], [syslog_h=])
		if test "x$syslog_h" = xyes; then
			AC_CHECK_FUNCS([syslog])
		fi
	fi
fi

dnl Examples build
AC_ARG_ENABLE([examples-build],
	[AS_HELP_STRING([--enable-examples-build], [build example applications [default=no]])],
	[build_examples=$enableval],
	[build_examples=no])

dnl Tests build
AC_ARG_ENABLE([tests-build],
	[AS_HELP_STRING([--enable-tests-build], [build test applications [default=no]])],
	[build_tests=$enableval],
	[build_tests=no])

AM_CONDITIONAL([BUILD_EXAMPLES], [test "x$build_examples" != xno])
AM_CONDITIONAL([BUILD_TESTS], [test "x$build_tests" != xno])
AM_CONDITIONAL([BUILD_UMOCKDEV_TEST], [test "x$ac_have_umockdev" = xyes -a "x$log_enabled" != xno])
AM_CONDITIONAL([CREATE_IMPORT_LIB], [test "x$create_import_lib" = xyes])
AM_CONDITIONAL([OS_DARWIN], [test "x$backend" = xdarwin])
AM_CONDITIONAL([OS_HAIKU], [test "x$backend" = xhaiku])
AM_CONDITIONAL([OS_LINUX], [test "x$backend" = xlinux])
AM_CONDITIONAL([OS_NETBSD], [test "x$backend" = xnetbsd])
AM_CONDITIONAL([OS_NULL], [test "x$backend" = xnull])
AM_CONDITIONAL([OS_OPENBSD], [test "x$backend" = xopenbsd])
AM_CONDITIONAL([OS_SUNOS], [test "x$backend" = xsunos])
AM_CONDITIONAL([OS_WINDOWS], [test "x$backend" = xwindows])
AM_CONDITIONAL([OS_EMSCRIPTEN], [test "x$backend" = xemscripten])
AM_CONDITIONAL([PLATFORM_POSIX], [test "x$platform" = xposix])
AM_CONDITIONAL([PLATFORM_WINDOWS], [test "x$platform" = xwindows])
AM_CONDITIONAL([USE_UDEV], [test "x$use_udev" = xyes])

dnl The -Wcast-function-type warning causes a flurry of warnings when compiling
dnl Windows with GCC 8 or later because of dynamically loaded functions
if test "x$backend" = xwindows; then
	saved_CFLAGS="${CFLAGS}"
	CFLAGS="-Werror -Wcast-function-type"
	AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
		[EXTRA_CFLAGS="${EXTRA_CFLAGS} -Wno-cast-function-type"],
		[])
	CFLAGS="${saved_CFLAGS}"
fi

dnl Some linkers do not support the '--add-stdcall-alias' option so check for it here
if test "x$backend" = xwindows; then
	saved_CFLAGS="${CFLAGS}"
	CFLAGS="-Wl,--add-stdcall-alias"
	AC_MSG_CHECKING([if linker supports --add-stdcall-alias])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
		[AC_MSG_RESULT([yes])
		 LT_LDFLAGS="${LT_LDFLAGS} -Wl,--add-stdcall-alias"],
		[AC_MSG_RESULT([no])])
	CFLAGS="${saved_CFLAGS}"
fi

SHARED_CFLAGS="-Wall -Wextra -Wshadow -Wunused -Wwrite-strings -Werror=format-security -Werror=implicit-function-declaration -Werror=implicit-int -Werror=init-self -Werror=missing-prototypes -Werror=strict-prototypes -Werror=undef -Werror=uninitialized"

AM_CPPFLAGS="${EXTRA_CPPFLAGS}"
AC_SUBST(AM_CPPFLAGS)

AM_CFLAGS="-std=${c_dialect}11 ${EXTRA_CFLAGS} ${SHARED_CFLAGS}"
AC_SUBST(AM_CFLAGS)

AM_CXXFLAGS="-std=${c_dialect}++11 ${EXTRA_CFLAGS} ${SHARED_CFLAGS} -Wmissing-declarations"
AC_SUBST(AM_CXXFLAGS)

AC_SUBST(LT_LDFLAGS)
AC_SUBST(AM_LDFLAGS)

AC_SUBST([EXTRA_LDFLAGS])

dnl set name of html output directory for doxygen
AC_SUBST(DOXYGEN_HTMLDIR, [api-1.0])

AC_CONFIG_FILES([libusb-1.0.pc])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([libusb/Makefile])
AC_CONFIG_FILES([examples/Makefile])
AC_CONFIG_FILES([tests/Makefile])
AC_CONFIG_FILES([doc/Makefile])
AC_CONFIG_FILES([doc/doxygen.cfg])
AC_OUTPUT
