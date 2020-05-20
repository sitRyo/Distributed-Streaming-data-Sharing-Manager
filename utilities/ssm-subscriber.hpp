/**
 * ssm-subscriber.hpp
 * 2020/5/7 R.Gunji
 */

#ifndef _INC_SSM_SUBSCIBER_
#define _INC_SSM_SUBSCIBER_

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

#include <cstring>
#include <cerrno>
#include <cstdlib>

int32_t verbose_mode = 1;

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

/**
 * @brief タプルを関数の引数に展開し, 関数を実行。
 */
template <class Fn, class Tuple, size_t... _Idx>
constexpr decltype(auto)
apply_impl(Fn&& f, Tuple&& t, std::index_sequence<_Idx...>) {
	return f(std::get<_Idx>(std::forward<Tuple>(t))...);
}

/**
 * @brief apply(f, t)でfにtupleを展開し実行する。apply_implへのアダプタ。c++14で動作可能。
 */
template <class Fn, class Tuple>
constexpr decltype(auto)  // この時点で戻り値はわからない
apply(Fn&& f, Tuple&& t) {
  using Indices 
		= std::make_index_sequence<
      std::tuple_size<std::remove_reference_t<Tuple>>::value
		>;
	return apply_impl(
		std::forward<Fn>(f),
		std::forward<Tuple>(t),
		Indices{}
	);
}

/**
 * @brief impl
 */
template <class... Args, std::size_t... Indices>
auto vector_to_tuple_impl(const std::vector<void *>& v, std::tuple<Args...> tpl, std::index_sequence<Indices...>) {
  return std::make_tuple(
    * (reinterpret_cast<typename std::tuple_element<Indices, decltype(tpl)>::type *> (v[Indices]))...
  );
}

/**
 * @brief vectorをtupleに変換する。
 */
template <std::size_t N, class... Args>
auto vector_to_tuple(const std::vector<void *>& v, std::tuple<Args...> tpl) {
  return vector_to_tuple_impl(v, tpl, std::make_index_sequence<N>());
}

/* ここまで */

struct Stream {
  std::string stream_name;
  int32_t stream_id;
  uint32_t data_size;
  uint32_t property_size;
};

struct ShmInfo {
  void* data;
  void* property;
  key_t shm_data_key;
  key_t shm_property_key;
  std::string stream_name;
  int32_t stream_id;
  uint32_t data_size;
  uint32_t property_size;

  ShmInfo(Stream const& stream) : 
  stream_name(stream.stream_name), stream_id(stream.stream_id),
  data_size(stream.data_size), property_size(stream.property_size) 
  {}
};

struct SubscriberSet {
  // 購読するストリーム名
  ssm_api_pair stream_info;
  
  // 条件
  int command;
};

class SubscriberBase {
protected:
  // stream情報
  std::vector<SubscriberSet> subscriber_set;

  // serial number
  uint32_t serial_number;

  SubscriberBase(std::vector<SubscriberSet>& _subscriber_set, uint32_t _serial_number) : subscriber_set(_subscriber_set), serial_number(_serial_number) 
  {}

public:
  
  /**
   * @brief callbackを実行する。
   */
  virtual void invoke() = 0;
  
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
  std::tuple<Strm...> data_property;

  // shm_info_ptrから作るshm_infoのvoid*が入ったvector
  std::vector<void *> shm_info;

  /**
   * @brief shm_info_ptrからdata, propertyを抽出してvectorを作成
   */
  void create_data_vector() {
    for (auto shm : shm_info_ptr) {
      shm_info.push_back(shm->data);
      shm_info.push_back(shm->property);
    }
  }

public:

  Subscriber(std::vector<std::shared_ptr<ShmInfo>> _shm_info, std::function<void(Strm...)> _callback, std::function<bool()> _local_condition, uint32_t _serial_number, SubscriberSet _subscriber_set) 
  : shm_info_ptr(_shm_info), local_condition(_local_condition), callback(_callback), SubscriberBase(_subscriber_set, _serial_number) {
    // data, propertyへのポインタvector作成
    create_data_vector();
    auto tpl = vector_to_tuple<sizeof...(Strm)>(shm_info, data_property);
  }

  void invoke() override {
    if (local_condition()) {
      apply(callback, data_property);
    }
  }
};

