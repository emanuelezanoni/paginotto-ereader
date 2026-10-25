#include "test_framework.h"
#include "pure/paths.h"

TEST_CASE("stripTxtExt removes only .txt") {
  CHECK(stripTxtExt("book.txt") == "book");
  CHECK(stripTxtExt("book") == "book");
  CHECK(stripTxtExt("book.TXT") == "book.TXT");  // case-sensitive
  CHECK(stripTxtExt(".txt") == "");
  CHECK(stripTxtExt("") == "");
}

TEST_CASE("lastPathComponent") {
  CHECK(lastPathComponent("/books/a.txt") == "a.txt");
  CHECK(lastPathComponent("a.txt") == "a.txt");
  CHECK(lastPathComponent("/a/b/c") == "c");
  CHECK(lastPathComponent("") == "");
  CHECK(lastPathComponent("/") == "");
}

TEST_CASE("folderParent") {
  CHECK(folderParent("a/b/c") == "a/b");
  CHECK(folderParent("a") == "");
  CHECK(folderParent("") == "");
  CHECK(folderParent("a/b") == "a");
}

TEST_CASE("prettyRelativeLabel") {
  CHECK(prettyRelativeLabel("the_great_book.txt") == "the great book");
  CHECK(prettyRelativeLabel("classics/the_iliad.txt") == "classics / the iliad");
  CHECK(prettyRelativeLabel("plain") == "plain");
}

TEST_CASE("folderLeafLabel") {
  CHECK(folderLeafLabel("a/b/sci_fi") == "sci fi");
  CHECK(folderLeafLabel("sci_fi") == "sci fi");
}

TEST_CASE("isAllowedFolderByte covers ascii alnum, underscore, dash, space, high bytes") {
  CHECK(isAllowedFolderByte('a'));
  CHECK(isAllowedFolderByte('Z'));
  CHECK(isAllowedFolderByte('0'));
  CHECK(isAllowedFolderByte('_'));
  CHECK(isAllowedFolderByte('-'));
  CHECK(isAllowedFolderByte(' '));
  CHECK(isAllowedFolderByte(0xC3));  // UTF-8 lead byte
  CHECK(!isAllowedFolderByte('/'));
  CHECK(!isAllowedFolderByte('.'));
  CHECK(!isAllowedFolderByte(':'));
  CHECK(!isAllowedFolderByte('\\'));
}

TEST_CASE("sanitizeFolderSegment replaces disallowed bytes") {
  CHECK(sanitizeFolderSegment("hello/world") == "hello_world");
  CHECK(sanitizeFolderSegment("a.b") == "a_b");
  CHECK(sanitizeFolderSegment("   x   ") == "x");
  CHECK(sanitizeFolderSegment("") == "");
}

TEST_CASE("sanitizeFolderInput normalizes separators and drops dot segments") {
  CHECK(sanitizeFolderInput("a/b/c") == "a/b/c");
  CHECK(sanitizeFolderInput("a\\b\\c") == "a/b/c");
  CHECK(sanitizeFolderInput("a/./b") == "a/b");
  CHECK(sanitizeFolderInput("a/../b") == "a/b");
  CHECK(sanitizeFolderInput("///a///b///") == "a/b");
  CHECK(sanitizeFolderInput("") == "");
  CHECK(sanitizeFolderInput(".") == "");
  CHECK(sanitizeFolderInput("..") == "");
}

TEST_CASE("sanitizeUploadedFilename") {
  CHECK(sanitizeUploadedFilename("simple.txt") == "simple.txt");
  CHECK(sanitizeUploadedFilename("no_ext") == "no_ext.txt");
  CHECK(sanitizeUploadedFilename("path/to/file.txt") == "file.txt");
  CHECK(sanitizeUploadedFilename("..hidden.txt") == "hidden.txt");
  CHECK(sanitizeUploadedFilename("with..parent.txt") == "withparent.txt");
  CHECK(sanitizeUploadedFilename("weird!@#chars.txt") == "weird___chars.txt");
  // Numeric, punctuation in middle stays as filename
  CHECK(sanitizeUploadedFilename("a1-b2.txt") == "a1-b2.txt");
  CHECK(sanitizeUploadedFilename("...", 12) == "book.txt");
  CHECK(sanitizeUploadedFilename("long-name.txt", 8) == "long.txt");
  CHECK(sanitizeUploadedFilename("caf\xC3\xA9.txt", 8) == "caf.txt");
}

TEST_CASE("path labels handle empty and trailing separators") {
  CHECK(lastPathComponent("folder/") == "");
  CHECK(folderParent("folder/") == "folder");
  CHECK(folderLeafLabel("folder/") == "");
  CHECK(bookLeafLabel("/books/") == "");
  CHECK(prettyRelativeLabel("folder/book.txt") == "folder / book");
}

TEST_CASE("sanitizeFolderInput strips unsafe segments without creating traversal") {
  CHECK(sanitizeFolderInput("/safe/../../other/") == "safe/other");
  CHECK(sanitizeFolderInput("safe:folder/with*chars") == "safe_folder/with_chars");
  CHECK(sanitizeFolderInput("\\safe\\nested") == "safe/nested");
}

TEST_CASE("sanitizeUploadedFilename preserves UTF-8 boundaries at the limit") {
  CHECK(sanitizeUploadedFilename("na\xC3\xAFve.txt", 8) == "na\xC3\xAF.txt");
  CHECK(sanitizeUploadedFilename("na\xC3\xAFve.txt", 9) == "na\xC3\xAFv.txt");
  CHECK(sanitizeUploadedFilename("na\xC3\xAFve.txt", 10) == "na\xC3\xAFve.txt");
  CHECK(sanitizeUploadedFilename("/tmp/\xC3\xA9.txt") == "\xC3\xA9.txt");
}
