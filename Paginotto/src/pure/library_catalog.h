#ifndef PAGINOTTO_PURE_LIBRARY_CATALOG_H
#define PAGINOTTO_PURE_LIBRARY_CATALOG_H

#include "library_nav.h"

// Pure catalog mutation and ordering. The LittleFS walker calls these
// helpers, while host tests exercise the bounds and ordering rules without
// requiring an ESP32 filesystem.
void resetCatalog(Catalog& catalog);
bool addCatalogFolder(Catalog& catalog, const char* folder);
bool addCatalogBook(Catalog& catalog, const char* path, const char* folder,
                    const char* name, size_t size);
void sortCatalog(Catalog& catalog);

#endif  // PAGINOTTO_PURE_LIBRARY_CATALOG_H
