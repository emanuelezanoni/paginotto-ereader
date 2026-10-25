#include "test_framework.h"
#include "pure/bionic_text.h"

TEST_CASE("bionic prefix splits ordinary words without splitting UTF-8") {
  CHECK_EQ(bionicPrefixBytesPure("reader", 6), 3);
  const char accented[] = "cafe\xC3\xA9";
  CHECK_EQ(bionicPrefixBytesPure(accented, 6), 2);
}

TEST_CASE("bionic prefix rejects punctuation and unsplittable words") {
  CHECK_EQ(bionicPrefixBytesPure("...", 3), 0);
  CHECK_EQ(bionicPrefixBytesPure("a", 1), 0);
  CHECK_EQ(bionicPrefixBytesPure(nullptr, 0), 0);
}
