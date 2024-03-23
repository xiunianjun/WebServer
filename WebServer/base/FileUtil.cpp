// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "FileUtil.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

AppendFile::AppendFile(string filename) : fp_(fopen(filename.c_str(), "ae")) {
  // 用户提供缓冲区
  /*
    __stream：指向 FILE 结构体的指针，表示要设置缓冲区的文件流。
    __buf：指向字符数组的指针，表示要用作缓冲区的内存地址。
    __size：表示缓冲区的大小，以字节数为单位。
  */
  setbuffer(fp_, buffer_, sizeof buffer_);
}

AppendFile::~AppendFile() { fclose(fp_); }

void AppendFile::append(const char* logline, const size_t len) {
  size_t n = this->write(logline, len);
  size_t remain = len - n;
  while (remain > 0) {
    size_t x = this->write(logline + n, remain);
    if (x == 0) {
      int err = ferror(fp_);
      if (err) fprintf(stderr, "AppendFile::append() failed !\n");
      break;
    }
    n += x;
    remain = len - n;
  }
}

void AppendFile::flush() { fflush(fp_); }

size_t AppendFile::write(const char* logline, size_t len) {
  return fwrite_unlocked(logline, 1, len, fp_);
}