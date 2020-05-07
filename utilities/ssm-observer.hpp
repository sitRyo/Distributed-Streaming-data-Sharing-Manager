/**
 * ssm-observer.hpp
 * 2020/5/7 R.Gunji
 */

#ifndef __INC_SSM_OBSERVER__

#include <libssm.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

class SSMObserver {
  int msq_id;
  int pid;
public:
  SSMObserver();
  bool observer_init();
  int recv_msg(ssm_obsv_msg& obsv_msg, int const& size) const; 
  int send_msg(ssm_obsv_msg& obsv_msg, int const& size) const; 
  void msq_loop();
  void show_msq_id();
};

#endif // __INC_SSM_OBSERVER__
