#ifndef PAGINOTTO_HAL_FILE_STREAM_H
#define PAGINOTTO_HAL_FILE_STREAM_H

#include <FS.h>

#include "src/pure/stream.h"

// Small boundary adapter: all reader algorithms depend on IReadStream, while
// this class is the only place where an Arduino File is exposed to them.
class FileReadStream : public IReadStream {
public:
  explicit FileReadStream(File& f) : f_(f) {}

  int read() override {
    if (!f_) return -1;
    const int value = f_.read();
    return value < 0 ? -1 : (value & 0xff);
  }

  bool seek(uint32_t pos) override {
    return f_ && f_.seek(pos);
  }

  uint32_t position() override {
    return f_ ? static_cast<uint32_t>(f_.position()) : 0;
  }

  size_t size() override {
    return f_ ? static_cast<size_t>(f_.size()) : 0;
  }

  bool available() override {
    return f_ && f_.available() > 0;
  }

private:
  File& f_;
};

#endif  // PAGINOTTO_HAL_FILE_STREAM_H
