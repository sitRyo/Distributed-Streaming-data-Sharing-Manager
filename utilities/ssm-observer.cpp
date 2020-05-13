/**
 * ssm-observer.cpp
 * 2020/5/7 R.Gunji
 */

// TODO: 関数の説明を追加

#include "ssm-observer.hpp"
#include "dssm-utility.hpp"

#include <cstdlib>
#include <cstring>
#include <csignal>

#include <iostream>
#include <functional>

using std::cout;
using std::endl;

int verbose_mode = 1;
/**
 * @brief SIGINT時に実行する関数
 * Subscriber側ではmsgqueへの参照を消さなくていい？
 * TODO: signal handlerはグローバルなオブジェクトでなければいけないのでstaticな関数にする必要がある。ただし、解放したいデータはクラスのメンバにあるのでstaticからでは参照できない。どうするか。
void SSMObserver::escape( int sig) {
  for (auto& api_info : api_map) {
    // デタッチ
    shmdt(api_info.second->data);
    shmdt(api_info.second->property);
    // 削除
    shmctl(api_info.second->shm_data_key, IPC_RMID, NULL);
    shmctl(api_info.second->shm_property_key, IPC_RMID, NULL);
  }

  // メッセージキューの削除
  msgctl(msq_id, IPC_RMID, NULL);
  printf("Deleted shared memory and message queue.\n");
}
*/

SSMApiInfo::SSMApiInfo() : stream_name(""), stream_id(-1), data(nullptr), property(nullptr)
{}

SSMApiInfo::~SSMApiInfo() {}

bool SSMApiInfo::open(SSM_open_mode mode) {
  if (stream_name.empty() || stream_id == -1) {
    fprintf(stderr, "ERROR: stream_name or stream_id haven't set yet.\n");
    return false;
  }

  if (!ssm_api_base.open(stream_name.c_str(), stream_id, mode)) {
    // TODO: std::runtime_errorでエラーハンドリングするか否か
    // ssm_api_baseでエラーメッセージが表示されるのでここではとりあえず何もしない。
    return false;
  }

  return true;
}

/**
 * @brief read_lastを行う(atomic)
 */
bool SSMApiInfo::read_last() {
  std::lock_guard<std::mutex> lock(mtx);
  ssm_api_base.readLast();
}

void* Subscriber::run(void *args) {
  this->subscribe_loop();
}

void Subscriber::subscribe_loop() {
  // ここで条件を検索。
  while (true) {
    for (auto&& api : ssm_api) {
      api->read_last();
    }
  }
}

SSMObserver::SSMObserver() : pid(getpid()), shm_key_num(0) {}

SSMObserver::~SSMObserver() {
  msgctl( msq_id, IPC_RMID, NULL );
}

/**
 * @brief メッセージキューを作る。
 */
bool SSMObserver::observer_init() {
  // ssm-coordinatorとのmsgque作成
  initSSM();

  // ssm-observerのmsgque作成
  msq_id = msgget(MSQ_KEY_OBS, IPC_CREAT | 0666);
  
  if (msq_id < 0) {
    // メッセージキューが存在する場合はエラーメッセージを出力して終了
    if (errno == EEXIST) {
      fprintf( stderr, "ERROR : message queue is already exist.\n" );
			fprintf( stderr, "maybe ssm-observer has started.\n" );
    }
    return false;
  }

  allocate_obsv_msg();

  return true;
}

bool SSMObserver::allocate_obsv_msg() {
	auto size = sizeof(ssm_obsv_msg);
	auto padding = OBSV_MSG_SIZE - size;
  // cout << padding << endl;
	obsv_msg.reset((ssm_obsv_msg *) malloc(sizeof(ssm_obsv_msg) + sizeof(char) * padding));
	if (!obsv_msg) {
		fprintf(stderr, "ERROR: memory allocate");
		return false;
	}
	padding_size = padding;
	return true;
}

