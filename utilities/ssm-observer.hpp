/**
 * ssm-observer.hpp
 * 2020/5/7 R.Gunji
 */

#ifndef __INC_SSM_OBSERVER__

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

struct SSMApiInfo {
  SSMApiBase ssm_api_base;
  void* data;
  void* property;
  key_t shm_data_key; // int32_t
  key_t shm_property_key; // int32_t

  SSMApiInfo() = default;
  ~SSMApiInfo(); // destructorが必ず実行されるものではないことに注意(SIGINTとかだったっけ)
};

struct Subscriber {
  std::vector<std::string> api;
  pid_t pid;
  bool isSubscribe;
  // 条件のメンバ(std::function)
  Subscriber() {};
  explicit Subscriber(pid_t const& _pid) : pid(_pid), isSubscribe(false) {}
};

class SSMObserver {
  int msq_id;
  int pid;
  key_t shm_key_num;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t padding_size;

  char buf[OBSV_MSG_SIZE];

  std::unordered_map<std::string, std::shared_ptr<SSMApiInfo>> api_map; // TODO: unique_ptrにできるか検討
  std::unordered_map<pid_t, std::shared_ptr<Subscriber>> subscriber_map;

  int32_t get_shared_memory(uint32_t const& size, void** data);
  bool serialize_raw_data(uint32_t const& size, void* data);
  bool serialize_4byte_data(int32_t data);
  int recv_msg();
  int send_msg(OBSV_msg_type const& type, pid_t const& s_pid);
  bool allocate_obsv_msg();
  void format_obsv_msg();
  bool create_subscriber(pid_t const& pid);
  std::vector<std::string> extract_subscriber_from_msg();
  bool register_subscriber(pid_t const& pid, std::vector<std::string>& name);
  bool api_open(std::vector<std::string> const& name);
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
