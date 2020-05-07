/**
 * ssm-observer.cpp
 * 2020/5/7 R.Gunji
 */


#include "ssm-observer.hpp"

#include <iostream>

SSMObserver::SSMObserver() : pid(getpid()) {}

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

  return true;
}

void SSMObserver::show_msq_id() {
  std::cout << msq_id << std::endl;
}

int SSMObserver::recv_msg(ssm_obsv_msg& obsv_msg, int const& size) const {
  int len = msgrcv(msq_id, &obsv_msg, size, OBSV_MSQ_CMD, 0);
  return len;
}

int SSMObserver::send_msg(ssm_obsv_msg& obsv_msg, int const& size) const {
  obsv_msg.msg_type = OBSV_MSQ_CMD; // TODO: MSQ_CMDを指定する意味がいまいちわからない
  obsv_msg.cmd_type = OBSV_RES;
  obsv_msg.pid = pid;
  obsv_msg.msg_size = sizeof(ssm_obsv_msg); // パディングの有無は考慮しない
  
  if (msgsnd(msq_id, (void*) &obsv_msg, sizeof(obsv_msg), 0) < 0) {
    perror("msgsnd");
    fprintf(stderr, "msq send err\n");
    return false;
  }

  return true;
}

void SSMObserver::msq_loop() {
  ssm_obsv_msg obsv_msg;
  int len = -1;
  while (true) {
    len = recv_msg(obsv_msg, sizeof(ssm_obsv_msg));
    
    switch (obsv_msg.cmd_type) {
      case OBSV_INIT:
        printf("OBSV_INIT pid = %d\n", obsv_msg.pid);
        obsv_msg.cmd_type = OBSV_RES;
        send_msg(obsv_msg, sizeof(ssm_obsv_msg));
        printf("send\n");
        break;
      default:
        fprintf(stderr, "ERROR: unrecognized message %d\n", obsv_msg.cmd_type);
    }
  }
}

int main() {
  SSMObserver observer;
  if (!observer.observer_init()) {
    return 0;
  }

  std::cout << "msgque id: ";
  observer.show_msq_id();
  observer.msq_loop();
}