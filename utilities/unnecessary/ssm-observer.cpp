/**
 * ssm-observer.cpp
 * 2020/5/7 R.Gunji
 */

#include "ssm-observer.hpp"
#include "observer-util.hpp"
#include "dssm-utility.hpp"

#include <cstdlib>
#include <cstring>
#include <csignal>

#include <iostream>
#include <functional>

using namespace ssm;

using std::cout;
using std::endl;
using dssm::util::hexdump;

int verbose_mode = 1;

static std::unordered_map<void*, key_t> shm_memory_ptr; /* 共有メモリのポインタ */
static uint32_t shm_key_num; /* 共有メモリキーの数 */

#include <chrono>
using namespace std;

struct Timer {
  chrono::system_clock::time_point _start, _end;  
  double _elapsed;
  Timer() : _elapsed(0) {
    _start = chrono::system_clock::now();
  }

  double timeUpdate() {
    _end = chrono::system_clock::now();
    double elapsed = chrono::duration_cast<chrono::milliseconds>(_end - _start).count();    
    _elapsed = elapsed;
    // printf("%lf\n", elapsed);
    return elapsed;
  }

  inline double elapsed() {
    return _elapsed;
  }

  inline void clear() {
    _start = chrono::system_clock::now();
  }
};

Timer timer;

/**
 * @brief SIGINT時に実行する関数
 * Subscriber側ではmsgqueへの参照を消さなくていい？
 */
static void escape(int sig) {
  printf("espace\n");
  printf("free shm_memory\n");
  for (auto& shm : shm_memory_ptr) {
    // デタッチ
    shmdt(shm.first);
    // 削除
    shmctl(shm.second, IPC_RMID, NULL);
  }

  printf("delete server pipe\n");
  remove("server");

  // ssm終了
  endSSM();
  
  exit(0);
}

/**
 * @brief シグナルハンドラをセットする。
 */
void set_signal_handler() {
  std::signal(SIGINT, escape);
}

/* SubscriberHost */

/**
 * @brief SubscriberHostのコンストラクタ2
 */
SubscriberHost::SubscriberHost(pid_t _pid, uint32_t _count) 
: pid(_pid), count(_count)
{
  // 通信用メッセージバッファを確保
  this->data_buffer.reset(new char[OBSV_MSG_SIZE]);
  this->obsv_msg = reinterpret_cast<ssm_obsv_msg*>(this->data_buffer.get());
  this->msg_body = reinterpret_cast<char*>(((char*) data_buffer.get()) + sizeof(ssm_obsv_msg));
}

/**
 * @brief Threadを実行。
 */
void* SubscriberHost::run(void* args) {
  printf("opponent pid: %d, thread start\n", pid);
  loop();
}

/**
 * @brief ループ
 */
void SubscriberHost::loop() {
  // propertyがあれば共有メモリにセット。
  this->set_property_data();
  int cnt = 0;

  while (true) {
    for (auto& sub : subscriber) {
      int32_t serial_number = -1;
      auto t1 = timer.timeUpdate();
      if ((serial_number = sub.is_satisfy_condition()) != -1) {
        /* Debugを一応残しておく */
        // printf("tid %d, serial num %d\n", sub.get_trigger_subscriber_set().ssm_api->tid, serial_number);
        serialize_subscriber_data(sub, serial_number);
        // auto t2 = timer.timeUpdate();
        // printf("check subscriber time %lf ", t2 - t1);
        
        // auto time1 = timer.timeUpdate();
        send_msg(OBSV_NOTIFY, this->pid);
        // auto time2 = timer.timeUpdate();

        // printf("send time %lf ", time2 - time1);
        // cnt++;
        
        /*
        if (time2 >= 1000) {
          printf("1 sec cnt = %d", cnt);
          cnt = 0;
          timer.clear();
        }
        printf("\n");
        */
      }
    }
  }
}

inline void SubscriberHost::set_property_data() {
  for (auto & sub : subscriber) { sub.set_property(); }
}

