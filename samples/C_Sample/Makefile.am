# set the include path found by configure
INCLUDES= $(all_includes) -I../../include

AM_CFLAGS   = -Wall
AM_CXXFLAGS = $(AM_CFLAGS)

# use shared library
AM_LDFLAGS = -L../../src -lssm -lm
# use static library
#AM_LDFLAGS = -L../../src -lssm -lm -all-static

EXTRA_DIST = logger.sh

noinst_HEADERS = SensorA.h SensorB.h

noinst_PROGRAMS = SensorAHandler SensorAContinualRead SensorAIntervalRead SensorAPastRead \
                  SensorBHandler SensorABSync

SensorAHandler_SOURCES = SensorAHandler.c

SensorAContinualRead_SOURCES = SensorAContinualRead.c

SensorAIntervalRead_SOURCES = SensorAIntervalRead.c

SensorAPastRead_SOURCES = SensorAPastRead.c

SensorBHandler_SOURCES = SensorBHandler.c

SensorABSync_SOURCES = SensorABSync.c
