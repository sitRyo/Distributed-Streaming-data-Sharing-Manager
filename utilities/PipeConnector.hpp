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
#include <memory> /* unique_ptr */
#include <string> /* describe path */
#include <unordered_map> /* hashmap */

struct FIFOPacketHeader {
  ssize_t data_size;
  uint32_t error_info;
};

class PipeConnection {
public:
  bool mk_pipe(std::string const& path, bool const is_remove_exist_file = false) {
    int ret = mkfifo(path.c_str(), 0666);

    if (is_remove_exist_file && ret == -1 && errno == EEXIST) {
      printf("remove exist pipe\n");
      remove(path.c_str());
      ret = mkfifo(path.c_str(), 0666);
    }

    if (ret == -1) {
      perror("PipeReader::mk_pipe");
      return false;
    }

    return true;
  }

  int open_pipe(std::string const& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
      perror("PipeReader::open_pipe");
    }
    return fd;
  }
};

class PipeReader {
  int fd_;
  std::string path_;
  std::unique_ptr<PipeConnection> con_;
public:
  PipeReader(std::string const& path) : path_(path), con_(new PipeConnection) {}

  bool mk_pipe(bool is_remove_exist_fifo = false) {
    return con_->mk_pipe(path_, is_remove_exist_fifo);
  }

  bool open_pipe() {
    fd_ = con_->open_pipe(path_);
    return fd_ >= 0;
  }

  int read_pipe(char* buf) const noexcept {
    FIFOPacketHeader header;
    int len = read(fd_, &header, sizeof(FIFOPacketHeader));
    if (len == -1) {
      perror("PipeReader::read_pipe");
      return -1;
    } else if (len != sizeof(FIFOPacketHeader)) {
      fprintf(stderr, "PipeReader::read_pipe size error.\n require = %d, packet size = %d\n", sizeof(FIFOPacketHeader), len);
      return -2;
    }

    len = read(fd_, buf, header.data_size);
    if (len == -1) {
      perror("PipeReader::read_pipe");
      return -3;
    } else if (len != header.data_size) {
      fprintf(stderr, "PipeReader::read_pipe size error. \n require = %d, packet size = %d\n", header.data_size, len);
      return -4;
    }

    return len;
  }
};

class PipeWriter {
  int fd_;
  std::string path_;
  std::unique_ptr<PipeConnection> con_;
public:
  PipeWriter(std::string const& path) : path_(path), con_(new PipeConnection) {}

  bool mk_pipe(bool is_remove_exist_fifo = false) {
    return con_->mk_pipe(path_, is_remove_exist_fifo);
  }

  bool open_pipe() {
    fd_ = con_->open_pipe(path_);
    return fd_ >= 0;
  }

  int write_pipe(char* buf, std::size_t const sz) const noexcept {
    FIFOPacketHeader header;
    header.data_size = sz;
    int len = write(fd_, &header, sizeof(header));
    if (len == -1) {
      perror("PipeWriter::write_pipe");
      return -1;
    }

    len = write(fd_, buf, sz);
    if (len == -1) {
      perror("PipeWriter::write_pipe");
      return -2;
    }

    return len;
  }
};

#endif //__INC_PIPE_CONNECTION__
