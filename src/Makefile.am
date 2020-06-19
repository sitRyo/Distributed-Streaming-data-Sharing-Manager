noinst_HEADERS =  ssm-coordinator.h

# set the include path found by configure
INCLUDES= $(all_includes) -I../include
DEBUG_FLAG = -g 

AM_CFLAGS   = -Wall $(DEBUG_FLAG)
AM_CXXFLAGS = $(AM_CFLAGS)

## libssm
lib_LTLIBRARIES = libssm.la

libssm_la_SOURCES = libssm-time.c libssm-shm.c libssm.c

libssm_la_CFLAGS = $(AM_CFLAGS) -Werror
libssm_la_CXXFLAGS = $(libssm_la_CFLAGS)
libssm_la_LIBADD = @LD_LIBRT@ -lpthread

#---------------------------------------------------
# ***_la_LDFLAGS = -version-info  CURRENT:REVISION:AGE
# 1. Bugfix                          C   : R++ : A
# 2. Add Interface                   C++ : R=0 : A++
# 3. Delete or Change Interface      C++ : R=0 : A=0

libssm_la_LDFLAGS = -version-info 5:1:5

## ssm-coordinator
bin_PROGRAMS = ssm-coordinator

ssm_coordinator_SOURCES = ssm-coordinator.cpp

ssm_coordinator_CFLAGS = $(AM_CFLAGS)
ssm_coordinator_CXXFLAGS = $(ssm_coordinator_CFLAGS)

ssm_coordinator_LDADD = libssm.la
