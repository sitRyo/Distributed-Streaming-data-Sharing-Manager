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

  int32_t tid;

  SSMApiInfo();
  ~SSMApiInfo() = default;
  
  // mutexはコピーできないのでcopy constructorはdelete
  SSMApiInfo(const SSMApiInfo&) = default;
  SSMApiInfo& operator=(const SSMApiInfo&) = default;
  // moveはいらない。

  int32_t read_last(int32_t opponent_tid);
  bool open(SSM_open_mode mode);
  bool setDataBuffer();
};

/* Subscriber */

struct SubscriberSet {
  std::shared_ptr<SSMApiInfo> ssm_api;
  int command; // 条件

  /* 条件など */
  int32_t top_tid;

  SubscriberSet(std::shared_ptr<SSMApiInfo> const& _ssm_api, int _command);
  ~SubscriberSet() = default;

  // copyはdefalut指定
  SubscriberSet(SubscriberSet const&) = default;
  SubscriberSet& operator=(SubscriberSet const&) = default;

  // moveもdefault
  SubscriberSet(SubscriberSet&&) = default;
  SubscriberSet& operator=(SubscriberSet&&) = default;
};

class Subscriber {
  std::vector<SubscriberSet> subscriber_set;
  uint32_t serial_number;
public:
  bool good();
  Subscriber(uint32_t _serial_number);
  ~Subscriber() = default;

  // copy
  Subscriber(const Subscriber&) = default;
  Subscriber& operator=(const Subscriber&) = default;

  // moveはdefault指定(noexcept)
  Subscriber(Subscriber&&) = default;
  Subscriber& operator=(Subscriber&&) = default;

  inline void set_subscriber_set(std::vector<SubscriberSet> const& subscriber_set);
  inline void set_subscriber_set(std::vector<SubscriberSet> && subscriber_set);
  inline uint32_t get_serial_number();

  int is_satisfy_condition();
};

class SubscriberHost : public Thread {
  pid_t pid;
  int32_t msq_id;
  std::vector<Subscriber> subscriber;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t count;
  
  void loop();
public:
  // defined in Thread class
  // Thread::start();
  // Thread::wait();

  void* run(void* args) override;
  
  SubscriberHost(pid_t _pid, uint32_t _count, uint32_t _padding, int32_t _msq_id);
  inline void set_subscriber(Subscriber const& subscriber);
  inline void set_subscriber(Subscriber&& subscriber);
  bool send_msg(OBSV_msg_type const type, pid_t const& s_pid);
  bool recv_msg();
};

/* Subscriber関連ここまで */

class SSMObserver {  
private:
  int msq_id;
  int pid;
  key_t shm_key_num;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t padding_size;
  std::unordered_map<ssm_api_pair, std::shared_ptr<SSMApiInfo>, SSMApiHash, SSMApiEqual> api_map; // TODO: unique_ptrにできるか検討
  std::unordered_map<pid_t, std::unique_ptr<SubscriberHost>> subscriber_map;

  int32_t get_shared_memory(uint32_t const& size, void** data);
  bool serialize_raw_data(uint32_t const& size, void* data);
  bool serialize_4byte_data(int32_t data);
  
  // 4byte指定位置から取得するだけ。
  int32_t deserialize_4byte_data(char* buf);

  int32_t deserialize_4byte(char** buf);
  int64_t deserialize_8byte(char** buf);
  std::string deserialize_string(char** buf);
  // std::string deserialize_raw_data(char** buf); 実装するかも？

  int recv_msg();
  int send_msg(OBSV_msg_type const& type, pid_t const& s_pid);
  bool allocate_obsv_msg();
  void format_obsv_msg();
  bool create_subscriber(pid_t const& pid, uint32_t count);

  std::vector<Stream> extract_stream_from_msg();
  uint32_t extract_subscriber_count();

  bool register_stream(pid_t const& pid, std::vector<Stream> const& stream_data);
  bool register_subscriber(pid_t const& pid);
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
