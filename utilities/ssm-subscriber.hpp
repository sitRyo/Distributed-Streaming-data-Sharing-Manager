/**
 * ssm-subscriber.hpp
 * 2020/5/7 R.Gunji
 */

#ifndef __INC_SSM_SUBSCIBER__

#include <unistd.h>
#include <libssm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include <memory>

class SSMSubscriber {
  int msq_id;
  pid_t pid;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  uint32_t padding_size;
public:
  SSMSubscriber();
  bool init_subscriber();
  bool allocate_obsv_msg();
  void format_obsv_msg();
  bool send_msg(OBSV_msg_type const& type, int const& body_size);
  int recv_msg();
};

#endif // __INC_SSM_SUBSCIBER__