bool SubscriberHost::serialize_subscriber_data(Subscriber& sub, int const serial_number) {
  // トリガー以外のデータを取得する。
  sub.get_other_subscriber_data();

  std::vector<SubscriberSet> other_subscriber_set = sub.get_other_subscriber_set();
  SubscriberSet trigger_subscriber_set = sub.get_trigger_subscriber_set();

  format_obsv_msg((char*)data_buffer.get());
  // シリアルナンバーをメッセージに追加
  serialize_4byte_data(serial_number);
  // APIの数(trigger + other_subscriber_setの数)
  serialize_4byte_data(other_subscriber_set.size() + 1);
  // トリガーの時刻データ・tidを追加
  serialize_string(trigger_subscriber_set.ssm_api->stream_name);
  serialize_4byte_data(trigger_subscriber_set.ssm_api->stream_id);
  serialize_4byte_data(trigger_subscriber_set.ssm_api->tid);
  serialize_double_data(trigger_subscriber_set.ssm_api->time);
  // 他のssmapiの時刻データ・tidを追加
  for (const auto& other_sub_set : other_subscriber_set) {
    serialize_string(other_sub_set.ssm_api->stream_name);
    serialize_4byte_data(other_sub_set.ssm_api->stream_id);
    serialize_4byte_data(other_sub_set.ssm_api->tid);
    serialize_double_data(other_sub_set.ssm_api->time);
  }

  return true;
}

inline void SubscriberHost::set_subscriber(Subscriber const& subscriber) {
  this->subscriber.emplace_back(subscriber);
}

inline void SubscriberHost::set_subscriber(Subscriber&& subscriber) {
  this->subscriber.emplace_back(std::move(subscriber));
}

inline void SubscriberHost::set_count(int32_t count) {
  this->count = count;
}

inline void SubscriberHost::set_stream_info_map_element(ssm_api_pair const& key, std::shared_ptr<SSMApiInfo> shm) {
  this->stream_info_map.insert({key, std::move(shm)});
}

inline void SubscriberHost::set_pipe_writer(std::unique_ptr<PipeWriter>&& pipe_writer) {
  this->pipe_writer = std::move(pipe_writer);
}

inline std::shared_ptr<SSMApiInfo> SubscriberHost::get_stream_info_map_element(ssm_api_pair const& key) {
  return this->stream_info_map.at(key);
}

inline int32_t SubscriberHost::get_count() {
  return this->count;
}

SSMSharedMemoryInfo SubscriberHost::get_shmkey(ssm_api_pair const& stream_pair, uint32_t const data_size, uint32_t const property_size) {
  // 共有メモリキーを生成
  if (stream_buffer_map.find(stream_pair) == stream_buffer_map.end()) {
    void* data = nullptr;
    void* property = nullptr;
    key_t data_key = -1;
    key_t property_key = -1;

    data_key = get_shared_memory(data_size, &data, shm_key_num++);

    // propertyはsize = 0もあり得るので
    if (property_size > 0) {
      property_key = get_shared_memory(property_size, &property, shm_key_num++);
    }

    SSMSharedMemoryInfo ssm_smemory_info {data, property, data_key, property_key};
    stream_buffer_map.insert({stream_pair, ssm_smemory_info});

    return ssm_smemory_info;
  }

  return stream_buffer_map.at(stream_pair);
}

bool SubscriberHost::send_msg(OBSV_msg_type const type, pid_t const& s_pid) {
  obsv_msg->msg_type = s_pid;
  obsv_msg->cmd_type = type;
  obsv_msg->pid      = pid; // バグるかも。というか想定外の値かもしれない。

  this->pipe_writer->write_pipe(data_buffer.get(), sizeof(ssm_obsv_msg) + obsv_msg->msg_size);
  
  return true;
}

bool SubscriberHost::serialize_4byte_data(int32_t data) {
  auto& size = obsv_msg->msg_size;
  // メッセージBodyの先頭から4byteにデータを書き込む。
  *reinterpret_cast<int32_t *>(&msg_body[size]) = data;
  // 埋めた分だけメッセージサイズを++
  size += 4;
  return true;
}

bool SubscriberHost::serialize_double_data(double data) {
  auto& size = obsv_msg->msg_size;
  // メッセージBodyの先頭から8byteにデータを書き込む。
  *reinterpret_cast<double *>(&msg_body[size]) = data;
  // 埋めた分だけメッセージサイズを++
  size += 8;
  return true;
}

bool SubscriberHost::serialize_string(std::string const& str) {
  auto size = str.size();
  if (size + obsv_msg->msg_size >= padding_size) {
    if (verbose_mode > 0) {
      fprintf(stderr, "VERBOSE: message body is too large.\n");
    }
    return false;
  }

  for (auto&& ch : str) { this->msg_body[obsv_msg->msg_size++] = ch; }
  this->msg_body[obsv_msg->msg_size++] = '\0';

  return true;
}

