/**
 * ssm-subscriber.hpp
 * 2020/5/7 R.Gunji
 */

#ifndef __INC_SSM_SUBSCIBER__
#define __INC_SSM_SUBSCIBER__

#include <unistd.h>
#include <libssm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <tuple>
#include <utility>
#include <iostream>

#include <cstring>
#include <cerrno>
#include <cstdlib>

#include "dssm-utility.hpp"
#include "observer-util.hpp"
#include "PipeConnector.hpp"

int32_t verbose_mode = 1;

using namespace ssm;
using dssm::util::hexdump;

/**
 * @brief ローカル条件が無いに使用するラムダ式
 */
std::function<bool()> no_local_cond = [](){ return true; };

struct ShmInfo {
  void* data;
  void* property;
  key_t shm_data_key;
  key_t shm_property_key;
  std::string stream_name;
  int32_t stream_id;
  uint32_t data_size;
  uint32_t property_size;
  std::string ip_address;
  double time;
  double tid;

  ShmInfo(Stream const& _stream) : 
  data(nullptr), property(nullptr), stream_name(_stream.stream_name), stream_id(_stream.stream_id), data_size(_stream.data_size), property_size(_stream.property_size), ip_address(_stream.ip_address)
  {}
};

struct SubscriberSet {
  // 購読するストリーム名
  ssm_api_pair stream_info;
  
  // 条件
  OBSV_cond_type command;

  // トリガーか否か (このSubscriber_setが更新されたら他のSubscriber_setもデータを取得するという意味)
  OBSV_cond_type command_trigger;

  // トリガーではないSubscriberSetが基準とするストリーム
  ssm_api_pair observed_stream;

  // ip address
  std::string ip_address;

  SubscriberSet() {}
  SubscriberSet(Stream const& _stream, OBSV_cond_type const _command, OBSV_cond_type const _command_trigger, ssm_api_pair _observed_stream = { "\0", 0 })
    : stream_info({_stream.stream_name, _stream.stream_id}), command(_command), command_trigger(_command_trigger), observed_stream(_observed_stream), ip_address(_stream.ip_address) {} 
};

class SubscriberBase {
protected:
  // stream情報
  std::vector<SubscriberSet> subscriber_set;

  // serial number
  uint32_t serial_number;

  // time, tidが入ったHashMap
  ssm_api_pair_map& ssm_shm_data_info;

  SubscriberBase(
    std::vector<SubscriberSet> const& _subscriber_set,
    uint32_t _serial_number,
    ssm_api_pair_map& _api_pair_map
  ) : subscriber_set(_subscriber_set), serial_number(_serial_number), ssm_shm_data_info(_api_pair_map)
  {}

public:
  
  /**
   * @brief callbackを実行する。
   */
  virtual void invoke() {};
  
  /**
   * @brief serial番号を取得
   */
  inline uint32_t get_serial_number() {
    return serial_number;
  }

  /**
   * @brief stream情報を取得
   */
  inline std::vector<SubscriberSet> get_subscriber_set() {
    return subscriber_set;
  }
  
  /**
   * @brief 時刻データ・APIデータをセット
   */
  inline void set_time_and_tid(ssm_api_pair const api_pair, SSMShmDataInfo const shm_data) {
    if (this->ssm_shm_data_info.find(api_pair) != ssm_shm_data_info.end()) {
      this->ssm_shm_data_info[api_pair] = shm_data;
    } else {
      this->ssm_shm_data_info.insert({api_pair, shm_data});
    }
  }
};

/**
 * @brief テンプレートに使う型をdata, propertyの順番で入れる。ex) dsm_gl, dsm_property など 
 */
template <typename ... Strm>
class Subscriber : public SubscriberBase {
  // callback
  std::function<void(Strm...)> callback;

  // local condition
  std::function<bool()> local_condition;

  // ShmInfoの共有ポインタ
  std::vector<std::shared_ptr<ShmInfo>> shm_info_ptr;

  // dataとpropertyの順でデータが入ったtuple
  std::tuple<std::add_pointer_t<Strm>...> data_property_tpl;

  // shm_info_ptrから作るshm_infoのvoid*が入ったvector
  std::vector<void *> shm_info;

  // invokeされたのは初めてか？
  bool is_invoke_first;

  /**
   * @brief shm_info_ptrからdata, propertyを抽出してvectorを作成
   */
  void create_data_vector() {
    for (auto shm : shm_info_ptr) {
      shm_info.push_back(shm->data);
      if (shm->property != nullptr) {
        shm_info.push_back(shm->property);
      }
    }
  }

public:

