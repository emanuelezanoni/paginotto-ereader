#include "paths.h"

namespace {

bool isAllowedFilenameByte(uint8_t c) {
  return ((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '_' || c == '-' || c == ' ' || c == '.' || c >= 128);
}

String sanitizeFilenameBytes(const String& input) {
  String clean;
  clean.reserve(input.length());
  for (size_t i = 0; i < input.length(); ++i) {
    const uint8_t c = (uint8_t)input[i];
    clean += isAllowedFilenameByte(c) ? (char)c : '_';
  }
  return clean;
}

String truncateUtf8Stem(const String& filename, size_t maxBytes) {
  if (maxBytes == 0 || filename.length() <= maxBytes || maxBytes <= 4) {
    return filename;
  }

  const size_t stemBytes = maxBytes - 4;  // reserve the ".txt" suffix
  size_t cut = stemBytes;
  while (cut > 0 && (((uint8_t)filename[cut] & 0xC0) == 0x80)) {
    --cut;
  }

  // A too-small UTF-8 prefix is less useful than the deterministic fallback.
  return cut == 0 ? String("book.txt") : filename.substring(0, cut) + ".txt";
}

}  // namespace

String stripTxtExt(const String& s) {
  return s.endsWith(".txt") ? s.substring(0, s.length() - 4) : s;
}

String lastPathComponent(const String& path) {
  const int slash = path.lastIndexOf('/');
  return slash < 0 ? path : path.substring(slash + 1);
}

String folderParent(const String& relPath) {
  const int slash = relPath.lastIndexOf('/');
  return slash < 0 ? String("") : relPath.substring(0, slash);
}

String prettyRelativeLabel(const String& relPath) {
  String label;
  label.reserve(relPath.length() + 8);
  for (size_t i = 0; i < relPath.length(); ++i) {
    const char c = relPath[i];
    if (c == '_') label += ' ';
    else if (c == '/') label += " / ";
    else label += c;
  }
  return stripTxtExt(label);
}

String folderLeafLabel(const String& relPath) {
  String leaf = lastPathComponent(relPath);
  leaf.replace('_', ' ');
  return leaf;
}

String bookLeafLabel(const String& path) {
  String leaf = stripTxtExt(lastPathComponent(path));
  leaf.replace('_', ' ');
  return leaf;
}

bool isAllowedFolderByte(uint8_t c) {
  return ((c >= 'a' && c <= 'z') ||
          (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') ||
          c == '_' || c == '-' || c == ' ' || c >= 128);
}

String sanitizeFolderSegment(const String& segment) {
  String clean;
  clean.reserve(segment.length());
  for (size_t i = 0; i < segment.length(); ++i) {
    const uint8_t c = (uint8_t)segment[i];
    clean += isAllowedFolderByte(c) ? (char)c : '_';
  }
  clean.trim();
  return clean;
}

String sanitizeFolderInput(const String& raw) {
  String normalized = raw;
  normalized.replace('\\', '/');

  String result;
  int cursor = 0;
  while (cursor <= (int)normalized.length()) {
    const int slash = normalized.indexOf('/', cursor);
    String segment = slash < 0 ? normalized.substring(cursor)
                               : normalized.substring(cursor, slash);
    cursor = slash < 0 ? (int)normalized.length() + 1 : slash + 1;

    segment.trim();
    if (segment.length() == 0 || segment == "." || segment == "..") continue;
    segment = sanitizeFolderSegment(segment);
    if (segment.length() == 0) continue;
    if (result.length() > 0) result += '/';
    result += segment;
  }
  return result;
}

String sanitizeUploadedFilename(String fname, size_t maxBytes) {
  const int slash = fname.lastIndexOf('/');
  if (slash >= 0) fname = fname.substring(slash + 1);

  String clean = sanitizeFilenameBytes(fname);
  clean.replace("..", "");
  while (clean.startsWith(".")) clean.remove(0, 1);
  if (clean.length() == 0) clean = "book";
  if (!clean.endsWith(".txt")) clean += ".txt";

  return truncateUtf8Stem(clean, maxBytes);
}