/* SubscriberHost End */

/* Subscriber */

Subscriber::Subscriber(uint32_t _serial_number) : serial_number(_serial_number) {}

inline void Subscriber::set_trigger_subscriber(SubscriberSet const& subscriber_set) {
  this->trigger_subscriber_set = subscriber_set;
}

inline void Subscriber::set_other_subscriber(std::vector<SubscriberSet> const& subscriber_set) {
  this->other_subscriber_set = subscriber_set;
}

inline void Subscriber::set_other_subscriber(std::vector<SubscriberSet> && subscriber_set) {
  this->other_subscriber_set = std::move(subscriber_set);
}

inline uint32_t Subscriber::get_serial_number() {
  return this->serial_number;
}

inline std::vector<SubscriberSet> Subscriber::get_other_subscriber_set() {
  return this->other_subscriber_set;  
}

inline SubscriberSet Subscriber::get_trigger_subscriber_set() {
  return this->trigger_subscriber_set;
}

/**
 * @brief Subscriberに通知する条件を満たしたか？
 */
int Subscriber::is_satisfy_condition() {
  bool is_satisfy = true;
  auto& api = trigger_subscriber_set.ssm_api;
  switch (trigger_subscriber_set.command) {
    case OBSV_COND_LATEST: {
      auto tid = api->read_last(trigger_subscriber_set.top_tid, trigger_subscriber_set.data);
      if (tid > trigger_subscriber_set.top_tid) {
        trigger_subscriber_set.top_tid = tid;
      } else {
        is_satisfy = false;
      }
      break;
    }
    
    default: {
      fprintf(stderr, "trigger: unknown command %d\n", trigger_subscriber_set.command);
      break;
    }
  }

  return (is_satisfy) ? this->serial_number : -1 ;
}

inline void Subscriber::set_property() {
  // triggerのプロパティチェック
  // nullptrではないときにプロパティのデータを共有メモリにセット
  auto trigger_sub = this->trigger_subscriber_set;
  if (trigger_sub.ssm_api->property != nullptr) {
    // セット
    memcpy(trigger_sub.property, trigger_sub.ssm_api->property.get(), trigger_sub.ssm_api->property_size);
  }

  // 他のsubscriberも同様にセット
  for (auto sub : this->other_subscriber_set) {
    if (sub.ssm_api->property != nullptr) {
      memcpy(sub.property, sub.ssm_api->property.get(), sub.ssm_api->property_size);
    }
  }
}

/**
 * @brief is_satisfy_conditionを満たしたときに呼ばれる。otherに入っているsubscriberのデータを読み出す。
 */
bool Subscriber::get_other_subscriber_data() {
  for (auto& sub_set : other_subscriber_set) {
    auto& api = sub_set.ssm_api;
    switch (sub_set.command) {
      case OBSV_COND_LATEST: {
        // 最新データを取得する。
        api->read_last(sub_set.top_tid, sub_set.data);
        break;
      }

      case OBSV_COND_TIME: {
        // 時間でデータを取得
        sub_set.ssm_api->read_time(sub_set.observe_stream->time, sub_set.data);
        break;
      }

      default: {
        ; // do nothing
      }
    }
  }

  return true;
}

/* Subscriber End */

/* SubscriberSet */ 

SubscriberSet::SubscriberSet() {}

SubscriberSet::SubscriberSet(std::shared_ptr<SSMApiInfo> const& _ssm_api, int _command) 
: ssm_api(_ssm_api), observe_stream(nullptr), command(_command), top_tid(-1)
{}

/* SubscriberSet End */

/* SSMApiInfo */

SSMApiInfo::SSMApiInfo()
: stream_name(""), stream_id(-1), data(nullptr), property(nullptr), tid(-1), ssm_api_base(nullptr), p_connector(nullptr)
{}

/**
 * @brief ssm_apiをopen
 */
