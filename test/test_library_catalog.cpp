#include "test_framework.h"

#include <cstring>

#include "src/pure/library_catalog.h"

TEST_CASE("catalog folders are unique and sorted") {
  Catalog catalog;
  resetCatalog(catalog);

  CHECK(addCatalogFolder(catalog, "zeta"));
  CHECK(!addCatalogFolder(catalog, "zeta"));
  CHECK(addCatalogFolder(catalog, "alpha"));
  CHECK(addCatalogFolder(catalog, "middle"));
  sortCatalog(catalog);

  CHECK(catalog.folderCount == 3);
  CHECK(std::strcmp(catalog.folders[0], "alpha") == 0);
  CHECK(std::strcmp(catalog.folders[1], "middle") == 0);
  CHECK(std::strcmp(catalog.folders[2], "zeta") == 0);
}

TEST_CASE("catalog books sort by display name then path") {
  Catalog catalog;
  resetCatalog(catalog);

  CHECK(addCatalogBook(catalog, "/books/z/shared.txt", "z", "Shared", 20));
  CHECK(addCatalogBook(catalog, "/books/a/shared.txt", "a", "Shared", 10));
  CHECK(addCatalogBook(catalog, "/books/a/alpha.txt", "a", "Alpha", 5));
  sortCatalog(catalog);

  CHECK(catalog.bookCount == 3);
  CHECK(std::strcmp(catalog.books[0].name, "Alpha") == 0);
  CHECK(std::strcmp(catalog.books[1].path, "/books/a/shared.txt") == 0);
  CHECK(std::strcmp(catalog.books[2].path, "/books/z/shared.txt") == 0);
}

TEST_CASE("catalog rejects invalid or over-capacity entries") {
  Catalog catalog;
  resetCatalog(catalog);

  CHECK(!addCatalogFolder(catalog, nullptr));
  CHECK(!addCatalogFolder(catalog, ""));
  CHECK(!addCatalogBook(catalog, nullptr, "", "", 0));
  CHECK(addCatalogBook(catalog, "/books/book.txt", "", "Book", 42));
  CHECK(catalog.bookCount == 1);
}

TEST_CASE("catalog safely truncates fixed-size metadata") {
  Catalog catalog;
  resetCatalog(catalog);
  const char* longPath =
      "/books/this/path/is/longer/than/the/fixed/catalog/path/buffer.txt";
  const char* longName =
      "A title that is longer than the fixed book name buffer";

  CHECK(addCatalogBook(catalog, longPath, "folder", longName, 123));
  CHECK(catalog.books[0].path[95] == '\0');
  CHECK(catalog.books[0].name[79] == '\0');
  CHECK(catalog.books[0].folder[MAX_FOLDER_PATH] == '\0');
  CHECK(catalog.books[0].size == 123);
}
