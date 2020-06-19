//
// Created by R.Gunji on 2020/06/12.
//

#ifndef __INC_PIPE_CONNECTION__
#define __INC_PIPE_CONNECTION__

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <cstddef> /* c++ type def */
#include <cstring> /* memcpy */
#include <memory> /* unique_ptr */
#include <string> /* describe path */
#include <unordered_map> /* hashmap */

struct FIFOPacketHeader {
  ssize_t data_size;
  uint32_t error_info;
};

class PipeConnection {
  int fd_; /// file descripter
  int buffer_max_; /// バッファ最大値
  std::unique_ptr<char[]> write_buf_; /// write時に使うバッファ
public:
  PipeConnection(int const buffer_max = 1024) : buffer_max_(1024) {}
  ~PipeConnection() = default;

  bool mk_pipe(std::string const& path, bool const is_remove_exist_file = false) {
    int ret = mkfifo(path.c_str(), 0666);

    if (is_remove_exist_file && ret == -1 && errno == EEXIST) {
      printf("remove exist pipe\n");
      remove(path.c_str());
      ret = mkfifo(path.c_str(), 0666);
    }

    if (ret == -1) {
      perror("PipeConnection::mk_pipe");
      return false;
    }

    return true;
  }

  int open_pipe(std::string const& path, int const mode) {
    fd_ = open(path.c_str(), mode);
    write_buf_.reset(new char[buffer_max_]); /* defaultで1024byte分確保 */
    if (fd_ == -1) {
      perror("PipeConnection::open_pipe");
    }

    return fd_;
  }

  int read_pipe(char* buf) noexcept {
    FIFOPacketHeader header;
    int ret;
    
    // header
    if ((ret = read_impl((char*)&header, sizeof(FIFOPacketHeader))) <= 0) {
      return ret;
    }

    // body
    if ((ret = read_impl(buf, static_cast<std::size_t>(header.data_size))) < 0) {
      return ret;
    }
    
    return header.data_size;
  }

  int read_impl(char* buf, std::size_t const len) const noexcept {
    int now_len = 0;
    
    do {
      int rd = read(fd_, buf + now_len, len - now_len);
      if (rd <= 0) {
        // rd = 0  => EOF
        // rd = -1 => error
        return rd;
      }
      now_len += rd;
    } while (len != now_len);

    return len;
  }

  int write_pipe(char* buf, std::size_t const sz) const noexcept {
    FIFOPacketHeader* header = reinterpret_cast<FIFOPacketHeader*>(write_buf_.get());
    std::size_t header_size = sizeof(FIFOPacketHeader);
    std::size_t send_data_size = header_size + sz;

    // header情報
    header->data_size = sz;
    header->error_info = 0;

    if (send_data_size > buffer_max_) {
      fprintf(stderr, "PipeConnection::write_pipe send msg is too large.\n");
      return -1;
    }

    // パケットの中身をコピー
    memcpy(write_buf_.get() + header_size, buf, sz);

    int ret = 0;
    // header + body
    if ((ret = write_impl(write_buf_.get(), send_data_size)) <= 0) {
      return ret;
    }

    return sz;
  }

  int write_impl(char* buf, std::size_t const len) const noexcept {
    int now_len = 0;

    do {
      int rd = write(fd_, buf + now_len, len - now_len);
      if (rd < 0) {
        // rd = 0  => EOF
        // rd = -1 => error
        return rd;
      }
      now_len += rd;
    } while (now_len != len);

    return len;
  }
};

class PipeReader {
  std::string path_;
  std::unique_ptr<PipeConnection> con_;
public:
  PipeReader(std::string const& path) : path_(path), con_(new PipeConnection) {}

  bool mk_pipe(bool const is_remove_exist_fifo = false) {
    return con_->mk_pipe(path_, is_remove_exist_fifo);
  }

  bool open_pipe(int const mode = O_RDONLY) {
    int fd = con_->open_pipe(path_, O_RDONLY | mode);
    return fd >= 0;
  }

  int read_pipe(char* buf) const noexcept {
    return con_->read_pipe(buf);
  }
};

class PipeWriter {
  std::string path_;
  std::unique_ptr<PipeConnection> con_;
public:
  PipeWriter(std::string const& path) : path_(path), con_(new PipeConnection) {}

  bool mk_pipe(bool is_remove_exist_fifo = false) {
    return con_->mk_pipe(path_, is_remove_exist_fifo);
  }

  bool open_pipe(int const mode = O_WRONLY) {
    int fd;
    if (mode & O_NONBLOCK) {
      // fifo(7)
      // NONBLOCK指定時にはO_WRONLYではENXIOエラーが生じるのでO_RDWRに変更
      fd = con_->open_pipe(path_, O_RDWR | O_NONBLOCK);
    } else {
      fd = con_->open_pipe(path_, O_WRONLY | mode);
    }
    return fd >= 0;
  }

  int write_pipe(char* buf, std::size_t const sz) const noexcept {
    return con_->write_pipe(buf, sz);
  }
};

#endif //__INC_PIPE_CONNECTION__