bool SSMApiInfo::open(SSM_open_mode mode) {
  if (stream_name.empty() || stream_id == -1) {
    fprintf(stderr, "ERROR: stream_name or stream_id haven't set yet.\n");
    return false;
  }

  switch (ssm_api_type) {
    case SSM_API_BASE: {
      if (!ssm_api_base->open(stream_name.c_str(), stream_id, mode)) {
        // TODO: std::runtime_errorでエラーハンドリングするか否か
        // ssm_api_baseでエラーメッセージが表示されるのでここではとりあえず何もしない。
        return false;
      }

      // property取得
      if (this->property_size > 0) {
        ssm_api_base->getProperty();
      }

      printf("   | ssm_api_base open\n");

      break;
    }

    case P_CONNECTOR: {
      // 通信路の確保
      if (!p_connector->initRemote()) {
        fprintf(stderr, "ERROR: initRemote\n");
        goto PCON_ERROR;
      }

      // proxy側を共有メモリに接続
      if (!p_connector->open(stream_name.c_str(), stream_id, mode)) {
        fprintf(stderr, "ERROR: open\n");
        goto PCON_ERROR;
      }

      // データ通信路を構築
      if (!p_connector->createDataCon()) {
        p_connector->terminate();
        goto PCON_ERROR;
      }

      if (this->property_size > 0) {
        p_connector->getProperty();
      }

      printf("   | p_connector open\n");

      break;
      
      // error handling
PCON_ERROR:
      p_connector->terminate();
      return false;
    }
  }

  return true;
}

/**
 * @brief read_lastを行い, timeidを返す(atomic)
 */
int32_t SSMApiInfo::read_last(int32_t opponent_tid, void* opponent_data_ptr) {
  int32_t tid_now;
  switch (ssm_api_type) {
    case SSM_API_BASE: {
      tid_now = getTID_top(ssm_api_base->getSSMId());
      if (opponent_tid < tid_now && this->tid != tid_now) {
        ssm_api_base->readLast();
        this->time = ssm_api_base->time;
      }
      break;
    }

    case P_CONNECTOR: {
      tid_now = p_connector->getTID_top();
      if (opponent_tid < tid_now && this->tid != tid_now) {
        p_connector->readLast();
        this->time = p_connector->time;
      }
      break;
    }

    default: {
      fprintf(stderr, "ERROR: read_last\n");
    }
  }
  
  // データをコピー
  memcpy(opponent_data_ptr, data.get(), data_size);

  this->tid = tid_now;

  return this->tid;
}

/**
 * @brief readtimeを行う。
 */
int32_t SSMApiInfo::read_time(ssmTimeT time, void* opponent_data_ptr) {
  switch (ssm_api_type) {
    case SSM_API_BASE: {
      this->ssm_api_base->readTime(time);
      this->tid = ssm_api_base->timeId;
      this->time = ssm_api_base->time;
      break;
    }

    case P_CONNECTOR: {
      this->p_connector->readTime(time);
      this->tid = p_connector->timeId;
      this->time = p_connector->time;
      break;
    }
  }
  

  // データをコピー
  memcpy(opponent_data_ptr, data.get(), data_size);
  
  return this->tid;
}

SSMObserver::SSMObserver() : pid(getpid()) {}

SSMObserver::~SSMObserver() {
  // msgctl( msq_id, IPC_RMID, NULL );
}

/* SSMApiInfo End */

/* SSMObserver */

/**
 * @brief FIFOを作る。
 */
bool SSMObserver::observer_init() {
  // ssm-coordinatorとのmsgque作成
  if (initSSM() == 0) {
    fprintf(stderr, "SSMObserver::observer_init: ssm-coordinator cannot find in this host.\n");
    return false;
  }

  pipe_reader = init_pipe_reader(true, true, SERVER_PIPE_NAME, O_NONBLOCK);

  return allocate_obsv_msg();
}

/**
 * @brief FIFOで使うバッファを確保。ポインタをバッファにセット。
 */
bool SSMObserver::allocate_obsv_msg() {
  data_buffer.reset(new char[OBSV_MSG_SIZE]);
  if (data_buffer == nullptr) {
    fprintf(stderr, "SSMObserver::allocate_obsv_msg: bad alloc.\n");
    return false;
  }

  this->obsv_msg = reinterpret_cast<ssm_obsv_msg*>(data_buffer.get());
  this->msg_body = reinterpret_cast<char*>(((char*) data_buffer.get()) + sizeof(ssm_obsv_msg)); // ここのポインタのセットは少し怪しいかもしれない。
	return true;
}

int SSMObserver::recv_msg_sync() {
  int len;
  format_obsv_msg(data_buffer.get());
  while ((len = recv_msg()) <= 0) {
    if (len == -1 && errno != EAGAIN)  {
      perror("SSMSubscriber::recv_msg_sync");
      return -1;
    }
  }

  return len;
}

