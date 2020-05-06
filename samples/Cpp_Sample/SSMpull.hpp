#ifndef __SSMPULL__

#include "ssm.hpp"
#include <vector>
#include <functional>
#include <string>
#include <map>
#include <memory>

struct ApiInfo {
  std::shared_ptr<SSMApiBase> ssmApiBase;
  const char* streamName;
  int streamId;
  void* storage;

  ApiInfo(const char* streamName, int streamId, void* _storage) : streamName(streamName), streamId(streamId), storage(_storage)
  {}

  void init() {
    this->ssmApiBase.reset(new SSMApiBase(streamName, streamId));
    setStorage(storage);
  }

  void setStorage(void* _storage) {
    this->storage = _storage;
  }

  bool open(SSM_open_mode mode) {
    return ssmApiBase->open(mode);
  }

  void close() {
    ssmApiBase->close();
  }
};

struct SSMApiInfo {
private:
  int index;
public:
  std::vector<ApiInfo> apiInfos;
  std::map<std::string, int> hashMap;

  SSMApiInfo() : index(0) {}

  void add(std::string name, void* alloc) {
    hashMap[name] = index++;
    ApiInfo apiInfo(name.c_str(), 0, alloc);
    apiInfos.push_back(apiInfo); // インスタンスを作ってから格納するとコピーとか起こりそうでメモリの位置とかでちょっと怖い。左辺値参照を要素で取ればいい？
    apiInfos.back().init();
  }

  template <typename S>
  void assign(std::string name) {
    auto& apiInfo = apiInfos[hashMap[name]];
    S * s = reinterpret_cast<S *>(apiInfo.storage);
    apiInfo.ssmApiBase->setBuffer(&s->data, sizeof(s->data), &s->property, sizeof(s->property));
  }

  ApiInfo getSSMApi(std::string name) {
    return apiInfos[hashMap[name]];
  }
};

struct ObserverList {
  std::vector<std::string> ssmApiList;
  std::function<bool(std::vector<std::string> const)> cond;
  std::function<bool(std::shared_ptr<SSMApiInfo>)> callback;
};

class Observer {
  std::shared_ptr<SSMApiInfo> ssmApiInfo;

public:
  Observer() = default;
  explicit Observer(std::shared_ptr<SSMApiInfo>& _ssmApiInfo) : ssmApiInfo(_ssmApiInfo) 
  {}
  void add() {

  }
};

#endif // __SSMPULL__