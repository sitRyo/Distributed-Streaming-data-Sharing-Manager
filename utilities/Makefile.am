# set the include path found by configure
INCLUDES= $(all_includes) -I../include
DEBUG_FLAG = -g 

#AM_CFLAGS   = -Wall -std=c++11 -L/lib64
AM_CFLAGS   = -Wall -lpthread -L/lib64 $(DEBUG_FLAG)
AM_CXXFLAGS = $(AM_CFLAGS) -std=c++14

bin_PROGRAMS = lsssm ssm-logger ssm-player ssm-proxy ssm-transporter ssm-date ssm-monitor ssm-graph dssm-player ssm-observer
noinst_PROGRAMS = read_proxy_test write_proxy_test subscriber_test subscriber_test2
noinst_HEADERS = ssm-transporter.h printlog.hpp intSsm.h dssm-player.hpp

# shell scripts
#EXTRA_DIST = ssm-advance-player killssm psssm topssm
dist_bin_SCRIPTS = ssm-advance-player killssm psssm topssm

## lsssm
lsssm_SOURCES = lsssm.c
lsssm_LDADD = ../src/libssm.la

## ssm-logger
ssm_logger_SOURCES = ssm-logger.cpp
ssm_logger_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
ssm_logger_CXXFLAGS = $(ssm_logger_CFLAGS)
ssm_logger_LDADD = ../src/libssm.la -lm

## ssm-player
ssm_player_SOURCES = ssm-player.cpp ssm-proxy-client.cpp dssm-utility.cpp
ssm_player_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
ssm_player_CXXFLAGS = $(ssm_advance_player_CFLAGS)
ssm_player_LDADD = ../src/libssm.la -lm

## ssm-proxy
ssm_proxy_SOURCES = ssm-proxy.cpp dssm-utility.cpp
#ssm_proxy_SOURCES = ssm-proxy.cpp test.cpp
ssm_proxy_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 # -fsanitize=address
ssm_proxy_CXXFLAGS = $(ssm_proxy_CFLAGS)
ssm_proxy_LDADD = ../src/libssm.la -lm

## read_test
read_proxy_test_SOURCES = read_proxy_test.cpp ssm-proxy-client.cpp dssm-utility.cpp
read_proxy_test_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 # -fsanitize=address
read_proxy_test_CXXFLAGS = $(read_proxy_test_CFLAGS)
read_proxy_test_LDADD = ../src/libssm.la -lm

## write_test
write_proxy_test_SOURCES = write_proxy_test.cpp ssm-proxy-client.cpp dssm-utility.cpp 
write_proxy_test_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 # -fsanitize=address
write_proxy_test_CXXFLAGS = $(write_proxy_test_CFLAGS)
write_proxy_test_LDADD = ../src/libssm.la -lm

## ssm-transporter
ssm_transporter_SOURCES = ssm-transporter.c
ssm_transporter_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
ssm_transporter_CXXFLAGS = $(ssm_transporter_CFLAGS)
ssm_transporter_LDADD = ../src/libssm.la -lm -lpthread

## ssm-date
ssm_date_SOURCES = ssm-date.c
ssm_date_CFLAGS = $(AM_CFLAGS)
ssm_date_CXXFLAGS = $(ssm_date_CFLAGS)
ssm_date_LDADD = ../src/libssm.la

## ssm-monitor
ssm_monitor_SOURCES = ssm-monitor.c
ssm_monitor_CFLAGS = $(AM_CFLAGS)
ssm_monitor_CXXFLAGS = $(ssm_monitor_CFLAGS)
ssm_monitor_LDADD = ../src/libssm.la

## ssm-graph
ssm_graph_SOURCES = ssm-graph.cpp
ssm_graph_CFLAGS = $(AM_CFLAGS)
ssm_graph_CXXFLAGS = $(ssm_graph_CFLAGS)
ssm_graph_LDADD = ../src/libssm.la

## ssm-log-parser
## ssm_log_parser_SOURCES = ssm-log-parser.cpp ssm-proxy-client.cpp
## ssm_log_parser_CFLAGS = $(AM_CFLAGS)
## ssm_log_parser_CXXFLAGS = $(ssm_log_parser_CFLAGS) -std=c++14
## ssm_log_parser_LDADD = ../src/libssm.la

## dssm-player
dssm_player_SOURCES = dssm-player.cpp ssm-proxy-client.cpp dssm-utility.cpp
dssm_player_CFLAGS = $(AM_CFLAGS) 
dssm_player_CXXFLAGS = $(dssm_player_CFLAGS)
dssm_player_LDADD = ../src/libssm.la 

## ssm-observer
ssm_observer_SOURCES = ssm-observer.cpp ssm-proxy-client.cpp dssm-utility.cpp
ssm_observer_CFLAGS = $(AM_CFLAGS) 
ssm_observer_CXXFLAGS = $(ssm_observer_CFLAGS) -fsanitize=address -O0 -g
ssm_observer_LDADD = ../src/libssm.la 

## subscriber_test
subscriber_test_SOURCES = subscriber_test.cpp ssm-proxy-client.cpp dssm-utility.cpp
subscriber_test_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -O0 -fsanitize=address
subscriber_test_CXXFLAGS = $(subscriber_test_CFLAGS)
subscriber_test_LDADD = ../src/libssm.la -lm

## subscriber_test2
subscriber_test2_SOURCES = subscriber_test2.cpp ssm-proxy-client.cpp dssm-utility.cpp
subscriber_test2_CFLAGS = $(AM_CFLAGS) -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -O0 -fsanitize=address
subscriber_test2_CXXFLAGS = $(subscriber_test2_CFLAGS)
subscriber_test2_LDADD = ../src/libssm.la -lm