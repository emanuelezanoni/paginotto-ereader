#include "text_util.h"

namespace {

bool isContinuation(uint8_t b) {
  return (b & 0xC0) == 0x80;
}

bool validUtf8At(const char* buf, size_t len, size_t index, int* outLength) {
  if (index >= len) { *outLength = 0; return false; }
  const uint8_t b0 = (uint8_t)buf[index];
  const int width = utf8CharLenFromLead(b0);
  if (width == 1) { *outLength = 1; return b0 < 0x80; }
  if (index + (size_t)width > len) { *outLength = 1; return false; }

  const uint8_t b1 = (uint8_t)buf[index + 1];
  if (!isContinuation(b1) ||
      (width == 2 && b0 < 0xC2) ||
      (width == 3 && ((b0 == 0xE0 && b1 < 0xA0) || (b0 == 0xED && b1 > 0x9F))) ||
      (width == 4 && ((b0 == 0xF0 && b1 < 0x90) || (b0 == 0xF4 && b1 > 0x8F)))) {
    *outLength = 1;
    return false;
  }
  for (int i = 2; i < width; ++i) {
    if (!isContinuation((uint8_t)buf[index + (size_t)i])) {
      *outLength = 1;
      return false;
    }
  }
  *outLength = width;
  return true;
}

}  // namespace

int utf8CharLenFromLead(uint8_t b) {
  if (b < 0x80) return 1;
  if (b >= 0xC2 && b <= 0xDF) return 2;
  if (b >= 0xE0 && b <= 0xEF) return 3;
  if (b >= 0xF0 && b <= 0xF4) return 4;
  return 1;
}

int utf8SafeCharLenAt(const String& s, int index) {
  if (index < 0 || index >= (int)s.length()) return 0;
  int length = 0;
  validUtf8At(s.c_str(), s.length(), (size_t)index, &length);
  return length;
}

int utf8SafeCharLenAt(const char* buf, size_t len, size_t index) {
  if (!buf || index >= len) return 0;
  int length = 0;
  validUtf8At(buf, len, index, &length);
  return length;
}

String utf8CharAt(const String& s, int index) {
  const int length = utf8SafeCharLenAt(s, index);
  if (length <= 0) return String("");
  return s.substring(index, index + length);
}

bool isBreakableWhitespaceByte(char b) {
  return b == ' ' || b == '\n' || b == '\t';
}

bool isBreakablePunctuationByte(char b) {
  return b == '.' || b == ',' || b == ';' || b == ':' || b == '!' || b == '?' ||
         b == ')' || b == ']' || b == '}' || b == '-' || b == '/';
}

String normalizeTypography(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  size_t i = 0;

  while (i < in.length()) {
    const uint8_t b0 = (uint8_t)in[i];
    if (i == 0 && b0 == 0xEF && i + 2 < in.length() &&
        (uint8_t)in[i + 1] == 0xBB && (uint8_t)in[i + 2] == 0xBF) {
      i += 3;
      continue;
    }
    if (b0 == 0xC2 && i + 1 < in.length()) {
      const uint8_t b1 = (uint8_t)in[i + 1];
      if (b1 == 0xA0) { out += ' '; i += 2; continue; }
      if (b1 == 0xAB || b1 == 0xBB) { out += '"'; i += 2; continue; }
      // Defensive handling for Windows-1252 smart quotes mislabelled as UTF-8.
      if (b1 == 0x91 || b1 == 0x92) { out += '\''; i += 2; continue; }
    }
    if (b0 == 0xE2 && i + 2 < in.length() && (uint8_t)in[i + 1] == 0x80) {
      const uint8_t b2 = (uint8_t)in[i + 2];
      if (b2 >= 0x98 && b2 <= 0x9B) { out += '\''; i += 3; continue; }
      if ((b2 >= 0x9C && b2 <= 0x9F) || b2 == 0xB9 || b2 == 0xBA) {
        out += '"'; i += 3; continue;
      }
      if (b2 >= 0x93 && b2 <= 0x95) { out += '-'; i += 3; continue; }
      if (b2 == 0xA6) { out += "..."; i += 3; continue; }
    }
    out += (char)b0;
    ++i;
  }
  return out;
}

String compactText(const String& in,
                   bool* ioLastWasSpace,
                   int* ioNewlineCount,
                   bool trimTail) {
  String out;
  out.reserve(in.length());
  bool lastWasSpace = ioLastWasSpace ? *ioLastWasSpace : false;
  int newlineCount = ioNewlineCount ? *ioNewlineCount : 0;
  size_t trimAnchor = 0;

  for (size_t i = 0; i < in.length(); ++i) {
    char c = in[i];
    if (c == '\r') continue;
    if (c == '\t') c = ' ';
    if (c == '\n') {
      // Keep at most two newlines, and remove spaces immediately before them.
      if (out.length() > trimAnchor) out.remove(trimAnchor);
      ++newlineCount;
      if (newlineCount <= 2) {
        out += '\n';
        trimAnchor = out.length();
      }
      lastWasSpace = false;
      continue;
    }
    newlineCount = 0;
    if (c == ' ') {
      if (!lastWasSpace) { out += ' '; lastWasSpace = true; }
      continue;
    }
    lastWasSpace = false;
    out += c;
    trimAnchor = out.length();
  }

  if (ioLastWasSpace) *ioLastWasSpace = lastWasSpace;
  if (ioNewlineCount) *ioNewlineCount = newlineCount;
  if (trimTail) {
    while (out.length() > 0) {
      const char last = out[out.length() - 1];
      if (last != ' ' && last != '\n') break;
      out.remove(out.length() - 1);
    }
  }
  return out;
}

String compactText(const String& in) {
  return compactText(in, nullptr, nullptr, /*trimTail=*/true);
}
