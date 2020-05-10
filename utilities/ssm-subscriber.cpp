/**
 * ssm-subscriber.cpp
 * 2020/5/7 R.Gunji
 */

// TODO: 
// 共有メモリのメモリ領域の解放
// Signal Handlerの登録(SIGINT = ctrl+c)

#include "ssm-subscriber.hpp"
#include "dssm-utility.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>

SSMSubscriber::SSMSubscriber(): msq_id(-1), padding_size(-1) {}

bool SSMSubscriber::init_subscriber() {
  if ((msq_id = msgget(MSQ_KEY_OBS, 0666)) < 0) {
    fprintf(stderr, "msgque cannot open.\n");
    return false;
  }

  // 区別のためのプロセスIDを取得
  pid = getpid();
	printf("mypid: %d\n", pid);
	allocate_obsv_msg();
  
  send_msg(OBSV_INIT, 0);

  if (recv_msg() < 0) {
    if (errno == E2BIG) {
      fprintf(stderr, "ERROR: msg size is too large.\n");
    }
    return false;
  }

  printf("observer pid: %d\n", obsv_msg->pid);

  return true;
}

bool SSMSubscriber::allocate_obsv_msg() {
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

void SSMSubscriber::format_obsv_msg() {
	obsv_msg->msg_type = 0;
	obsv_msg->res_type = 0;
	obsv_msg->cmd_type = 0;
	obsv_msg->pid      = 0;
	obsv_msg->msg_size = 0;
	memset(obsv_msg->body, 0, padding_size);
}

bool SSMSubscriber::send_msg(OBSV_msg_type const& type, int const& body_size) {
  obsv_msg->msg_type = OBSV_MSQ_CMD;
  obsv_msg->cmd_type = type;
  obsv_msg->pid      = pid;
  obsv_msg->msg_size = body_size;

	if (msgsnd(msq_id, (void *) obsv_msg.get(), OBSV_MSG_SIZE, 0) < 0) {
    perror("msgsnd");
    fprintf(stderr, "msq send err\n");
    return false;
  }

  return true;
}

// TODO: エラーハンドリング
// http://manpages.ubuntu.com/manpages/bionic/ja/man2/msgop.2.html
int SSMSubscriber::recv_msg() {
  format_obsv_msg();
  int len = msgrcv(msq_id, obsv_msg.get(), OBSV_MSG_SIZE, pid, 0);
  return len;
}

/**
 * @brief SubscribeするSSMAPiを追加
 */
void SSMSubscriber::add_subscriber(std::vector<std::string> const& api) {
  for (auto _name : api) {
    shm_info_map.insert({_name, std::make_shared<ShmInfo>()});
    name.push_back(_name);
  }
}

/**
 * @brief subscriber情報を送信
 * @return 成功したか否か
 */
bool SSMSubscriber::send_subscriber() {
  format_obsv_msg();
  
  auto idx = 0;
  for (auto&& _name : name) {
    for (auto&& ch : _name) { obsv_msg->body[idx++] = ch; }
    obsv_msg->body[idx++] = '\0';
  }

  if (!send_msg(OBSV_SUBSCRIBE, idx)) {
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

bool SSMSubscriber::register_shm_info(std::vector<std::string> const& name) {
  auto sz = 0;
  auto key_t_sz = sizeof(key_t);
  for (auto&& _name : name) {
    if (sz + key_t_sz * 2 <= obsv_msg->msg_size) {
      auto& shm_info = shm_info_map.at(_name);

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

bool SSMSubscriber::attach_shared_memory(void** data, int32_t s_id) {
  // attach
  if ((*data = static_cast<char*>(shmat(s_id, 0, 0))) == (void*) -1) {
    perror("shmat");
    fprintf(stderr, "ERROR: shmat\n");
    return false;
  }

  return true;
}

/**
 * @brief subscriber, condをObserverに送信。Subscribeを開始
 * @return 成功したか否か
 */
bool SSMSubscriber::start() {
  send_subscriber();
  printf("send subscribers\n");
}