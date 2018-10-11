#!/bin/sh

CMDNAME=`basename $0`
if [ $# -ne 1 ]; then
	echo "Usage: $CMDNAME log-dir" 1>&2
	exit 1
fi

if [ -d $1 ]; then
	echo "ERROR: Directory $1 already exists."
else
	mkdir $1
	ssm-logger -n keyboard		-i 0 -l $1/keyboard.log &
	ssm-logger -n pws_motor		-i 0 -l $1/pws.log &
	ssm-logger -n odometry		-i 0 -l $1/odm.log &
	ssm-logger -n base_odometry	-i 0 -l $1/bsodm.log &
	ssm-logger -n urg			-i 0 -l $1/urg.log &
	ssm-logger -n us			-i 0 -l $1/us.log &
	ssm-logger -n bumper		-i 0 -l $1/bumper.log
fi