  Subscriber(
    std::vector<std::shared_ptr<ShmInfo>> _shm_info,
    std::function<void(Strm...)> _callback,
    std::function<bool()> _local_condition,
    uint32_t _serial_number,
    std::vector<SubscriberSet> const& _subscriber_set,
    ssm_api_pair_map& _api_pair_map
  ) : SubscriberBase(_subscriber_set, _serial_number, _api_pair_map), callback(_callback), local_condition(_local_condition), shm_info_ptr(_shm_info), is_invoke_first(false)
  {}

  void invoke() override {
    // もし作られてなかったら
    if (!is_invoke_first) {
      is_invoke_first = true;
      create_data_vector();
      this->data_property_tpl = vector_to_tuple<sizeof...(Strm)>(shm_info, data_property_tpl);
    }

    if (local_condition()) {
      apply(callback, this->data_property_tpl);
    }
  }
};

class SSMSubscriber {
  int msq_id;
  int subscriber_msq_id;
  pid_t pid;
  ssm_obsv_msg *obsv_msg;
  char* msg_body;
  std::unique_ptr<char[]> data_buffer;
  std::unique_ptr<PipeWriter> pipe_writer;
  std::unique_ptr<PipeReader> pipe_reader;
  uint32_t padding_size;
  std::vector<Stream> name;
  std::unordered_map<ssm_api_pair, std::shared_ptr<ShmInfo>, SSMApiHash, SSMApiEqual> shm_info_map;
  std::vector<std::unique_ptr<SubscriberBase>> subscriber;

  inline void format_obsv_msg() {
    memset((char *) data_buffer.get(), 0, OBSV_MSG_SIZE);
  }

  bool send_msg(OBSV_msg_type const type, int32_t const id) {
    obsv_msg->msg_type = OBSV_MSQ_CMD;
    obsv_msg->cmd_type = type;
    obsv_msg->pid      = pid;

    // hexdump(data_buffer.get(), 48);

    // dssm::util::hexdump(data_buffer.get(), 48);
    if (pipe_writer->write_pipe(data_buffer.get(), sizeof(ssm_obsv_msg) + obsv_msg->msg_size) <= 0) {
      perror("SSMSubscriber::send_msg");
      return false;
    }

    return true;
  }

  int recv_msg_sync() {
    int len;
    format_obsv_msg();
    while ((len = recv_msg()) <= 0) {
      if (len == -1 && errno != EAGAIN)  {
        perror("SSMSubscriber::recv_msg_sync");
        return -1;
      }
    }

    return len;
  }

  int recv_msg(int32_t const id = 0) {
    // messageをrecv
    int len = pipe_reader->read_pipe(data_buffer.get());
    if (len < 0) {
      if (errno != EAGAIN) {
        perror("SSMObserver::recv_msg");
      }

      return -1;
    }

    if (len == 0) {
      return 0;
    }

    return len;
  }

  bool allocate_obsv_msg() {
    data_buffer.reset(new char[OBSV_MSG_SIZE]);
    if (data_buffer == nullptr) {
      fprintf(stderr, "SSMObserver::allocate_obsv_msg: bad alloc.\n");
      return false;
    }

    this->obsv_msg = reinterpret_cast<ssm_obsv_msg*>(data_buffer.get());
    this->msg_body = reinterpret_cast<char*>(((char*) data_buffer.get()) + sizeof(ssm_obsv_msg));
    return true;
  }

  bool send_stream() {
    format_obsv_msg();
  
    for (auto&& element : name) {
      serialize_string(element.stream_name);
      serialize_4byte_data(element.stream_id);
      serialize_4byte_data(element.data_size);    
      serialize_4byte_data(element.property_size);
      serialize_string(element.ip_address);
    }

    if (!send_msg(OBSV_ADD_STREAM, msq_id)) {
      // Error handling?
      return false;
    }

    // 共有メモリ鍵データが帰ってくる
    format_obsv_msg();
    if (recv_msg_sync() < 0) {
      // Error handling?
      return false;
    }

    // register_shm_info(name);
    name.clear();
    return true;
  }

  /**
   * @brief subscriberのsizeを送信する。
   */
  bool send_subscriber_size() {
    format_obsv_msg();
    serialize_4byte_data(subscriber.size());
    // dssm::util::hexdump(data_buffer.get(), 40);
    send_msg(OBSV_SUBSCRIBE, msq_id);

    return recv_msg_sync() > 0;
  }

