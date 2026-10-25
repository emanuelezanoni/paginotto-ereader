#include "src/pure/bionic_text.h"

namespace {
bool isWordLead(unsigned char b) {
  return (b >= '0' && b <= '9') || (b >= 'A' && b <= 'Z') ||
         (b >= 'a' && b <= 'z') || b >= 128;
}

int utf8Length(unsigned char b) {
  if (b < 0x80) return 1;
  if ((b & 0xe0) == 0xc0) return 2;
  if ((b & 0xf0) == 0xe0) return 3;
  if ((b & 0xf8) == 0xf0) return 4;
  return 1;
}

int strongCharacters(int count) {
  if (count <= 0) return 0;
  if (count <= 3) return 1;
  if (count / 2 > 4) return 4;
  return count / 2;
}
}  // namespace

int bionicPrefixBytesPure(const char* word, int byte_len) {
  if (!word || byte_len <= 0 || !isWordLead(static_cast<unsigned char>(word[0]))) {
    return 0;
  }

  int chars = 0;
  for (int offset = 0; offset < byte_len;) {
    const int length = utf8Length(static_cast<unsigned char>(word[offset]));
    if (offset + length > byte_len) return 0;
    offset += length;
    ++chars;
  }

  const int strong = strongCharacters(chars);
  if (strong <= 0) return 0;

  int offset = 0;
  for (int i = 0; i < strong; ++i) {
    const int length = utf8Length(static_cast<unsigned char>(word[offset]));
    if (offset + length > byte_len) return 0;
    offset += length;
  }
  return offset > 0 && offset < byte_len ? offset : 0;
}
