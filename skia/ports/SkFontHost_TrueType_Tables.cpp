/* libs/graphics/ports/SkFontHost_TrueType_VDMX.cpp
**
** Copyright 2008, Google Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

// -----------------------------------------------------------------------------
// VDMX parsing code.
//
// VDMX tables are found in some TrueType/OpenType fonts and contain
// ascender/descender overrides for certain (usually small) sizes. This is
// needed in order to match font metrics on Windows.
//
// Freetype does not parse these tables so we do so here. In the future we
// might support loading of arbitary fonts. This is not something that one
// would wish to do, dangerous as it is, so we are careful where we tread.
// -----------------------------------------------------------------------------

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

// For htons/ntohs
#include <arpa/inet.h>

// -----------------------------------------------------------------------------
// Buffer helper class
//
// This class perform some trival buffer operations while checking for
// out-of-bounds errors. As a family they return false if anything is amiss,
// updating the current offset otherwise.
// -----------------------------------------------------------------------------
class Buffer {
  public:
    Buffer(const uint8_t* buffer, size_t length)
        : buffer_(buffer),
          length_(length),
          offset_(0) { }

    bool Skip(size_t n_bytes) {
        if (offset_ + n_bytes > length_)
            return false;
        offset_ += n_bytes;
        return true;
    }

    bool ReadU8(uint8_t* value) {
        if (offset_ + 1 > length_)
            return false;
        *value = buffer_[offset_];
        offset_++;
        return true;
    }

    bool ReadU16(uint16_t* value) {
        if (offset_ + 2 > length_)
            return false;
        memcpy(value, buffer_ + offset_, sizeof(uint16_t));
        *value = ntohs(*value);
        offset_ += 2;
        return true;
    }

    bool ReadS16(int16_t* value) {
        return ReadU16(reinterpret_cast<uint16_t*>(value));
    }

    size_t offset() const { return offset_; }

    void set_offset(size_t newoffset) { offset_ = newoffset; }

  private:
    const uint8_t *const buffer_;
    const size_t length_;
    size_t offset_;
};

// -----------------------------------------------------------------------------
// Parse a TrueType VDMX table.
//   ymax: (output) the ascender value from the table
//   ymin: (output) the descender value from the table (negative!)
//   vdmx: the table bytes
//   vdmx_length: length of @vdmx, in bytes
//   target_pelsize: the pixel size of the font (e.g. 16)
//
// Returns true iff a suitable match are found. Otherwise, *ymax and *ymin are
// untouched. size_t must be 32-bits to avoid overflow.
//
// See http://www.microsoft.com/opentype/otspec/vdmx.htm
// -----------------------------------------------------------------------------
bool
VDMX_Parse(int* ymax, int* ymin,
           const uint8_t* vdmx, const size_t vdmx_length,
           const unsigned target_pelsize) {
    Buffer buf(vdmx, vdmx_length);

    // We ignore the version. Future tables should be backwards compatible with
    // this layout.
    uint16_t numratios;
    if (!buf.Skip(4) ||
        !buf.ReadU16(&numratios))
        return false;

    // Now we have two tables. Firstly we have @numratios Ratio records, then a
    // matching array of @numratios offsets. We save the offset of the beginning
    // of this second table.
    //
    // Range 6 <= x <= 262146
    unsigned long offset_table_offset =
        buf.offset() + 4 /* sizeof struct ratio */ * numratios;

    unsigned desired_ratio = 0xffffffff;
    // We read 4 bytes per record, so the offset range is
    //   6 <= x <= 524286
    for (unsigned i = 0; i < numratios; ++i) {
        uint8_t xratio, yratio1, yratio2;

        if (!buf.Skip(1) ||
            !buf.ReadU8(&xratio) ||
            !buf.ReadU8(&yratio1) ||
            !buf.ReadU8(&yratio2))
            return false;

        // This either covers 1:1, or this is the default entry (0, 0, 0)
        if ((xratio == 1 && yratio1 <= 1 && yratio2 >= 1) ||
            (xratio == 0 && yratio1 == 0 && yratio2 == 0)) {
            desired_ratio = i;
            break;
        }
    }

    if (desired_ratio == 0xffffffff) {
        // no ratio found
        return false;
    }

    // Range 10 <= x <= 393216
    buf.set_offset(offset_table_offset + sizeof(uint16_t) * desired_ratio);

    // Now we read from the offset table to get the offset of another array
    uint16_t group_offset;
    if (!buf.ReadU16(&group_offset))
        return false;
    // Range 0 <= x <= 65535
    buf.set_offset(group_offset);

    uint16_t num_records;
    if (!buf.ReadU16(&num_records) ||
        !buf.Skip(sizeof(uint16_t)))
        return false;

    // We read 6 bytes per record, so the offset range is
    //   4 <= x <= 458749
    for (unsigned i = 0; i < num_records; ++i) {
        uint16_t pel_size;
        if (!buf.ReadU16(&pel_size))
            return false;
        // the entries are sorted, so we can abort early if need be
        if (pel_size > target_pelsize)
            return false;

        if (pel_size == target_pelsize) {
            int16_t t_ymax, t_ymin;
            if (!buf.ReadS16(&t_ymax) ||
                !buf.ReadS16(&t_ymin))
                return false;
            *ymin = t_ymin;
            *ymax = t_ymax;
            return true;
        } else {
          if (!buf.Skip(2 * sizeof(int16_t)))
              return false;
        }
    }

    return false;
}
