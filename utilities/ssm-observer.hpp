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

#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <memory>

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
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t padding_size;

  char buf[OBSV_MSG_SIZE];

  std::unordered_map<std::string, std::shared_ptr<SSMApiBase>> api_map; // TODO: unique_ptrにできるか検討
  std::unordered_map<pid_t, std::shared_ptr<Subscriber>> subscriber_map;
public:
  SSMObserver();
  ~SSMObserver();
  bool observer_init();
  int recv_msg();
  int send_msg(OBSV_msg_type type, int const& body_size);
  bool allocate_obsv_msg();
  void format_obsv_msg();
  void msq_loop();
  bool create_subscriber(pid_t const& pid);

  void show_msq_id();
};

#endif // __INC_SSM_OBSERVER__
