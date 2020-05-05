#!/bin/bash
SSMSOURCE="/home/seriru/Researchment/Distributed-Streaming-data-Sharing-Manager"

cd ${SSMSOURCE}
autoheader
aclocal
automake -a -c
./configure 
sudo make CFLAGS="-Wno-error" CXXFLAGS="-Wno-error"
sudo make install
printf "\n**************************\n"
printf "\033[1m%s\033[m\n" "$(date '+%y/%m/%d%H:%M:%S')  "
printf "\033[1;36m%s\033[m\n" "build finish"
printf "**************************\n"