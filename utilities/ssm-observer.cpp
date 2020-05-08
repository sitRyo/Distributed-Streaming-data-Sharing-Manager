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

SSMObserver::SSMObserver() : pid(getpid()) {}

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

int SSMObserver::send_msg(OBSV_msg_type type, int const& body_size) {
  pid_t send_pid = obsv_msg->pid;
  format_obsv_msg();
  obsv_msg->msg_type = send_pid;
  obsv_msg->cmd_type = type;
  obsv_msg->pid      = pid;
  obsv_msg->msg_size = body_size;

  // dssm::util::hexdump((char*)obsv_msg.get(), sizeof(ssm_obsv_msg));
  if (msgsnd(msq_id, (void *) obsv_msg.get(), OBSV_MSG_SIZE, 0) < 0) {
    fprintf(stderr, "errno: %d\n", errno);
    perror("msgsnd");
    fprintf(stderr, "msq send err\n");
    return false;
  }

  return true;
}

void SSMObserver::msq_loop() {
  int len = -1;
  while (true) {
    len = recv_msg();
    
    switch (obsv_msg->cmd_type) {
      // Subscriberの追加
      case OBSV_INIT: {
        printf("OBSV_INIT pid = %d\n", obsv_msg->pid);
        // subscriberを追加
        create_subscriber(obsv_msg->pid);
        send_msg(OBSV_RES, 0);
        break;
      }

      default: {
        fprintf(stderr, "ERROR: unrecognized message %d\n", obsv_msg->cmd_type);
      }
    }
  }
}

bool SSMObserver::create_subscriber(pid_t const& pid) {
  subscriber_map[pid] = std::make_shared<Subscriber>(pid); // 例外を投げることがある(try catchをすべき?)
  return true;
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