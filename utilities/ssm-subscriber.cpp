/**
 * ssm-subscriber.cpp
 * 2020/5/7 R.Gunji
 */

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
	memset(obsv_msg.get() + sizeof(ssm_obsv_msg), 0, padding_size);
}

bool SSMSubscriber::send_msg(OBSV_msg_type const& type, int const& body_size) {
	format_obsv_msg();
  obsv_msg->msg_type = OBSV_MSQ_CMD;
  obsv_msg->cmd_type = type;
  obsv_msg->pid = pid;
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
