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

#include "observer-util.hpp"

using namespace ssm;

struct SSMApiInfo {
  SSMApiBase ssm_api_base;
  std::string stream_name;
  int32_t stream_id;
  std::unique_ptr<uint8_t[]> data;
  std::unique_ptr<uint8_t[]> property;
  uint32_t data_size;
  uint32_t property_size;
  ssmTimeT time;
  std::mutex mtx;

  int32_t tid;

  SSMApiInfo();
  ~SSMApiInfo() = default;
  
  // mutexはコピーできないのでcopy constructorはdelete
  SSMApiInfo(const SSMApiInfo&) = default;
  SSMApiInfo& operator=(const SSMApiInfo&) = default;
  // moveはいらない。

  int32_t read_last(int32_t opponent_tid, void* opponent_data_ptr);
  int32_t read_time(ssmTimeT const time, void* opponent_data_ptr);
  bool open(SSM_open_mode mode);
  bool setDataBuffer();
};

/* Subscriber */

struct SubscriberSet {
  std::shared_ptr<SSMApiInfo> ssm_api;
  std::shared_ptr<SSMApiInfo> observe_stream;
  int command; // 条件

  // SSMApiInfoが持つdata, propertyへのアクセス権限
  void* data;
  void* property;

  /* 条件など */
  int32_t top_tid;

  SubscriberSet();
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
  // std::vector<SubscriberSet> subscriber_set;
  SubscriberSet trigger_subscriber_set;
  std::vector<SubscriberSet> other_subscriber_set;
  uint32_t serial_number;
public:
  Subscriber(uint32_t _serial_number);
  ~Subscriber() = default;

  // copy
  Subscriber(const Subscriber&) = default;
  Subscriber& operator=(const Subscriber&) = default;

  // moveはdefault指定(noexcept)
  Subscriber(Subscriber&&) = default;
  Subscriber& operator=(Subscriber&&) = default;

  inline void set_trigger_subscriber(SubscriberSet const& subscriber_set);
  inline void set_other_subscriber(std::vector<SubscriberSet> const& subscriber_set);
  inline void set_other_subscriber(std::vector<SubscriberSet> && subscriber_set);
  inline uint32_t get_serial_number();

  int is_satisfy_condition();
  bool get_other_subscriber_data();
};

/**
 * @brief SSMの共有メモリデータ
 * property_sizeが0の場合, property = nullptr, property_key = -1
 */
struct SSMSharedMemoryInfo {
  void* data;
  void* property;
  key_t data_key;
  key_t property_key;
};

class SubscriberHost : public Thread {
  pid_t pid;
  int32_t msq_id;
  std::vector<Subscriber> subscriber;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t count;
  uint32_t padding_size;

  // 共有メモリ管理HashMap。メモリの解放方法を考えなければいけない。(順当にDestructorでやる？ctrl-cにどう対処するか。)
  std::unordered_map<ssm_api_pair, SSMSharedMemoryInfo, SSMApiHash, SSMApiEqual> stream_buffer_map;

  void loop();
public:
  void* run(void* args) override;
  
  SubscriberHost(pid_t _pid, uint32_t _count, uint32_t _padding, int32_t _msq_id);
  inline void set_subscriber(Subscriber const& subscriber);
  inline void set_subscriber(Subscriber&& subscriber);
  SSMSharedMemoryInfo get_shmkey(ssm_api_pair const& pair, uint32_t const data_size, uint32_t const property_size);
  bool send_msg(OBSV_msg_type const type, pid_t const& s_pid);
  bool recv_msg();
  bool serialize_4byte_data(int32_t data);
};

/* Subscriber関連ここまで */

class SSMObserver {  
private:
  int pid;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t padding_size;
  std::unordered_map<ssm_api_pair, std::shared_ptr<SSMApiInfo>, SSMApiHash, SSMApiEqual> api_map; // TODO: unique_ptrにできるか検討
  std::unordered_map<pid_t, std::unique_ptr<SubscriberHost>> subscriber_map;

  bool serialize_raw_data(uint32_t const& size, void* data);
  bool serialize_4byte_data(int32_t data);
  
  // 4byte指定位置から取得するだけ。
  int32_t deserialize_4byte_data(char* buf);

  int recv_msg();
  int send_msg(OBSV_msg_type const& type, pid_t const& s_pid);
  bool allocate_obsv_msg();
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
  // void set_signal_handler(int sig_num);
  void show_msq_id();
};

#endif // __INC_SSM_OBSERVER__
