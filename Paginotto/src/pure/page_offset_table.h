#ifndef PALA_PURE_PAGE_OFFSET_TABLE_H
#define PALA_PURE_PAGE_OFFSET_TABLE_H

#include "arduino_compat.h"
#include "../config.h"

// Reader pagination state. Page offsets are deliberately NOT stored as an
// array in RAM: a pathological text file can contain an enormous number of
// tiny pages. The reader keeps only the offset of the page currently shown.
struct PageOffsetTable {
  uint32_t currentOffset = 0;
  bool eofReached = false;
};

#endif  // PALA_PURE_PAGE_OFFSET_TABLE_H
