#include "paginator.h"

#include <string.h>

#include "text_util.h"

// The reader deliberately avoids allocating while laying out a page. The
// buffers are bounded by the largest line we can render and by a long
// unbroken token (for example, a URL).
static constexpr size_t kLineCapacity = 256;
static constexpr size_t kTokenCapacity = 512;
static constexpr size_t kCandidateCapacity = kLineCapacity + kTokenCapacity + 1;

uint32_t paginatePage(IReadStream& in,
                      uint32_t startPos,
                      const LayoutMetrics& m,
                      const MeasureFn& measure,
                      const LineCallback& onLine) {
  in.seek(startPos);

  int linesUsed = 0;
  size_t lineLen = 0;
  size_t tokenLen = 0;
  char line[kLineCapacity] = {};
  char token[kTokenCapacity] = {};
  char candidate[kCandidateCapacity] = {};
  uint32_t tokenStart = startPos;

  auto nextOffset = [&](uint32_t offset) -> uint32_t {
    if (offset <= startPos) offset = startPos + 1;
    const size_t total = in.size();
    if (total != 0 && offset > total) offset = (uint32_t)total;
    return offset;
  };

  auto publish = [&](const char* text, size_t length) {
    while (length > 0 && text[length - 1] == ' ') --length;
    if (length >= kLineCapacity) length = kLineCapacity - 1;

    char output[kLineCapacity];
    if (length != 0) memcpy(output, text, length);
    output[length] = '\0';
    ++linesUsed;
    if (onLine) onLine(output, length);
  };

  auto flushLine = [&]() {
    publish(line, lineLen);
    lineLen = 0;
    line[0] = '\0';
  };

  auto splitToken = [&]() -> uint32_t {
    while (tokenLen != 0) {
      size_t pieceLen = 0;
      while (pieceLen < tokenLen && pieceLen < kLineCapacity - 1) {
        const int charLen = utf8SafeCharLenAt(token, tokenLen, pieceLen);
        if (charLen <= 0 || pieceLen + (size_t)charLen > tokenLen) break;

        const char saved = token[pieceLen + (size_t)charLen];
        token[pieceLen + (size_t)charLen] = '\0';
        const bool fits = measure(token) <= m.maxWidth;
        token[pieceLen + (size_t)charLen] = saved;
        if (!fits) break;
        pieceLen += (size_t)charLen;
      }

      if (pieceLen == 0) {
        int charLen = utf8SafeCharLenAt(token, tokenLen, 0);
        if (charLen <= 0) charLen = 1;
        pieceLen = (size_t)charLen > tokenLen ? tokenLen : (size_t)charLen;
      }

      publish(token, pieceLen);
      if (linesUsed >= m.maxLines)
        return nextOffset(tokenStart + (uint32_t)pieceLen);

      memmove(token, token + pieceLen, tokenLen - pieceLen);
      tokenLen -= pieceLen;
      tokenStart += (uint32_t)pieceLen;
    }
    return 0;
  };

  std::function<uint32_t()> placeToken = [&]() -> uint32_t {
    if (tokenLen == 0) return 0;

    if (lineLen == 0) {
      token[tokenLen] = '\0';
      if (tokenLen < kLineCapacity && measure(token) <= m.maxWidth) {
        memcpy(line, token, tokenLen);
        lineLen = tokenLen;
        tokenLen = 0;
        return 0;
      }
      return splitToken();
    }

    if (lineLen + tokenLen >= kCandidateCapacity) {
      flushLine();
      if (linesUsed >= m.maxLines) return nextOffset(tokenStart);
      return placeToken();
    }

    memcpy(candidate, line, lineLen);
    memcpy(candidate + lineLen, token, tokenLen);
    candidate[lineLen + tokenLen] = '\0';

    if (lineLen + tokenLen >= kLineCapacity || measure(candidate) > m.maxWidth) {
      flushLine();
      if (linesUsed >= m.maxLines) return nextOffset(tokenStart);
      return placeToken();
    }

    memcpy(line, candidate, lineLen + tokenLen);
    lineLen += tokenLen;
    tokenLen = 0;
    return 0;
  };

  while (in.available() && linesUsed < m.maxLines) {
    uint32_t charPos = in.position();
    int rb = in.read();
    if (rb < 0) break;
    char c = (char)rb;
    if (c == '\r') continue;

    if (c == '\n') {
      uint32_t forcedNext = placeToken();
      if (forcedNext != 0) return forcedNext;
      flushLine();
      if (linesUsed >= m.maxLines) return nextOffset(in.position());
      continue;
    }

    if (isBreakableWhitespaceByte(c)) {
      uint32_t forcedNext = placeToken();
      if (forcedNext != 0) return forcedNext;
      if (lineLen > 0 && line[lineLen - 1] != ' ' && lineLen < kLineCapacity - 1) {
        line[lineLen++] = ' ';
      }
      continue;
    }

    // A token longer than the scratch buffer is handled in pieces. This keeps
    // malformed or generated input from overflowing the fixed buffers.
    if (tokenLen >= kTokenCapacity - 1) {
      uint32_t forcedNext = placeToken();
      if (forcedNext != 0) return forcedNext;
    }
    if (tokenLen == 0) tokenStart = charPos;
    token[tokenLen++] = c;

    if (isBreakablePunctuationByte(c)) {
      uint32_t forcedNext = placeToken();
      if (forcedNext != 0) return forcedNext;
    }
  }

  uint32_t forcedNext = placeToken();
  if (forcedNext != 0) return forcedNext;

  if (linesUsed < m.maxLines && lineLen > 0) {
    flushLine();
  }

  return nextOffset(in.position());
}
