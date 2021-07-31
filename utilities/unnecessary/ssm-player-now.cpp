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

  std::vector<std::string> opts;
  while((opt = getopt_long(aArgc, aArgv, "n:l:", longOpt, &optIndex)) != -1) {
    std::string temp(optarg);
    switch(opt) { 
      case 'n': {
        // error handlingはなし
        opts.emplace_back(optarg);        
        // this->streamLogArray.push_back(DataWriter(temp.substr(0, colom), temp.substr(colom + 1)));
        break;
      }
      case 'l': {
        opts.emplace_back(optarg);
        // this->streamLogArray.emplace_back(optarg);
        break;
      }
      default: break; // do nothing?
    }
  }

  for (auto itr : opts) {
    auto colom = itr.find_first_of(':');
    if (colom != std::string::npos) {
      this->streamLogArray.emplace_back((itr.substr(0, colom), itr.substr(colom + 1)));
    } else {
      this->streamLogArray.emplace_back(itr);
    }
  }

  if (streamLogArray.empty()) {
    cerr << "USAGE: this program needs <LOGFILE> of dssm." << endl;
    return false;
  }

  return true;
}

bool SSMPlayerNow::streamCreate() {
  for (auto& log : this->streamLogArray) {
    cout << log.parser.streamName() << endl;
    if (log.parser.ipAddress().empty()) { // ipアドレスが設定されているならPconnector
      log.ssmApi.reset(
        new SSMApiBase(log.parser.streamName().c_str(), log.parser.streamId())
      );
      cout << log.parser.dataSize() << endl;
      log.ssmApi->setBuffer(
        log.parser.data(), 
        log.parser.dataSize(), 
        log.parser.property(), 
        log.parser.propertySize()
      );
      log.ssmApi->create(
        calcSSM_life(log.parser.bufferNum(), log.parser.cycle()),
        log.parser.cycle()
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
        log.fulldata
      );
      log.pCon->create(
        calcSSM_life(log.parser.bufferNum(), log.parser.cycle()),
        log.parser.cycle()
      );
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

  player.streamCreate();
}