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

class SSMSubscriber {
  int msq_id;
  pid_t pid;
public:
  SSMSubscriber();
  bool init_subscriber();
  bool send_msg(OBSV_msg_type const& msg_type, ssm_obsv_msg& obsv_msg) const;
  int recv_msg(ssm_obsv_msg& obsv_msg, int const& size) const;
};

#endif // __INC_SSM_SUBSCIBER__
