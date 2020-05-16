#!/bin/bash

## Makefileの更新からmake installまでの処理を行うスクリプト

SSMSOURCE="$HOME/Researchment/Distributed-Streaming-data-Sharing-Manager"

output_message() {
  printf "\n**************************\n"
  printf "\033[1m%s\033[m\n" "$(date '+%y/%m/%d %H:%M:%S')  "
  printf "\033[1;$1m%s\033[m\n" "$2"
  printf "**************************\n"
}

cd ${SSMSOURCE}

option=$1
if [[ $option = "-fixed" ]]; then
  autoheader
  aclocal
  automake -a -c
  sudo make distclean
  ./configure 
fi

if [[ $option = "-distclean" ]]; then
  sudo make distclean
  exit 0
fi

if [[ $option = "-clean" ]]; then
  sudo make clean
  exit 0
fi

if [[ $option = "-help" ]]; then
  echo "help" 
  echo "-fixed     :   Autotools, configure実行"
  echo "-clean     :   make cleanを実行"
  echo "-distclean :   make distcleanを実行"
  echo "-help      :   helpメッセージを表示"
  exit 0
fi



sudo make CFLAGS="-Wno-error" CXXFLAGS="-Wno-error"

## makeの正常終了コードは0なので、それ以外のときはエラー処理
answer=$?
if [ $answer -ne 0 ]; then
  output_message 31 "build failed"
  exit 0 # 途中終了
fi

sudo make install

output_message 36 "build finish"