class SSMSubscriber {
  int msq_id;
  pid_t pid;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  
  uint32_t padding_size;
  std::vector<Stream> name;
  std::unordered_map<ssm_api_pair, std::shared_ptr<ShmInfo>, SSMApiHash, SSMApiEqual> shm_info_map;
  std::vector<SubscriberBase> subscriber;

  void format_obsv_msg() {
    obsv_msg->msg_type = 0;
    obsv_msg->res_type = 0;
    obsv_msg->cmd_type = 0;
    obsv_msg->pid      = 0;
    obsv_msg->msg_size = 0;
    memset(obsv_msg->body, 0, padding_size);
  }

  bool attach_shared_memory(void** data, int32_t s_id) {
    // attach
    if ((*data = static_cast<char*>(shmat(s_id, 0, 0))) == (void*) -1) {
      perror("shmat");
      fprintf(stderr, "ERROR: shmat\n");
      return false;
    }

    return true;
  }

  bool register_shm_info(std::vector<Stream> const& name) {
    auto sz = 0;
    auto key_t_sz = sizeof(key_t);
    for (auto&& element : name) {
      auto& _name = element.stream_name;
      auto& _id   = element.stream_id;
      if (sz + key_t_sz * 2 <= obsv_msg->msg_size) {
        auto& shm_info = shm_info_map.at({_name, _id});

        shm_info->shm_data_key = *reinterpret_cast<int32_t *>(&obsv_msg->body[sz]);
        shm_info->shm_property_key = *reinterpret_cast<int32_t *>(&obsv_msg->body[sz + key_t_sz]);
        
        sz += key_t_sz * 2;

        // 共有メモリをアタッチ
        attach_shared_memory(&shm_info->data, shm_info->shm_data_key);
        attach_shared_memory(&shm_info->property, shm_info->shm_property_key);
      } else {
        fprintf(stderr, "ERROR: msgsize is too small.\n");
        return false;
      }
    }

    return true;
  }

  bool send_msg(OBSV_msg_type const& type) {
    obsv_msg->msg_type = OBSV_MSQ_CMD;
    obsv_msg->cmd_type = type;
    obsv_msg->pid      = pid;

    if (msgsnd(msq_id, (void *) obsv_msg.get(), OBSV_MSG_SIZE, 0) < 0) {
      perror("msgsnd");
      fprintf(stderr, "msq send err\n");
      return false;
    }

    return true;
  }

  int recv_msg() {
    format_obsv_msg();
    int len = msgrcv(msq_id, obsv_msg.get(), OBSV_MSG_SIZE, pid, 0);
    if (obsv_msg->res_type == OBSV_FAIL) {
      fprintf(stderr, "ERROR: RESTYPE is OBSV_FAIL\n");
      return -1;
    }
    return len;
  }

  bool allocate_obsv_msg() {
    auto size = sizeof(ssm_obsv_msg);
    auto padding = OBSV_MSG_SIZE - size;
    obsv_msg.reset((ssm_obsv_msg *) malloc(sizeof(ssm_obsv_msg) + sizeof(char) * padding));
    if (!obsv_msg) {
      fprintf(stderr, "ERROR: memory allocate");
      return false;
    }
    padding_size = padding;
    return true;    
  }

  bool send_stream() {
    format_obsv_msg();
  
    for (auto&& element : name) {
      serialize_string(element.stream_name);
      serialize_4byte_data(element.stream_id);
      serialize_4byte_data(element.data_size);    
      serialize_4byte_data(element.property_size);
    }

    if (!send_msg(OBSV_ADD_STREAM)) {
      // Error handling?
      return false;
    }

    // 共有メモリ鍵データが帰ってくる
    format_obsv_msg();
    if (!recv_msg()) {
      // Error handling?
      return false;
    }

    register_shm_info(name);
    name.clear();
    return true;
  }

  /**
   * @brief subscriberのsizeを送信する。
   */
  bool send_subscriber_size() {
    format_obsv_msg();
    serialize_4byte_data(subscriber.size());
    send_msg(OBSV_SUBSCRIBE);

    return recv_msg() > 0;
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
    for (SubscriberBase& s_info : subscriber) {
      // Subscriberを1つずつ送信する。
      format_obsv_msg();

      // serial_number
      serialize_4byte_data(s_info.get_serial_number());

      // 1つのSubscriberが購読しているssmapiの情報と条件
      auto subscriber_set = s_info.get_subscriber_set();
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
      }

      send_msg(OBSV_SUBSCRIBE);

      if (recv_msg() < 0) {
        return false;
      }
    }