  /**
   * @brief subscriberの情報を送信する。
   * 1. serial_number
   * 2. 監視するSSMApiの数
   * 3. stream name送信
   * 4. stream id送信 (int32_t)
   * 5. commmand (int32_t)
   */
  bool send_subscriber_stream_info() {
    for (std::unique_ptr<SubscriberBase>& s_info : subscriber) {
      // Subscriberを1つずつ送信する。
      format_obsv_msg();

      // serial_number
      serialize_4byte_data(s_info->get_serial_number());

      // 1つのSubscriberが購読しているssmapiの情報と条件
      auto subscriber_set = s_info->get_subscriber_set();
      auto size = subscriber_set.size();
      // ssm_apiの数
      serialize_4byte_data(size);
      for (auto& sub_set : subscriber_set) {
        // stream_name
        serialize_string(sub_set.stream_info.first);
        // stream_id
        serialize_4byte_data(sub_set.stream_info.second);
        // stream_command(条件)
        serialize_4byte_data(sub_set.command);
        // triggerか否か
        serialize_4byte_data(sub_set.command_trigger);
        // ip_address
        // serialize_string(sub_set.ip_address);
        // もしcond_timeのときにどのストリームの時刻でデータを取得するかを送信する。
        if (sub_set.command_trigger == OBSV_COND_NO_TRIGGER) {
          serialize_string(sub_set.observed_stream.first);
          serialize_4byte_data(sub_set.observed_stream.second);
        }
      }

      send_msg(OBSV_SUBSCRIBE, msq_id);

      if (recv_msg_sync() < 0) {
        return false;
      }

      // 共有メモリ鍵を登録する。
      attach_shared_memory_to_api(subscriber_set);
    }

    // 全部の情報を送信終わった
    return true;
  }

  void attach_shared_memory_to_api(std::vector<SubscriberSet> const& subscriber_set) {
    char *buf = msg_body;
    for (auto& sub_set : subscriber_set) {
      auto data_key = deserialize_4byte(&buf);
      auto property_key = deserialize_4byte(&buf);
      auto shm_info = shm_info_map.at(sub_set.stream_info);
      
      if (shm_info->data == nullptr) {
        attach_shared_memory(&(shm_info->data), data_key);
      }
      
      // propertyはセットされない可能性がある。セットされていないときはproperty_keyは-1になる。
      if (property_key != -1 && shm_info->property == nullptr) {
        attach_shared_memory(&(shm_info->property), property_key);
      }
    }
  }

  /**
   * @brief subscriber情報を送信する。
   * 1. 何個Subscriberを作るか
   * 2. 1つのSubscriberずつ、StreamInfo, Command(int)ずつ送る。
   */
  bool send_subscriber() {
    format_obsv_msg();

    // Subscriberの数を送信
    send_subscriber_size();

    // Subscriberの情報を送信
    send_subscriber_stream_info();

    return true;
  }

  bool serialize_4byte_data(int32_t data) {
    // メッセージに書き込めないときはエラー(呼び出し元でerror handlingしないと(面倒))
    if (sizeof(int32_t) + obsv_msg->msg_size >= padding_size) {
      if (verbose_mode > 0) {
        fprintf(stderr, "VERBOSE: message body is too large.\n");
      }
      return false;
    }

    auto& size = obsv_msg->msg_size;
    // メッセージBodyの先頭から4byteにデータを書き込む。
    *reinterpret_cast<int32_t *>(&msg_body[size]) = data;
    // 埋めた分だけメッセージサイズを++
    size += 4;
    return true;
  }

  bool serialize_string(std::string const& str) {
    auto size = str.size();
    if (size + obsv_msg->msg_size >= padding_size) {
      if (verbose_mode > 0) {
        fprintf(stderr, "VERBOSE: message body is too large.\n");
      }
      return false;
    }

    for (auto&& ch : str) { msg_body[obsv_msg->msg_size++] = ch; }
    msg_body[obsv_msg->msg_size++] = '\0';

    return true;
  }

  void invoke(uint32_t serial_number) {
    subscriber.at(serial_number)->invoke();
  }

