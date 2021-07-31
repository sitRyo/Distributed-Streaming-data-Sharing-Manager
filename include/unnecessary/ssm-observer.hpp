/**
 * ssm-observer.hpp
 * 2020/5/7 R.Gunji
 */

#ifndef __INC_SSM_OBSERVER__
#define __INC_SSM_OBSERVER__

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

#include "observer-util.hpp"
#include "ssm-proxy-client.hpp"
#include "PipeConnector.hpp"

using namespace ssm;

enum SSMApiType {
  SSM_API_BASE = 0,
  P_CONNECTOR,
};

struct SSMApiInfo {
  SSMApiType ssm_api_type;
  std::unique_ptr<SSMApiBase> ssm_api_base;
  std::unique_ptr<PConnector> p_connector;
  std::string stream_name;
  int32_t stream_id;
  std::unique_ptr<uint8_t[]> data;
  std::unique_ptr<uint8_t[]> property;
  uint32_t data_size;
  uint32_t property_size;
  ssmTimeT time;
  int32_t tid;
  std::string ip_address;

  SSMApiInfo();
  ~SSMApiInfo() = default;
  
  SSMApiInfo(const SSMApiInfo&) = default;
  SSMApiInfo& operator=(const SSMApiInfo&) = default;
  
  // SSMApiBaseがmoveを実装していないのでmove constructorはdelete

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
  inline void set_property();
  inline uint32_t get_serial_number();
  inline std::vector<SubscriberSet> get_other_subscriber_set();
  inline SubscriberSet get_trigger_subscriber_set();

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
  int32_t subscriber_msq_key;
  uint32_t count;
  uint32_t padding_size;
  std::vector<Subscriber> subscriber;
  std::unique_ptr<char[]> data_buffer;
  ssm_obsv_msg* obsv_msg;
  char* msg_body;
  std::unique_ptr<PipeWriter> pipe_writer;

  // 共有メモリ管理HashMap
  std::unordered_map<ssm_api_pair, SSMSharedMemoryInfo, SSMApiHash, SSMApiEqual> stream_buffer_map;
  std::unordered_map<ssm_api_pair, std::shared_ptr<SSMApiInfo>, SSMApiHash, SSMApiEqual> stream_info_map;

  void loop();
public:
  void* run(void* args) override;
  
  SubscriberHost(pid_t _pid, uint32_t _count);
  inline void set_subscriber(Subscriber const& subscriber);
  inline void set_subscriber(Subscriber&& subscriber);
  inline void set_count(int32_t const count);
  inline void set_stream_info_map_element(ssm_api_pair const& key, std::shared_ptr<SSMApiInfo> shm);
  inline void set_property_data();
  inline void set_pipe_writer(std::unique_ptr<PipeWriter>&& pipe_writer);
  inline std::shared_ptr<SSMApiInfo> get_stream_info_map_element(ssm_api_pair const& key);
  inline int32_t get_count();
  SSMSharedMemoryInfo get_shmkey(ssm_api_pair const& pair, uint32_t const data_size, uint32_t const property_size);
  bool send_msg(OBSV_msg_type const type, pid_t const& s_pid);
  bool recv_msg();
  bool serialize_4byte_data(int32_t data);
  bool serialize_double_data(double data);
  bool serialize_string(std::string const& data);
  bool serialize_subscriber_data(Subscriber& sub, int const serial_number);
};

/* Subscriber関連ここまで */

class SSMObserver {  
private:
  int pid; /* SSMObserverのpid */
  ssm_obsv_msg *obsv_msg; /* ipcでやり取りするメッセージのヘッダー */
  char *msg_body;
  uint32_t padding_size; /* もう使わない */
  std::unordered_map<pid_t, std::unique_ptr<SubscriberHost>> subscriber_map; /* subscriberとprocess id のhashmap */
  std::unique_ptr<PipeReader> pipe_reader; /* pipereader */
  std::unique_ptr<char[]> data_buffer; /* obsv_msgの後に続くデータを取得するためのバッファ */
  std::unordered_map<pid_t, std::unique_ptr<PipeWriter>> client_pipe_writer;

  bool serialize_raw_data(uint32_t const& size, void* data);
  bool serialize_4byte_data(int32_t data);
  
  // 4byte指定位置から取得するだけ。
  int32_t deserialize_4byte_data(char* buf);

  int recv_msg();
  int recv_msg_sync();
  int send_msg(OBSV_msg_type const& type, pid_t const& s_pid);
  bool allocate_obsv_msg();
  bool create_subscriber(pid_t const pid);
  //std::unique_ptr<PipeWriter> init_pipe_writer(bool const is_create, std::string const& pipe_name, int const mode = O_WRONLY);
  //std::unique_ptr<PipeReader> init_pipe_reader(bool const is_create, std::string const& pipe_name, int const mode = O_RDONLY);
  //std::string create_full_pipe_path(std::string const& name);

  std::vector<Stream> extract_stream_from_msg();
  uint32_t extract_subscriber_count();

  inline void instantiate_ssm_api_type(Stream const& stream, std::shared_ptr<SSMApiInfo> ssm_api_info);
  inline void api_set_buffer(std::shared_ptr<SSMApiInfo> ssm_api_info);
  inline void set_stream_data_to_ssm_api(Stream const& stream, std::shared_ptr<SSMApiInfo> ssm_api_info);

  bool register_stream(pid_t const& pid, std::vector<Stream> const& stream_data);
  bool register_subscriber(pid_t const& pid);
  bool create_ssm_api_info(Stream const& stream_data, pid_t const pid);
  void escape(int sig_num);
public:
  SSMObserver();
  ~SSMObserver();
  bool observer_init();
  void msq_loop();
};

#endif // __INC_SSM_OBSERVER__