    // 全部の情報を送信終わった
    return true;
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

  /**
 * @brief ポインタの先頭から4byte分取得。アドレスも4byte分進める。
 */
  int32_t deserialize_4byte(char** buf) {
    int32_t res = *reinterpret_cast<int32_t *>(*buf); 
    *buf += 4;
    return res;
  }

  /**
   * @brief ポインタの先頭から8byte分取得。アドレスも8byte分進める。
   */
  int64_t deserialize_8byte(char** buf) {
    int64_t res = *reinterpret_cast<int64_t *>(*buf); 
    *buf += 8;
    return res;
  }

  /**
   * @brief ポインタの先頭からnull文字が来るまでデータを文字列として取得。アドレスも進める。
   */
  std::string deserialize_string(char** buf) {
    std::string data;
    while (**buf != '\0') {
      data += **buf;
      *buf += 1;
    }
    
    // null文字分を飛ばす
    *buf += 1;

    return data;
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
    *reinterpret_cast<int32_t *>(&obsv_msg->body[size]) = data;
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

    for (auto&& ch : str) { obsv_msg->body[obsv_msg->msg_size++] = ch; }
    obsv_msg->body[obsv_msg->msg_size++] = '\0';

    return true;
  }
  
public:
  SSMSubscriber(): msq_id(-1), padding_size(-1) {}

  bool init_subscriber() {
    if ((msq_id = msgget(MSQ_KEY_OBS, 0666)) < 0) {
      fprintf(stderr, "msgque cannot open.\n");
      return false;
    }

    // 区別のためのプロセスIDを取得
    pid = getpid();
    printf("mypid: %d\n", pid);
    allocate_obsv_msg();
    
    send_msg(OBSV_INIT);

    if (recv_msg() < 0) {
      if (errno == E2BIG) {
        fprintf(stderr, "ERROR: msg size is too large.\n");
      }
      return false;
    }

    printf("observer pid: %d\n", obsv_msg->pid);

    return true;
  }

  void add_stream(std::vector<Stream> const& api) {
    for (auto element : api) {
      // stream_id, stream_data, datasize, propertysizeでインスタンスを生成
      auto shm_info_ptr = std::make_shared<ShmInfo>(element);
      shm_info_map.insert({std::make_pair(element.stream_name, element.stream_id), shm_info_ptr});
    }

    name = api;    
  }

  bool start() {
    // Stream情報を送信
    send_stream();

    // Subscriber情報を送信
    send_subscriber();

    format_obsv_msg();    
    if (!send_msg(OBSV_START)) {
      return false;
    }
    
    if (recv_msg() < 0) {
      return false;
    }

    printf("start\n");

    msq_loop();
  }

  void msq_loop() {
    int len = -1;
    while (true) {
      len = recv_msg();
      if (len < 0) {
        fprintf(stderr, "ERROR: msgrcv\n");
        return;
      }

      switch (obsv_msg->cmd_type) {
        // 通知
        case OBSV_NOTIFY: {
          char*  tmp = obsv_msg->body;
          char** buf = &tmp;

          auto serial_number = deserialize_4byte(buf);
          invoke(serial_number);
        }
      }
    }
  }

  void invoke(uint32_t serial_number) {
    subscriber.at(serial_number).invoke();
  }

  void access_subscriber(ssm_api_pair const& p) {
    auto& api = shm_info_map[p];
    printf("%d\n", *(int32_t *) api->data);
  }

  /**
  * @brief stream, callback, 条件を登録
  */
  template <class ...Args>
  bool register_subscriber(std::vector<SubscriberSet> subscriber_set, std::function<bool()> local_condition, std::function<void(Args...)>& callback) {
    static auto serial_num = 0UL;
    std::vector<std::shared_ptr<ShmInfo>> sub_stream;
    
    for (auto& info : subscriber_set) {
      auto stream = info.stream_info;
      auto cmd = info.command;

      auto shm_info = shm_info_map.at(stream);
      if (shm_info == nullptr) {
        fprintf(stderr, "ERROR: ssm cannot find.\n");
        return false;
      }

      sub_stream.push_back(shm_info);
    }

    Subscriber<Args...> sub(sub_stream, callback, local_condition, serial_num, subscriber_set);
    subscriber.push_back(sub);
  }
};

#endif // _INC_SSM_SUBSCIBER_
