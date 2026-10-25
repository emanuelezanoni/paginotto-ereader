#include "test_framework.h"
#include "pure/stream.h"

TEST_CASE("StringReadStream reads bytes in order and reports EOF") {
  char raw[] = {'A', (char)0xC3, (char)0xA9};
  StringReadStream stream(raw);

  REQUIRE(stream.available());
  CHECK_EQ(stream.position(), 0u);
  CHECK_EQ(stream.size(), 3u);
  CHECK_EQ(stream.read(), 'A');
  CHECK_EQ(stream.read(), 0xC3);
  CHECK_EQ(stream.read(), 0xA9);
  CHECK_EQ(stream.read(), -1);
  CHECK_EQ(stream.read(), -1);
  CHECK(!stream.available());
  CHECK_EQ(stream.position(), 3u);
}

TEST_CASE("StringReadStream seek accepts EOF and rejects out of range") {
  StringReadStream stream("abc");

  CHECK(stream.seek(2));
  CHECK_EQ(stream.position(), 2u);
  CHECK_EQ(stream.read(), 'c');

  CHECK(stream.seek(0));
  CHECK_EQ(stream.read(), 'a');
  CHECK(stream.seek(3));
  CHECK(!stream.available());
  CHECK_EQ(stream.read(), -1);

  CHECK(!stream.seek(4));
  CHECK_EQ(stream.position(), 3u);
}

TEST_CASE("StringReadStream handles empty and null input") {
  StringReadStream empty("");
  CHECK_EQ(empty.size(), 0u);
  CHECK(!empty.available());
  CHECK_EQ(empty.read(), -1);
  CHECK(empty.seek(0));
  CHECK(!empty.seek(1));

  StringReadStream nullInput((const char*)nullptr);
  CHECK_EQ(nullInput.size(), 0u);
  CHECK_EQ(nullInput.read(), -1);
}
