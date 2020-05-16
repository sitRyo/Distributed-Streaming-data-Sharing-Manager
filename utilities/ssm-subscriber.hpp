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
template <typename... Args, std::size_t... Indices>
auto vector_to_tuple_impl(const std::vector<void *>& v, std::tuple<Args...> tpl, std::index_sequence<Indices...>) {
  return std::make_tuple(
    * (reinterpret_cast<typename std::tuple_element<Indices, decltype(tpl)>::type *> (v[Indices]))...
  );
}

/**
 * @brief vectorをtupleに変換する。
 */
template <std::size_t N, typename... Args>
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

class SubscriberBase {
  virtual void invoke() = 0;
};

/**
 * @brief テンプレートに使う型をdata, propertyの順番で入れる。ex) dsm_gl, dsm_property など 
 */
template <typename ... Strm>
class Subscriber : public SubscriberBase {
  // callback
  std::function<void(Strm...)> callback;

  // ShmInfoの共有ポインタ
  std::vector<std::shared_ptr<ShmInfo>> shm_info_ptr;

  // dataとpropertyの順でデータが入ったtuple
  std::tuple<Strm...> data_property;

  // shm_info_ptrから作るshm_infoのvoid*が入ったvector
  std::vector<void *> shm_info;

  /**
   * @brief shm_info_ptrからdata, propertyを抽出してvectorを作成
   */
  std::vector<void *> create_data_vector() {
    for (auto shm : shm_info_ptr) {
      shm_info.push_back(shm->data);
      shm_info.push_back(shm->property);
    }
  }
public:

  Subscriber(std::vector<std::shared_ptr<ShmInfo>> _shm_info, std::function<void(Strm...)> _callback) 
  : shm_info_ptr(_shm_info), callback(_callback){
    // data, propertyへのポインタvector作成
    shm_info = create_data_vector();

    auto tpl = vector_to_tuple<sizeof...(Strm)>(shm_info, data_property);
  }

  void invoke() override {
    apply(callback, data_property);
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

  bool send_subscriber() {
    format_obsv_msg();
  
    for (auto&& element : name) {
      serialize_string(element.stream_name);
      serialize_4byte_data(element.stream_id);
      serialize_4byte_data(element.data_size);    
      serialize_4byte_data(element.property_size);
    }

    if (!send_msg(OBSV_SUBSCRIBE)) {
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
    send_subscriber();
    format_obsv_msg();
    
    if (!send_msg(OBSV_START)) {
      return false;
    }
    
    if (!recv_msg()) {
      return false;
    }

    printf("start\n");
  }

  void access_subscriber(ssm_api_pair const& p) {
    auto& api = shm_info_map[p];
    printf("%d\n", *(int32_t *) api->data);
  }

  /**
  * @brief stream, callback, 条件を登録
  */
  template <typename ...Args>
  bool register_subscriber(std::vector<ssm_api_pair> stream, std::function<void(Args...)> callback) {
    std::vector<std::shared_ptr<ShmInfo>> shm_info;
    for (auto&& stream_info : stream) {
      if (shm_info_map.find(stream_info) == shm_info_map.end()) {
        fprintf(stderr, "ERROR: stream cannot find\n");
        return false;
      }

      shm_info.push_back(shm_info_map.at(stream_info));
    }

    Subscriber<Args...> sub(shm_info, callback);
    subscriber.push_back(sub);
  }
};

#endif // _INC_SSM_SUBSCIBER_
