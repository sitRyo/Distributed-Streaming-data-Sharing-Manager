#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#*********************************************
# NOTICE
#---------------------------------------------
# if you change with "libssm",
# you MUST change
#    ***_la_LDFLAGS = -version-info C:R:A
# in src/Makefile.am
#*********************************************
AC_PREREQ(2.61)
AC_INIT([ssm], [1.0.6], [platform@roboken.esys.tsukuba.ac.jp])
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([src/libssm.c])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_INSTALL

# Checks for libraries.
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/types.h sys/ipc.h sys/shm.h sys/msg.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_STRTOD
#AC_CHECK_FUNCS([gettimeofday memset])

AC_CHECK_LIB( rt, clock_gettime )
LD_LIBRT=""
if test "x$HAVE_LIBRT" == "x1"; then
	LD_LIBRT="-lrt"
fi
AC_SUBST(LD_LIBRT)


AC_ARG_ENABLE([debug],
        AS_HELP_STRING([--enable-debug], [Enable all debugging message]),
        AC_DEFINE(DEBUG,, [Enable all debugging message]))

AC_CONFIG_FILES([
 Makefile
 include/Makefile
 src/Makefile
 utilities/Makefile
 samples/Makefile
 samples/C_Sample/Makefile
 samples/Cpp_Sample/Makefile
])

# utilities/Makefile
AC_ARG_WITH(ldconfig,AC_HELP_STRING([--disable-ldconfig],
	[don't execute ldconfig after install]))
if test "$enable_ldconfig" = "no"; then
	LDCONFIG="#ldconfig"
else
	AC_PATH_PROG(LDCONFIG, ldconfig, true, [/sbin /usr/sbin $PATH])
fi
AC_SUBST(LDCONFIG)

# Check for 64 bit
if test "x$prefix" = "xNONE" ; then
  prefix="/usr/local"
fi

case "$host" in
  x86_64*)
    CFLAGS=" -m64"
    libdir="$prefix/lib64"
    ;;
  i*86*)
    CFLAGS=" -m32"
    ;;
  *)
    ;;
esac

CFLAGS+=" -Wall -O2"
CXXFLAGS=$CFLAGS
AC_SUBST(CFLAGS)
AC_SUBST(CXXFLAGS)
AC_SUBST(libdir)

AC_OUTPUT
