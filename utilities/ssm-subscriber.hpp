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
  std::string stream_name;
  int32_t stream_id;
};

class SSMSubscriber {
public:
  using ssm_api_pair = std::pair<std::string, int32_t>;
private:
  int msq_id;
  pid_t pid;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  
  uint32_t padding_size;
  std::vector<ssm_api_pair> name;
  std::unordered_map<std::string, std::shared_ptr<ShmInfo>> shm_info_map;

  void format_obsv_msg();
  bool attach_shared_memory(void** data, int32_t s_id);
  bool register_shm_info(std::vector<ssm_api_pair> const& name);
  bool send_msg(OBSV_msg_type const& type);
  int recv_msg();
  bool serialize_4byte_data(int32_t data);
public:
  SSMSubscriber();
  bool init_subscriber();
  bool allocate_obsv_msg();

  void add_subscriber(std::vector<ssm_api_pair> const& api);
  bool start();
  bool send_subscriber();
};

#endif // __INC_SSM_SUBSCIBER__
