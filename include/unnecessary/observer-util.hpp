/**
 * observer-util.hpp
 * 2020/5/24 R.Gunji
 * ssm-subscriber, ssm-observer間で共通で使用するUtil関数を定義する
 */

#ifndef __INC_OBSERVERUTIL__
#define __INC_OBSERVERUTIL__

#include <utility>
#include <string>
#include <functional>
#include <libssm.h>

#include <sys/msg.h>

#include <cstring>
#include <cstdlib>

#include "dssm-utility.hpp"
#include "PipeConnector.hpp"

namespace ssm {

using ssm_api_pair = std::pair<std::string, int32_t>;

constexpr char* DSSM_PATH_ENV_NAME = "DSSM_DIRECTORY"; /* DSSM_DIRECTORYの環境変数 */
constexpr char* SERVER_PIPE_NAME = "pipe_dir/server"; /* serverのパイプ名 */
constexpr char* CLIENT_PIPE_NAME = "pipe_dir/client"; /* clientのパイプ名 */

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

struct Stream {
  std::string stream_name;
  int32_t stream_id;
  uint32_t data_size;
  uint32_t property_size;
  std::string ip_address;

  Stream(std::string const& _stream_name, int32_t _stream_id, uint32_t _data_size, uint32_t _property_size, std::string _ip_address = "") 
  : stream_name(_stream_name), stream_id(_stream_id), data_size(_data_size), property_size(_property_size), ip_address(_ip_address) 
  {}

