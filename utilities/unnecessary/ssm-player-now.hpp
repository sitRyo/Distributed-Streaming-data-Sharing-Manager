#ifndef __INC_SSM_PLAYER_NOW__
#define __INC_SSM_PLAYER_NOW__

#include <vector>
#include <memory>
#include <string>
#include <cassert>
#include "ssm-log-parser.hpp"
#include "ssm.hpp"
#include "ssm-proxy-client.hpp"

struct DataWriter {
  SSMLogParser parser;
  std::unique_ptr<SSMApiBase> ssmApi;
  std::unique_ptr<PConnector> pCon;
  char* fulldata;

  DataWriter() {}
  DataWriter(std::string src) : parser(src) {}
  DataWriter(std::string src, std::string ipAddr) : parser(src, ipAddr) {}

  ~DataWriter() {
    delete fulldata;
  }

  DataWriter(DataWriter&& rhs) {
    this->ssmApi = std::move(rhs.ssmApi);
    this->pCon = std::move(rhs.pCon);
    this->fulldata = rhs.fulldata;
    rhs.fulldata = nullptr;
  }

  bool write(ssmTimeT time = -1) {
    if (ssmApi != nullptr) { return ssmApi->write(time); }
    else if (pCon != nullptr) { return pCon->write(time); }

    assert(false);
    return false;
  }
};

class SSMPlayerNow {
private:
  std::vector<DataWriter> streamLogArray;
public:
  SSMPlayerNow() = default;
  ~SSMPlayerNow() = default;

  bool optAnalyze(int aArgc, char **aArgv);
  bool streamCreate();
  bool streamWrite();
};

#endif // __INC_SSM_PLAYER_NOW__