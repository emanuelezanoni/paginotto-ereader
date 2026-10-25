#ifndef PAGINOTTO_PURE_BIONIC_TEXT_H
#define PAGINOTTO_PURE_BIONIC_TEXT_H

// Return the byte length of the bold UTF-8 prefix for one word. Punctuation
// and unsplittable words return zero. No renderer or Arduino dependency.
int bionicPrefixBytesPure(const char* word, int byte_len);

#endif  // PAGINOTTO_PURE_BIONIC_TEXT_H