  void subscribe_loop() {
    printf("start msg loop\n");
    int len = -1;
    while (true) {
      len = recv_msg_sync();
      if (len < 0) {
        fprintf(stderr, "ERROR: msgrcv\n");
        return;
      }

      switch (obsv_msg->cmd_type) {
        // 通知
        case OBSV_NOTIFY: {
          char* buf = msg_body;

          auto serial_number = deserialize_4byte(&buf);
          auto ssm_api_count = deserialize_4byte(&buf);
          for (auto idx = 0; idx < ssm_api_count; ++idx) {
            auto stream_name = deserialize_string(&buf);
            auto stream_id = deserialize_4byte(&buf);
            auto tid = deserialize_4byte(&buf);
            double time = deserialize_double(&buf);
            subscriber.at(serial_number)->set_time_and_tid({stream_name, stream_id}, {time, tid});
          }
          invoke(serial_number);
        }
      }
    }
  }

public:
  SSMSubscriber(): msq_id(-1), padding_size(-1) {}

  /**
   * @brief Subscriberを初期化。Observer間にメッセージキューを作る。
   */
  bool init_subscriber() {
    // 区別のためのプロセスIDを取得
    pid = getpid();
    printf("mypid: %d\n", pid);
    allocate_obsv_msg();

    // server間のパイプと接続
    if (!init_client_pipe_writer()) {
      return false;
    }
    
    format_obsv_msg();
    send_msg(OBSV_INIT, msq_id);
    
    // clientパイプに接続
    if (!init_client_pipe_reader()) {
      return false;
    }

    if (recv_msg_sync() < 0) {
      return false;
    }

    printf("observer pid %d my pid %d\n", obsv_msg->pid, obsv_msg->msg_type);
    return true;
  }

  /**
   * @brief serverとの通信用パイプを作る
   */
  bool init_client_pipe_writer() {
    // serverとの通信
    this->pipe_writer = init_pipe_writer(false, false, SERVER_PIPE_NAME);
    
    return true;
  }

  /**
   * @brief client側のパイプを初期化する
   */
  bool init_client_pipe_reader() {
    // clientパイプ作成
    std::string client_path = CLIENT_PIPE_NAME + std::to_string(pid);
    this->pipe_reader = init_pipe_reader(true, true, client_path);

    return true;
  }

  /**
   * @brief Stream情報(Stream名, Stream ID)を登録
   */
  void add_stream(std::vector<Stream> const& api) {
    for (auto element : api) {
      // stream_id, stream_data, datasize, propertysizeでインスタンスを生成
      auto shm_info_ptr = std::make_shared<ShmInfo>(element);
      shm_info_map.insert({std::make_pair(element.stream_name, element.stream_id), shm_info_ptr});
    }

    name = api;    
  }

  /**
   * @brief Stream情報を送信
   */
  void stream_open() {
    // Stream情報を送信
    send_stream();
  }

  int subscriber_serial_number = 0;

  /**
  * @brief stream, callback, 条件を登録
  */
  template <class ...Args>
  bool register_subscriber(std::vector<SubscriberSet> const& subscriber_set, std::function<bool()>& local_condition, std::function<void(Args...)>& callback, ssm_api_pair_map& api_pair_map) {
    // vectorのインデックスとして使用する。
    // static auto serial_num = 0UL;
    std::vector<std::shared_ptr<ShmInfo>> sub_stream;

    // subscriberのうち1つはtriggerを持たなければいけない。
    bool has_trigger = false;
    
    for (auto& info : subscriber_set) {
      auto stream = info.stream_info;

      // トリガーを1つ以上持つ必要がある。
      // 1つ以上無い場合はobserverが通知をsubscriberに送信できない。
      if (info.command_trigger == OBSV_COND_TRIGGER) {
        has_trigger = true;
      }

      auto shm_info = shm_info_map.at(stream);
      if (shm_info == nullptr) {
        fprintf(stderr, "ERROR: ssm cannot find.\n");
        return false;
      }
      
      sub_stream.push_back(shm_info);
    }

    if (!has_trigger) {
      fprintf(stderr, "ERROR: No trigger.\n subscriber MUST have trigger.\n");
      return false;
    }

    std::unique_ptr<SubscriberBase> sub = std::make_unique<Subscriber<Args...>>(sub_stream, callback, local_condition, subscriber_serial_number, subscriber_set, api_pair_map);
    subscriber.push_back(std::move(sub));

    // serial_num++;
    subscriber_serial_number += 1;

    return true;
  }

  /**
   * @brief Subscriberをスタート
   */
  bool start() {
    printf("send subscriber\n");
    // Subscriber情報を送信
    send_subscriber();

    format_obsv_msg();    
    if (!send_msg(OBSV_START, msq_id)) {
      return false;
    }

    subscribe_loop();

    return true;
  }
};

#endif // _INC_SSM_SUBSCIBER_