void SSMObserver::format_obsv_msg() {
	obsv_msg->msg_type = 0;
	obsv_msg->res_type = 0;
	obsv_msg->cmd_type = 0;
	obsv_msg->pid      = 0;
	obsv_msg->msg_size = 0;
	memset(obsv_msg.get() + sizeof(ssm_obsv_msg), 0, padding_size);
}

void SSMObserver::show_msq_id() {
  std::cout << msq_id << std::endl;
}

int SSMObserver::recv_msg() {
  format_obsv_msg();
  int len = msgrcv(msq_id, (void *) obsv_msg.get(), OBSV_MSG_SIZE, OBSV_MSQ_CMD, 0);
  return len;
}

int SSMObserver::send_msg(OBSV_msg_type const& type, pid_t const& s_pid) {
  obsv_msg->msg_type = s_pid;
  obsv_msg->cmd_type = type;
  obsv_msg->pid      = pid;
  // obsv_msg->msg_size = body_size;

  // dssm::util::hexdump((char*)obsv_msg.get(), sizeof(ssm_obsv_msg));
  if (msgsnd(msq_id, (void *) obsv_msg.get(), OBSV_MSG_SIZE, 0) < 0) {
    fprintf(stderr, "errno: %d\n", errno);
    perror("msgsnd");
    fprintf(stderr, "msq send err\n");
    return false;
  }

  return true;
}

/*
bool SSMObserver::serialize_raw_data(uint32_t const& size, void* data) {
  auto& idx = obsv_msg->msg_size;
  auto left = 0;

  char* p = (char *) data;
  // printf("%p\n", p);

  // 最後にnull文字が入るのでpadding - 1
  for (; idx < padding_size - 1 && left < size; ++idx, ++left) {
    printf("%d ", p[left]);
    obsv_msg->body[idx] = p[left];
  }
  printf("\n");
  if (left < size) {
    fprintf(stderr, "ERROR: message size is too large.\n");
    return false;
  }

  obsv_msg->body[idx++] = '\0';
  return true;
}
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
  *reinterpret_cast<int32_t *>(&obsv_msg->body[size]) = data;
  // 埋めた分だけメッセージサイズを++
  size += 4;
  return true;
}

void SSMObserver::msq_loop() {
  int len = -1;
  key_t s_pid;
  while (true) {
    len = recv_msg();
    s_pid = obsv_msg->pid;
    
    switch (obsv_msg->cmd_type) {
      // Subscriberの追加
      case OBSV_INIT: {    
        printf("OBSV_INIT pid = %d\n", s_pid);
        // subscriberを追加
        create_subscriber(obsv_msg->pid);
        format_obsv_msg();
        send_msg(OBSV_RES, s_pid);
        break;
      }

      case OBSV_SUBSCRIBE: {
        printf("OBSV_SUBSCRIBE pid = %d\n", s_pid);
        auto stream_data = extract_subscriber_from_msg();
        /*for (auto itr : stream_data) {
          cout << itr.data_size << endl;
          cout << itr.property_size << endl;
          cout << itr.stream_id << endl;
          cout << itr.stream_name << endl;
        }*/
        if (!register_subscriber(obsv_msg->pid, stream_data)) {
          send_msg(OBSV_FAIL, s_pid);
        }
        
        // 返信データの準備
        format_obsv_msg();
        for (auto&& _name : stream_data) {
          ssm_api_pair api_key = std::make_pair(_name.stream_name, _name.stream_id);
          // key -> propertyの順で共有メモリキーを書き込む。
          serialize_4byte_data(api_map[api_key]->shm_data_key);
          serialize_4byte_data(api_map[api_key]->shm_property_key);
        }

        send_msg(OBSV_RES, s_pid);
        break;
      }

      case OBSV_START: {
        printf("OBSV_START pid = %d\n", s_pid);
        auto& sub = subscriber_map.at(s_pid);
        sub->start(nullptr);

        send_msg(OBSV_RES, s_pid);
        break;
      }

      default: {
        fprintf(stderr, "ERROR: unrecognized message %d\n", obsv_msg->cmd_type);
      }
    }
  }
}

