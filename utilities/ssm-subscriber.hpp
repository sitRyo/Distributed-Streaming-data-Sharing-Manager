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

using ssm_api_pair = std::pair<std::string, int32_t>;

/* SSMApiをunordered_mapで使うためのユーザ定義構造体群 */

struct SSMApiHash {
  size_t operator()(ssm_api_pair const& p) const {
    auto h1 = std::hash<std::string>()(p.first);
    auto h2 = std::hash<std::int32_t>()(p.second);
    return h1 xor (h2 << 1);
  }
};

struct SSMApiEqual {
  bool operator()(ssm_api_pair const& p1, ssm_api_pair const& p2) const {
    return (p1.first == p2.first) and (p1.second == p2.second);
  }
};

/* ここまで */

struct Stream {
  std::string stream_name;
  int32_t stream_id;
  uint32_t data_size;
  uint32_t property_size;
};

struct ShmInfo {
  void* data;
  void* property;
  key_t shm_data_key;
  key_t shm_property_key;
  std::string stream_name;
  int32_t stream_id;
  uint32_t data_size;
  uint32_t property_size;

  ShmInfo(Stream const& stream) : 
  stream_name(stream.stream_name), stream_id(stream.stream_id),
  data_size(stream.data_size), property_size(stream.property_size) 
  {}
};

class SSMSubscriber {
  int msq_id;
  pid_t pid;
  std::unique_ptr<ssm_obsv_msg> obsv_msg;
  
  uint32_t padding_size;
  std::vector<Stream> name;
  std::unordered_map<ssm_api_pair, std::shared_ptr<ShmInfo>, SSMApiHash, SSMApiEqual> shm_info_map;

  void format_obsv_msg();
  bool attach_shared_memory(void** data, int32_t s_id);
  bool register_shm_info(std::vector<Stream> const& name);
  bool send_msg(OBSV_msg_type const& type);
  int recv_msg();
  bool serialize_4byte_data(int32_t data);
  bool serialize_string(std::string const& str);
public:
  SSMSubscriber();
  bool init_subscriber();
  bool allocate_obsv_msg();

  void add_subscriber(std::vector<Stream> const& api);
  bool start();
  bool send_subscriber();

  void access_subscriber(ssm_api_pair const& p);
};

#endif // __INC_SSM_SUBSCIBER__
