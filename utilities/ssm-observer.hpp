/**
 * ssm-observer.hpp
 * 2020/5/7 R.Gunji
 */

#ifndef _INC_SSM_OBSERVER_
#define _INC_SSM_OBSERVER_

#include <libssm.h>
#include <ssm.hpp>
#include "Thread.hpp"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>

using ssm_api_pair = std::pair<std::string, int32_t>;

/* SSMApiをunordered_mapで使うためのユーザ定義構造体群 */

struct SSMApiHash {
  size_t operator()(ssm_api_pair const& p) const {
    auto h1 = std::hash<std::string>()(p.first);
    auto h2 = std::hash<std::int32_t>()(p.second);
    return h1 xor (h2 << 1);
  }
};

struct SSMApiEqual {
  bool operator()(ssm_api_pair const& p1, ssm_api_pair const& p2) const {
    return (p1.first == p2.first) and (p1.second == p2.second);
  }
};

/* ここまで */

struct Stream {
  std::string stream_name;
  int32_t stream_id;
  uint32_t data_size;
  uint32_t property_size;
};

struct SSMApiInfo {
  SSMApiBase ssm_api_base;
  std::string stream_name;
  int32_t stream_id;
  void* data;
  void* property;
  uint32_t data_size;
  uint32_t property_size;
  key_t shm_data_key; // int32_t
  key_t shm_property_key; // int32_t
  std::mutex mtx;

  SSMApiInfo();
  ~SSMApiInfo(); // destructorが必ず実行されるものではないことに注意(SIGINTとかだったっけ)

  bool read_last();
  bool open(SSM_open_mode mode);
  bool setDataBuffer();
};

struct Subscriber : public Thread {
  std::vector<std::shared_ptr<SSMApiInfo>> ssm_api;
  pid_t pid;
  bool isSubscribe;
  // 条件のメンバ(std::function)
  // std::vector<std::function> cond; みたいな
  Subscriber() {};
  explicit Subscriber(pid_t const& _pid) : pid(_pid), isSubscribe(false) {}
  void* run(void *args) override;
  void subscribe_loop();
};

class SSMObserver {  
private:
  int msq_id;
  int pid;
  key_t shm_key_num;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t padding_size;
  std::unordered_map<ssm_api_pair, std::shared_ptr<SSMApiInfo>, SSMApiHash, SSMApiEqual> api_map; // TODO: unique_ptrにできるか検討
  std::unordered_map<pid_t, std::unique_ptr<Subscriber>> subscriber_map;

  int32_t get_shared_memory(uint32_t const& size, void** data);
  bool serialize_raw_data(uint32_t const& size, void* data);
  bool serialize_4byte_data(int32_t data);
  int32_t deserialize_4byte_data(char* buf);
  int recv_msg();
  int send_msg(OBSV_msg_type const& type, pid_t const& s_pid);
  bool allocate_obsv_msg();
  void format_obsv_msg();
  bool create_subscriber(pid_t const& pid);
  std::vector<Stream> extract_subscriber_from_msg();
  bool register_subscriber(pid_t const& pid, std::vector<Stream> const& stream_data);
  bool create_ssm_api_info(Stream const& stream_data);
  void escape(int sig_num);
public:
  SSMObserver();
  ~SSMObserver();
  bool observer_init();
  void msq_loop();
  void set_signal_handler(int sig_num);
  void show_msq_id();
};

#endif // __INC_SSM_OBSERVER__
