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
#include <sys/shm.h>

#include <vector>
#include <memory>
#include <unordered_map>

struct ShmInfo {
  void* data;
  void* property;
  key_t shm_data_key;
  key_t shm_property_key;
};

class SSMSubscriber {
  int msq_id;
  pid_t pid;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  
  uint32_t padding_size;
  std::vector<std::string> name;
  std::unordered_map<std::string, std::shared_ptr<ShmInfo>> shm_info_map;

  void format_obsv_msg();
  bool attach_shared_memory(void** data, int32_t s_id);
  bool register_shm_info(std::vector<std::string> const& name);
  bool send_msg(OBSV_msg_type const& type, int const& body_size);
  int recv_msg();
public:
  SSMSubscriber();
  bool init_subscriber();
  bool allocate_obsv_msg();

  void add_subscriber(std::vector<std::string> const& api);
  bool start();
  bool send_subscriber();
};

#endif // __INC_SSM_SUBSCIBER__
