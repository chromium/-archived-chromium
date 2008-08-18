#include "SkTypeface.h"

// ===== Begin Chrome-specific definitions =====

uint32_t SkTypeface::UniqueID(const SkTypeface* face)
{
    return 0;
}

void SkTypeface::serialize(SkWStream* stream) const {
}

SkTypeface* SkTypeface::Deserialize(SkStream* stream) {
  return NULL;
}

// ===== End Chrome-specific definitions =====
