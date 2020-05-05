# set the include path found by configure
INCLUDES= $(all_includes) -I../../include
DEBUG_FLAG = -g 

AM_CFLAGS   = -Wall -g3 $(DEBUG_FLAG)
AM_CXXFLAGS = $(AM_CFLAGS) -std=c++11 

# use shared library
AM_LDFLAGS = -L../../src ../../src/libssm.la -lm
# use static library
#AM_LDFLAGS = -L../src -lssm -lm -all-static

noinst_HEADERS = intSsm.h

noinst_PROGRAMS = ssmReadSample ssmWriteSample ssmPullSample

##ssmWriteSample, ssmReadSample
ssmWriteSample_SOURCES = ssmWriteSample.cpp
ssmReadSample_SOURCES =  ssmReadSample.cpp
ssmPullSample_SOURCES = ssmPullSample.cpp