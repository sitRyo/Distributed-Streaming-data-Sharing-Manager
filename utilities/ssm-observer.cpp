/**
 * ssm-observer.cpp
 * 2020/5/7 R.Gunji
 */

// TODO: 関数の説明を追加

#include "ssm-observer.hpp"
#include "dssm-utility.hpp"

#include <cstdlib>
#include <cstring>

#include <iostream>

using std::cout;
using std::endl;

int verbose_mode = 1;

SSMApiInfo::~SSMApiInfo() {
  // 共有メモリにあたっちしているので一旦削除
  // delete[] data;
  // delete[] property;
}

SSMObserver::SSMObserver() : pid(getpid()), shm_key_num(0) {}

SSMObserver::~SSMObserver() {
  msgctl( msq_id, IPC_RMID, NULL );
}

// メッセージキューを作る
bool SSMObserver::observer_init() {
  // msq_id = msgget(MSQ_KEY_OBS, IPC_CREAT | IPC_EXCL | 0666 );
  msq_id = msgget(MSQ_KEY_OBS, IPC_CREAT | 0666 );
  
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
        auto name = extract_subscriber_from_msg();
        if (!register_subscriber(obsv_msg->pid, name)) {
          send_msg(OBSV_FAIL, s_pid);
        }

        // 返信データの準備
        format_obsv_msg();
        for (auto&& _name : name) {
          // key -> propertyの順で共有メモリキーを書き込む。
          serialize_4byte_data(api_map[_name]->shm_data_key);
          serialize_4byte_data(api_map[_name]->shm_property_key);
        }

        send_msg(OBSV_RES, s_pid);
        break;
      }

      default: {
        fprintf(stderr, "ERROR: unrecognized message %d\n", obsv_msg->cmd_type);
      }
    }
  }
}

std::vector<std::string> SSMObserver::extract_subscriber_from_msg() {
  std::vector<std::string> name;
  auto size = obsv_msg->msg_size;
  char* data = obsv_msg->body;
  std::string str = "";
  for (auto i = 0; i < size; ++i) {
    if (data[i] == '\0') {
      name.push_back(str);
      str.clear();
    } else {
      str += data[i];
    }
  }
  return name;
}

bool SSMObserver::create_subscriber(pid_t const& pid) {
  subscriber_map[pid] = std::make_shared<Subscriber>(pid); // 例外を投げることがある(try catchをすべき?)
  return true;
}

bool SSMObserver::register_subscriber(pid_t const& pid, std::vector<std::string>& name) {
  auto sub = subscriber_map[pid];
  sub->api = name;
  return api_open(sub->api);
}

bool SSMObserver::api_open(std::vector<std::string> const& name) {
  for (auto&& _name : name) {
    // SSMApiが作られていない場合
    if (api_map.find(_name) == api_map.end()) {
      auto ssm_api_info = std::make_shared<SSMApiInfo>();
      auto data_size = ssm_api_info->ssm_api_base.dataSize();
      auto property_size = ssm_api_info->ssm_api_base.propertySize();
      ssm_api_info->ssm_api_base.setBuffer(ssm_api_info->data, data_size, ssm_api_info->property, property_size);

      if ((ssm_api_info->shm_data_key = get_shared_memory(data_size, &ssm_api_info->data)) < 0) {
        fprintf(stderr, "data\n");
        return false;
      }

      if ((ssm_api_info->shm_property_key = get_shared_memory(data_size, &ssm_api_info->property)) < 0) {
        fprintf(stderr, "property\n");
        return false;
      }

      /// debug!!!!
      // *reinterpret_cast<int32_t*>(ssm_api_info->data) = 10;
      // *reinterpret_cast<int32_t*>(ssm_api_info->property) = 11;

      // printf("%d %d\n", ssm_api_info->shm_data_key, ssm_api_info->shm_property_key);

      // api情報登録
      api_map.insert({_name, ssm_api_info});
    }
  }

  return true;
}

int32_t SSMObserver::get_shared_memory(uint32_t const& size, void** data) {
  /// TODO: 共有メモリセグメントの残メモリ量を確認する。
  // cout << size << endl;
  
  int32_t s_id;
  /// TODO: sizeを変更する！！！！！！
  if ((s_id = shmget(OBSV_SHM_KEY + shm_key_num, 4, IPC_CREAT | 0666)) < 0) {
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