  ~Stream() = default;
  Stream(const Stream&) = default;
  Stream& operator=(const Stream&) = default;
};

/**
 * @brief 共有メモリにあるデータに紐付いている時刻・tidを表現するデータ構造
 */
struct SSMShmDataInfo {
  ssmTimeT time;
  int32_t tid;
};

using ssm_api_pair_map = std::unordered_map<ssm_api_pair, SSMShmDataInfo, SSMApiHash, SSMApiEqual>;

/**
 * @brief 共有メモリをアタッチして、データキーを返す。
 */
int32_t get_shared_memory(uint32_t const& size, void** data, uint32_t const shm_key_num) {
  /// TODO: 共有メモリセグメントの残メモリ量を確認する。
  
  int32_t s_id;
  if ((s_id = shmget(OBSV_SHM_KEY + shm_key_num, size, IPC_CREAT | 0666)) < 0) {
    perror("shmget");
    fprintf(stderr, "ERROR: shared memory allocate\n");
    return -1;
  }

  // attach
  if ((*data = static_cast<void*>(shmat(s_id, 0, 0))) == (void*) -1) {
    perror("shmat");
    fprintf(stderr, "ERROR: shmat\n");
    return -1;
  }

  return s_id;
}

/**
 * @brief 存在する共有メモリにポインタをアタッチする。
 */
bool attach_shared_memory(void** data, int32_t s_id) {
  // attach
  if ((*data = static_cast<void*>(shmat(s_id, 0, 0))) == (void*) -1) {
    perror("shmat");
    fprintf(stderr, "ERROR: shmat\n");
    return false;
  }

  return true;
}

/**
 * @brief タプルを関数の引数に展開し, 関数を実行。
 * 本当はSFINAEでテンプレートのオーバーロードをするべき。
 */
template <class Fn, class Tuple, size_t... _Idx>
constexpr decltype(auto)
apply_impl(Fn&& f, Tuple&& t, std::index_sequence<_Idx...>) {
	return f(* std::get<_Idx>(std::forward<Tuple>(t))...);
}

/**
 * @brief apply(f, t)でfにtupleを展開し実行する。apply_implへのアダプタ。c++14で動作可能。
 */
template <class Fn, class Tuple>
constexpr decltype(auto)  // この時点で戻り値はわからない
apply(Fn&& f, Tuple&& t) {
  using Indices 
		= std::make_index_sequence<
      std::tuple_size<std::remove_reference_t<Tuple>>::value
		>;
	return apply_impl(
		std::forward<Fn>(f),
		std::forward<Tuple>(t),
		Indices{}
	);
}

/**
 * @brief impl
 */
template <class... Args, std::size_t... Indices>
auto vector_to_tuple_impl(const std::vector<void *>& v, std::tuple<Args...> tpl, std::index_sequence<Indices...>) {
  return std::make_tuple(
    (reinterpret_cast<typename std::tuple_element<Indices, decltype(tpl)>::type> (v[Indices]))...
  );
}

/**
 * @brief vectorをtupleに変換する。
 */
template <std::size_t N, class... Args>
auto vector_to_tuple(const std::vector<void *>& v, std::tuple<Args...> tpl) {
  return vector_to_tuple_impl(v, tpl, std::make_index_sequence<N>());
}

/**
 * @brief ポインタの先頭から4byte分取得。アドレスも4byte分進める。
 */
int32_t deserialize_4byte(char** buf) {
  int32_t res = *reinterpret_cast<int32_t *>(*buf); 
  *buf += 4;
  return res;
}

/**
 * @brief ポインタの先頭から8byte分取得。アドレスも8byte分進める。
 */
int64_t deserialize_8byte(char** buf) {
  int64_t res = *reinterpret_cast<int64_t *>(*buf); 
  *buf += 8;
  return res;
}

/**
 * @brief ポインタの先頭からdouble型でデータを取得。アドレスは8byte分進める
 */
double deserialize_double(char** buf) {
  double res = *reinterpret_cast<double *>(*buf);
  *buf += 8;
  return res;
}

/**
 * @brief ポインタの先頭からnull文字が来るまでデータを文字列として取得。アドレスも進める。
 */
std::string deserialize_string(char** buf) {
  std::string data;
  while (**buf != '\0') {
    data += **buf;
    *buf += 1;
  }
  // null文字分を飛ばす
  *buf += 1;
  return data;
}

/**
 * @brief obsv_msgを0初期化
 */
inline void format_obsv_msg(char* src) {
  // dssm::util::hexdump(src, 64);
  memset(src, 0, OBSV_MSG_SIZE);
  //dssm::util::hexdump(src, 64);
}

inline int construct_msg_que(key_t msg_key) {
  auto msq_id = msgget(msg_key, IPC_CREAT | 0666);
  if (msq_id < 0) {
    // メッセージキューが存在する場合はエラーメッセージを出力して終了
    if (errno == EEXIST) {
      fprintf( stderr, "ERROR : message queue is already exist.\n" );
			fprintf( stderr, "maybe ssm-observer has started.\n" );
      return -1;
    }
  }

  return msq_id;
}

std::string get_env(std::string const& name) {
  const char *env_name = getenv(name.c_str());
  std::string s {env_name};
  return s;
}

/**
 * @brief dssmのroot directoryを追加したパイプのpathを作る
 */
std::string create_full_pipe_path(std::string const& name) {
  std::string dssm_root_path = get_env(DSSM_PATH_ENV_NAME);
  return dssm_root_path + name;
}

/**
 * @brief PipeWriter関連の処理(creat, open)
 * @param is_create trueでpipeを作る
 * @param is_check_exist_pipe パイプを新しく作り直すかどうか
 * @param mode NONBLOCKなどfcntlで宣言されているマクロを指定
 */
std::unique_ptr<PipeWriter> init_pipe_writer(bool const is_create, bool const is_check_exist_pipe, std::string const& pipe_name, int const mode = O_WRONLY) {
  std::unique_ptr<PipeWriter> pipe_writer = std::make_unique<PipeWriter>(create_full_pipe_path(pipe_name));
  if (is_create) {
    pipe_writer->mk_pipe(is_check_exist_pipe);
  }

  pipe_writer->open_pipe(mode);
  
  return pipe_writer;
}

/**
 * @brief PipeReader関連の処理(creat, open)
 * @param is_create trueでpipeを作る
 * @param is_check_exist_pipe パイプを新しく作り直すかどうか
 * @param mode NONBLOCKなどfcntlで宣言されているマクロを指定
 */
std::unique_ptr<PipeReader> init_pipe_reader(bool const is_create, bool const is_check_exist_pipe, std::string const& pipe_name, int const mode = O_RDONLY) {
  std::unique_ptr<PipeReader> pipe_reader = std::make_unique<PipeReader>(create_full_pipe_path(pipe_name));
  if (is_create) {
    pipe_reader->mk_pipe(is_check_exist_pipe);
  }

  pipe_reader->open_pipe(mode);

  return pipe_reader;
}

} // namespace ssm

#endif // __INC_OBSERVERUTIL__
