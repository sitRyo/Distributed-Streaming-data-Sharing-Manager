/**
 * ssm-subscriber.cpp
 * 2020/5/7 R.Gunji
 */

#include "ssm-subscriber.hpp"
#include <cstdio>
#include <cerrno>

SSMSubscriber::SSMSubscriber(): msq_id(-1) {}

bool SSMSubscriber::init_subscriber() {
  if ((msq_id = msgget(MSQ_KEY_OBS, 0666)) < 0) {
    fprintf(stderr, "msgque cannot open.\n");
    return false;
  }

  // 区別のためのプロセスIDを取得
  pid = getpid();

  ssm_obsv_msg msg;
  send_msg(OBSV_INIT, msg);

  if (recv_msg(msg, sizeof(ssm_obsv_msg)) < 0) {
    if (errno == E2BIG) {
      fprintf(stderr, "ERROR: msg size larger than size defined by user.");
    }
    return false;
  }

  return true;
}

bool SSMSubscriber::send_msg(OBSV_msg_type const& msg_type, ssm_obsv_msg& obsv_msg) const {
  obsv_msg.msg_type = OBSV_MSQ_CMD; // TODO: MSQ_CMDを指定する意味がいまいちわからない
  obsv_msg.cmd_type = OBSV_INIT;
  obsv_msg.pid = pid;
  obsv_msg.msg_size = sizeof(ssm_obsv_msg); // パディングの有無は考慮しない
  
  if (msgsnd(msq_id, (void*) &obsv_msg, sizeof(obsv_msg), 0) < 0) {
    perror("msgsnd");
    fprintf(stderr, "msq send err\n");
    return false;
  }

  return true;
}

// TODO: エラーハンドリング
// http://manpages.ubuntu.com/manpages/bionic/ja/man2/msgop.2.html
int SSMSubscriber::recv_msg(ssm_obsv_msg& obsv_msg, int const& size) const {
  int len = msgrcv(msq_id, &obsv_msg, size, OBSV_MSQ_CMD, 0);
  return len;
}