int SSMObserver::recv_msg() {
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

int SSMObserver::send_msg(OBSV_msg_type const& type, pid_t const& s_pid) {
  obsv_msg->msg_type = s_pid;
  obsv_msg->cmd_type = type;
  obsv_msg->pid      = pid;

  std::unique_ptr<PipeWriter>& pw = this->client_pipe_writer[s_pid];

  pw->write_pipe(data_buffer.get(), sizeof(ssm_obsv_msg) + obsv_msg->msg_size);

  return true;
}

/**
 * @brief ポインタの先頭から4byte分取得。アドレスは加算しない
 */
int32_t SSMObserver::deserialize_4byte_data(char* buf) {
  int32_t res = *reinterpret_cast<int32_t *>(buf);
  return res;
}

bool SSMObserver::serialize_4byte_data(int32_t data) {
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

void SSMObserver::msq_loop() {
  key_t s_pid;
  while (true) {
    int len = recv_msg_sync();
    // メッセージがパイプの中に入っていない
    if (len < 0) { 
      perror("SSMObserver::msq_loop");
      exit(1);
    }

    s_pid = obsv_msg->pid;

    switch (obsv_msg->cmd_type) {
      // SubscriberHostの追加
      case OBSV_INIT: {
        printf("OBSV_INIT pid = %d\n", s_pid);

        // SubscriberHostを追加
        create_subscriber(s_pid); 

        format_obsv_msg((char*)data_buffer.get());

        send_msg(OBSV_RES, s_pid);
        break;
      }

      case OBSV_ADD_STREAM: {
        printf("OBSV_ADD_STREAM pid = %d\n", s_pid);
        printf("data buf %p, msg body %p\n", data_buffer.get(), msg_body);
        auto stream_data = extract_stream_from_msg();
        
        if (!register_stream(obsv_msg->pid, stream_data)) {
          send_msg(OBSV_FAIL, s_pid);
        }

        // 返信データの準備
        format_obsv_msg((char*)data_buffer.get());
        send_msg(OBSV_RES, s_pid);
        break;
      }

      case OBSV_SUBSCRIBE: {
        printf("OBSV_SUBSCRIBE pid = %d\n", s_pid);
        printf("data buf %p, msg body %p\n", data_buffer.get(), msg_body);
        
        // msg内のsubscriberを登録
        register_subscriber(s_pid);
        send_msg(OBSV_RES, s_pid);
        break;
      }

      case OBSV_START: {
        printf("OBSV_START pid = %d\n", s_pid);
        
        auto& sh = subscriber_map.at(s_pid);
        // writerをセット
        sh->set_pipe_writer(std::move(client_pipe_writer.at(s_pid)));
        // thread start
        sh->start(nullptr);
        break;
      }

      default: {
        fprintf(stderr, "ERROR: unrecognized message %d\n", obsv_msg->cmd_type);
      }
    }
  }
}

std::vector<Stream> SSMObserver::extract_stream_from_msg() {
  std::vector<Stream> stream_data;
  char *p = msg_body;

  // stream_name, stream_id, data_size, property_size,を抽出する。
  while (true) {
    std::string stream_name = deserialize_string(&p);
    if (stream_name == "\0") {
      break;
    }

    auto stream_id = deserialize_4byte(&p);
    uint32_t data_size = static_cast<uint32_t>(deserialize_4byte(&p));
    uint32_t property_size = static_cast<uint32_t>(deserialize_4byte(&p));
    std::string ip_address = deserialize_string(&p);
    stream_data.push_back({stream_name, stream_id, data_size, property_size, ip_address});
  }
  
  return stream_data;
}

/**
 * @brief SubscriberHostを作成
 */
bool SSMObserver::create_subscriber(pid_t const pid) {
  subscriber_map.insert({pid, std::make_unique<SubscriberHost>(pid, 0)});
  // clientとのpipeを作る
  std::string client_path = CLIENT_PIPE_NAME + std::to_string(pid);

  // serverがclientに対してデータを送信するのでWriter
  // pipewriter作成
  auto client_pipe_writer = init_pipe_writer(false, true, client_path, O_NONBLOCK);
  this->client_pipe_writer.insert({pid, std::move(client_pipe_writer)});

  return true;
}

/**
 * @brief Subscriberの数を取得
 */
uint32_t SSMObserver::extract_subscriber_count() {
  auto num = deserialize_4byte_data(msg_body);
  return static_cast<uint32_t>(num);
}

/**
 * @brief Subscriberからのメッセージ解析
 * User Program
 * => Subscriber1, Subscriber2, Subscriber3 ... SubscriberN
 * Subscriber
 * => SSMApi, trigger
 * と構成されている。
 * なので、最初にプログラムのSubscriberの数を取得して、その後、Subscriberが要求するAPIやtriggerなどの条件をメッセージから取得する。
 */
bool SSMObserver::register_subscriber(pid_t const& pid) {
  std::unique_ptr<SubscriberHost>& subscriber_host = subscriber_map.at(pid);

  // SubscriberHostにsubscriberが1つも登録されていない場合
  if (subscriber_host->get_count() == 0) {
    auto count = extract_subscriber_count();
    subscriber_host->set_count(count);
    printf("   | send subscriber\n");
    return true;
  }

  std::vector<key_t> shm_keys;

  char* buf = this->msg_body;
  
  /* 1つのSubscriberに登録されている条件などを取得 & 登録 */
  // serial_number
  auto serial_number = deserialize_4byte(&buf);

  // Subscriberをシリアルナンバーから生成
  Subscriber sub(serial_number);
  // subscribeするssmapiの数
  uint32_t ssm_api_num = deserialize_4byte(&buf);

  printf("   | api num %d, serial number %d\n", ssm_api_num, serial_number);

  SubscriberSet trigger_sub_set;
  std::vector<SubscriberSet> other_sub_set;
  other_sub_set.reserve(ssm_api_num);
  for (uint32_t i = 0; i < ssm_api_num; ++i) {
    // ストリーム情報
    auto stream_name = deserialize_string(&buf);
    auto stream_id   = deserialize_4byte(&buf);
    ssm_api_pair stream_info_pair = std::make_pair(stream_name, stream_id);
    // コマンドとトリガー
    auto command     = deserialize_4byte(&buf);
    auto trigger     = deserialize_4byte(&buf);

    // 時間指定でデータを取得するとき。どのストリーム情報を基準とするかを取得している。
    ssm_api_pair observe_stream;
    if (trigger == OBSV_COND_NO_TRIGGER) {
      observe_stream.first = deserialize_string(&buf);
      observe_stream.second = deserialize_4byte(&buf);
    }

    // ストリームの情報(SubscriberSetに登録する)
    std::shared_ptr<SSMApiInfo> ssm_api_info = subscriber_host->get_stream_info_map_element(stream_info_pair);

    // 共有メモリキーを取得する。
    auto ss_shm_info = subscriber_host->get_shmkey(stream_info_pair, ssm_api_info->data_size, ssm_api_info->property_size);

    // SubscriberSet
    SubscriberSet ss(ssm_api_info, command);

    printf("   | stream_name: %s, stream_id: %d, command: %d\n", stream_name.c_str(), stream_id, command);
    printf("   | data_key   : %d, property_key %d\n", ss_shm_info.data_key, ss_shm_info.property_key);
    printf("   |\n");

    // データへのアクセスのためのキー
    shm_keys.push_back(ss_shm_info.data_key);
    // プロパティへのアクセスのためのキー
    shm_keys.push_back(ss_shm_info.property_key);

    // SIGINT時に共有メモリを解放するためにstaticなhashmapに共有メモリ情報を登録。
    shm_memory_ptr.insert({ss_shm_info.data, ss_shm_info.data_key});
    shm_memory_ptr.insert({ss_shm_info.property, ss_shm_info.property_key});

    // 共有メモリをセット
    ss.data = ss_shm_info.data;
    ss.property = ss_shm_info.property;

    if (trigger == OBSV_COND_TRIGGER) {
      trigger_sub_set = ss;
    } else {
      ss.observe_stream = subscriber_host->get_stream_info_map_element(observe_stream);
      other_sub_set.push_back(ss);
    }
  }

  // subscriberにsubscribersetを登録
  // trigger
  sub.set_trigger_subscriber(trigger_sub_set);
  // trigger以外
  sub.set_other_subscriber(std::move(other_sub_set));

  // subscriberの全情報をsubscriberhostに登録
  subscriber_host->set_subscriber(std::move(sub));

  // データを詰める前にバッファを初期化
  format_obsv_msg((char*)data_buffer.get());
  // 共有メモリキーを送信
  for (auto&& key : shm_keys) {
    serialize_4byte_data(key);
  }

  return true;
}

bool SSMObserver::register_stream(pid_t const& pid, std::vector<Stream> const& stream_data) {
  for (auto&& data : stream_data) {
    if (!create_ssm_api_info(data, pid)) {
      return false;
    }
    
    printf("   | stream_name: %s\n", data.stream_name.c_str());
    printf("   | stream_id:   %d\n", data.stream_id);
    printf("   | data_size:   %d\n", data.data_size);
    printf("   | type     :   %s\n", (data.ip_address == "") ? "SSMApi" : "PConnector");
    printf("   |\n");
  }

  return true;
}

/**
 * @brief Streamからssmapitypeを決定し, instantiateする。
 */
inline void SSMObserver::instantiate_ssm_api_type(Stream const& stream, std::shared_ptr<SSMApiInfo> ssm_api_info) {
  // 基底クラスを作ろうと思ったけど, SSMApiBaseと異なる使い方をするメソッドが多かったのでpointerで実装。
  // ip_addressがあればPConnector
  if (stream.ip_address == "") {
    ssm_api_info->ssm_api_base.reset(new SSMApiBase);
    ssm_api_info->ssm_api_type = SSM_API_BASE;
  } else {
    ssm_api_info->p_connector.reset(new PConnector);
    ssm_api_info->ssm_api_type = P_CONNECTOR;
  }
}

/**
 * @brief SSMApi, PConnectorにbufferを設定
 */
inline void SSMObserver::api_set_buffer(std::shared_ptr<SSMApiInfo> ssm_api_info) {
  switch (ssm_api_info->ssm_api_type) {
    case SSM_API_BASE: {
      ssm_api_info->ssm_api_base
        ->setBuffer(ssm_api_info->data.get(), ssm_api_info->data_size, ssm_api_info->property.get(), ssm_api_info->property_size);
      break;
    }

    case P_CONNECTOR: {
      // writeはしないので, fulldataにはnullptrを入れておく。
      ssm_api_info->p_connector
        ->setBuffer(ssm_api_info->data.get(), ssm_api_info->data_size, ssm_api_info->property.get(), ssm_api_info->property_size, nullptr);
      break;
    }

    default: {
      fprintf(stderr, "ERROR: unknown api type\n");
    }
  }
}

/**
 * @brief 
 */
inline void SSMObserver::set_stream_data_to_ssm_api(Stream const& stream, std::shared_ptr<SSMApiInfo> ssm_api_info) {
  ssm_api_info->stream_name = stream.stream_name;
  ssm_api_info->stream_id = stream.stream_id;
  ssm_api_info->data_size = stream.data_size;
  ssm_api_info->property_size = stream.property_size;
  ssm_api_info->ip_address = stream.ip_address;

  // データ確保
  ssm_api_info->data = std::make_unique<uint8_t[]>(ssm_api_info->data_size);
  ssm_api_info->property = std::make_unique<uint8_t[]>(ssm_api_info->property_size);
}

/**
 * @brief SSMApiを作成
 */
bool SSMObserver::create_ssm_api_info(Stream const& stream_data, pid_t const pid) {
  std::unique_ptr<SubscriberHost>& subscriber_host = subscriber_map.at(pid);
  ssm_api_pair key = std::make_pair(stream_data.stream_name, stream_data.stream_id);

  auto ssm_api_info = std::make_shared<SSMApiInfo>();
  // SSMApiBase or PConnectorをInstantiate
  instantiate_ssm_api_type(stream_data, ssm_api_info);
  // ストリーム情報を設定する。
  set_stream_data_to_ssm_api(stream_data, ssm_api_info);  
  // アタッチしたバッファをデータ保存場所として指定
  api_set_buffer(ssm_api_info);
  // Apiをオープン
  ssm_api_info->open(SSM_READ);
  subscriber_host->set_stream_info_map_element(key, ssm_api_info);

  return true;
}

void espace_load() {
  struct sigaction sa_sigint;
  sa_sigint.sa_handler = escape;
  sa_sigint.sa_flags = SA_RESETHAND | SA_NODEFER;
	sa_sigint.sa_restorer = 0;

  if (sigaction(SIGINT, &sa_sigint, NULL) < 0) {
    while (1) {}
    perror("sigaction");
    exit(1); // 異常終了
  }
}

int main() {
  SSMObserver observer;
  if (!observer.observer_init()) {
    return 0;
  }

  espace_load();

  std::cout << "mypid: " << getpid() << "\n";
  observer.msq_loop();
}

/* SSMObserver End */