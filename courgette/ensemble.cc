// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// *** File comments

#include "courgette/ensemble.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"

#include "courgette/image_info.h"
#include "courgette/region.h"
#include "courgette/streams.h"
#include "courgette/simple_delta.h"

namespace courgette {

Element::Element(Kind kind, Ensemble* ensemble, const Region& region)
    : kind_(kind), ensemble_(ensemble), region_(region) {
}

std::string Element::Name() const {
  return ensemble_->name() + "("
      + IntToString(kind()) + ","
      + Uint64ToString(offset_in_ensemble()) + ","
      + Uint64ToString(region().length()) + ")";
}

// A subclass of Element that has a PEInfo.
class ElementWinPE : public Element {
 public:
  ElementWinPE(Kind kind, Ensemble* ensemble, const Region& region,
               PEInfo* info)
      : Element(kind, ensemble, region),
        pe_info_(info) {
  }

  virtual PEInfo* GetPEInfo() const { return pe_info_; }

 protected:
  ~ElementWinPE() { delete pe_info_; }

 private:
  PEInfo* pe_info_;  // Owned by |this|.
};

// Scans the Ensemble's region, sniffing out Elements.  We assume that the
// elements do not overlap.
Status Ensemble::FindEmbeddedElements() {
  size_t length = region_.length();
  const uint8* start = region_.start();

  size_t position = 0;
  while (position < length) {
    // Quick test; Windows executables begin with 'MZ'.
    if (start[position] == 'M' &&
        position + 1 < length && start[position + 1] == 'Z') {
      courgette::PEInfo *info = new courgette::PEInfo();
      info->Init(start + position, length - position);
      if (info->ParseHeader()) {
        Region region(start + position, info->length());

        if (info->has_text_section()) {
          Element* element = new ElementWinPE(Element::WIN32_X86_WITH_CODE,
                                              this, region, info);
          owned_elements_.push_back(element);
          elements_.push_back(element);
          position += region.length();
          continue;
        }

        // If we had a clever transformation for resource-only executables we
        // should identify the suitable elements here:
        if (!info->has_text_section() && false) {
          Element* element = new ElementWinPE(Element::WIN32_NOCODE,
                                              this, region, info);
          owned_elements_.push_back(element);
          elements_.push_back(element);
          position += region.length();
          continue;
        }
      }
      delete info;
    }

    // This is where to add new formats, e.g. Linux executables, Dalvik
    // executables etc.

    // No Element found at current position.
    ++position;
  }
  return C_OK;
}

Ensemble::~Ensemble() {
  for (size_t i = 0;  i < owned_elements_.size();  ++i)
    delete owned_elements_[i];
}

}  // namespace