std::vector<Stream> SSMObserver::extract_subscriber_from_msg() {
  std::vector<Stream> stream_data;
  auto size = obsv_msg->msg_size;
  char* data = obsv_msg->body;
  // dssm::util::hexdump(obsv_msg->body, 20);
  
  std::string stream_name;
  int32_t stream_id;
  uint32_t i = 0, data_size, property_size;
  while (i < size) {
    if (data[i] == '\0') {
      ++i;
      stream_id = deserialize_4byte_data(data + i); i += 4;
      data_size = deserialize_4byte_data(data + i); i += 4;
      property_size = deserialize_4byte_data(data + i); i += 4;
      stream_data.push_back({stream_name, stream_id, data_size, property_size});
      stream_name.clear();
    } else {
      stream_name += data[i];
      ++i;
    }
  }
  
  return stream_data;
}

bool SSMObserver::create_subscriber(pid_t const& pid) {
  subscriber_map[pid] = std::make_unique<Subscriber>(pid); // 例外を投げることがある(try catchをすべき?)
  return true;
}

bool SSMObserver::register_subscriber(pid_t const& pid, std::vector<Stream> const& stream_data) {
  auto& sub = subscriber_map[pid];
  for (auto&& data : stream_data) {
    if (!create_ssm_api_info(data)) {
      return false;
    }

    sub->ssm_api.push_back(api_map.at({data.stream_name, data.stream_id}));
    
    printf("stream_name: %s\n", data.stream_name.c_str());
    printf("stream_id:   %d\n", data.stream_id);
  }

  return true;
}

bool SSMObserver::create_ssm_api_info(Stream const& stream_data) {
  ssm_api_pair api_key = std::make_pair(stream_data.stream_name, stream_data.stream_id);
  // SSMApiが作られていない場合
  if (api_map.find(api_key) == api_map.end()) {
    auto ssm_api_info = std::make_shared<SSMApiInfo>();
    auto data_size = stream_data.data_size;
    auto property_size = stream_data.property_size;

    // 共有メモリをアタッチする。
    if ((ssm_api_info->shm_data_key = get_shared_memory(data_size, &ssm_api_info->data)) < 0) {
      fprintf(stderr, "data\n");
      return false;
    }

    if ((ssm_api_info->shm_property_key = get_shared_memory(data_size, &ssm_api_info->property)) < 0) {
      fprintf(stderr, "property\n");
      return false;
    }

    // アタッチしたバッファをデータ保存場所として指定
    ssm_api_info->ssm_api_base.setBuffer(ssm_api_info->data, data_size, ssm_api_info->property, property_size);
    ssm_api_info->stream_name = stream_data.stream_name;
    ssm_api_info->stream_id = stream_data.stream_id;
    
    // Apiをオープン
    ssm_api_info->open(SSM_READ);

    // api情報登録
    api_map.insert({api_key, ssm_api_info});
  }

  return true;
}

int32_t SSMObserver::get_shared_memory(uint32_t const& size, void** data) {
  /// TODO: 共有メモリセグメントの残メモリ量を確認する。
  // cout << size << endl;
  
  int32_t s_id;
  /// TODO: sizeを変更する！！！！！！
  if ((s_id = shmget(OBSV_SHM_KEY + shm_key_num, size, IPC_CREAT | 0666)) < 0) {
    perror("shmget");
    fprintf(stderr, "ERROR: shared memory allocate\n");
    return -1;
  }
  ++shm_key_num;

  // attach
  if ((*data = static_cast<void*>(shmat(s_id, 0, 0))) == (void*) -1) {
    perror("shmat");
    fprintf(stderr, "ERROR: shmat\n");
    return -1;
  }

  return s_id;
}

/*
void SSMObserver::set_signal_handler(int sig_num) {
  std::signal(sig_num, func);
}
*/

int main() {
  SSMObserver observer;
  if (!observer.observer_init()) {
    return 0;
  }

  std::cout << "mypid:     " << getpid() << "\n";
  std::cout << "msgque id: ";
  observer.show_msq_id();
  observer.msq_loop();
}