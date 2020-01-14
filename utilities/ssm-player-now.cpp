/**
 * 2020/1/14 R.Gunji
 * ssm-player-now.cpp
 * ログデータを解析してストリームを作成する。
 * ssm-playerと違って, ログデータの時間は読み飛ばす。
 */

#include <iostream>
#include <string>
#include <getopt.h>
#include "ssm-player-now.hpp"

using std::cout;
using std::cerr;
using std::endl;

bool SSMPlayerNow::optAnalyze(int aArgc, char **aArgv) {
  int opt, optIndex = 0; // optindexは使わないけど一応
  struct option longOpt[] = {
    {"network", required_argument, NULL, 'n'},
    {"local", no_argument, NULL, 'l'},
    {"ip", required_argument, NULL, 'i'},
    {0, 0, 0},
  };

  while((opt = getopt_long(aArgc, aArgv, "n:l:i:", longOpt, &optIndex)) != -1) {
    cout << optarg << endl;
    switch(opt) { 
      case 'n': this->streamLogArray.emplace_back(optarg); break;
      case 'i': this->streamLogArray.back().parser.setIpAddress(optarg); break;
      case 'l': this->streamLogArray.emplace_back(optarg); break;
      default: break; // do nothing?
    }
  }

  if (streamLogArray.empty()) {
    cerr << "USAGE: this program needs <LOGFILE> of dssm." << endl;
    return false;
  }

  return true;
}

bool SSMPlayerNow::streamCreate() {
  for (auto&& log : this->streamLogArray) {
    if (!log.parser.ipAddress().empty()) { // ipアドレスが設定されているならPconnector
      log.ssmApi.reset(
        new SSMApiBase(log.parser.streamName().c_str(), log.parser.streamId())
      );
      log.ssmApi->setBuffer(
        log.parser.data(), 
        log.parser.dataSize(), 
        log.parser.property(), 
        log.parser.propertySize()
      );
    } else { // それ以外SSMApi
      log.pCon.reset(
        new PConnector(log.parser.streamName().c_str(), log.parser.streamId())
      );
      log.fulldata = new char[sizeof(ssmTimeT) + log.parser.dataSize()];
      log.parser.setData(&(log.fulldata[8]));
      log.pCon->setBuffer(
        log.parser.data(), 
        log.parser.dataSize(), 
        log.parser.property(), 
        log.parser.propertySize(), 
        log.fulldata);
    }
  }

  return true;
}

// usage ./ssm-player-now -n <logfile> -i <ipadder> -l <logfile> -n <logfile> -i <ipaddr> ...
int main(int argc, char *argv[]) {
  SSMPlayerNow player;
  if (!player.optAnalyze(argc, argv)) {
    return 0;
  }


}