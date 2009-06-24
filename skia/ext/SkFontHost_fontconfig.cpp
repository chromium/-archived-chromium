/* libs/graphics/ports/SkFontHost_fontconfig.cpp
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
// This file provides implementations of the font resolution members of
// SkFontHost by using the fontconfig[1] library. Fontconfig is usually found
// on Linux systems and handles configuration, parsing and caching issues
// involved with enumerating and matching fonts.
//
// [1] http://fontconfig.org
// -----------------------------------------------------------------------------

#include <map>
#include <string>

#include <unistd.h>
#include <sys/stat.h>

#include "SkFontHost.h"
#include "SkStream.h"
#include "SkFontHost_fontconfig_impl.h"
#include "SkFontHost_fontconfig_direct.h"
#include "SkFontHost_fontconfig_ipc.h"

static FontConfigInterface* global_fc_impl = NULL;

void SkiaFontConfigUseDirectImplementation() {
    if (global_fc_impl)
      delete global_fc_impl;
    global_fc_impl = new FontConfigDirect;
}

void SkiaFontConfigUseIPCImplementation(int fd) {
    if (global_fc_impl)
      delete global_fc_impl;
    global_fc_impl = new FontConfigIPC(fd);
}

static FontConfigInterface* GetFcImpl() {
  if (!global_fc_impl)
    global_fc_impl = new FontConfigDirect;
  return global_fc_impl;
}

static SkMutex global_fc_map_lock;
static std::map<uint32_t, SkTypeface *> global_fc_typefaces;

// This is the maximum size of the font cache.
static const unsigned kFontCacheMemoryBudget = 2 * 1024 * 1024;  // 2MB

// UniqueIds are encoded as (fileid << 8) | style

static unsigned UniqueIdToFileId(unsigned uniqueid)
{
    return uniqueid >> 8;
}

static SkTypeface::Style UniqueIdToStyle(unsigned uniqueid)
{
    return static_cast<SkTypeface::Style>(uniqueid & 0xff);
}

static unsigned FileIdAndStyleToUniqueId(unsigned fileid,
                                         SkTypeface::Style style)
{
    SkASSERT(style & 0xff == style);
    return (fileid << 8) | static_cast<int>(style);
}

class FontConfigTypeface : public SkTypeface {
public:
    FontConfigTypeface(Style style, uint32_t id)
        : SkTypeface(style, id)
    { }
};

// static
SkTypeface* SkFontHost::CreateTypeface(const SkTypeface* familyFace,
                                       const char familyName[],
                                       SkTypeface::Style style)
{
    std::string resolved_family_name;

    if (familyFace) {
        // Given the fileid we can ask fontconfig for the familyname of the
        // font.
        const unsigned fileid = UniqueIdToFileId(familyFace->uniqueID());
        if (!GetFcImpl()->Match(
          &resolved_family_name, NULL, true /* fileid valid */, fileid, "",
          NULL, NULL)) {
            return NULL;
        }
    } else if (familyName) {
        resolved_family_name = familyName;
    } else {
        return NULL;
    }

    bool bold = style & SkTypeface::kBold;
    bool italic = style & SkTypeface::kItalic;
    unsigned fileid;
    if (!GetFcImpl()->Match(NULL, &fileid, false, -1, /* no fileid */
                               resolved_family_name, &bold, &italic)) {
        return NULL;
    }
    const SkTypeface::Style resulting_style = static_cast<SkTypeface::Style>(
        (bold ? SkTypeface::kBold : 0) |
        (italic ? SkTypeface::kItalic : 0));

    const unsigned id = FileIdAndStyleToUniqueId(fileid, resulting_style);
    SkTypeface* typeface = SkNEW_ARGS(FontConfigTypeface, (resulting_style, id));

    {
        SkAutoMutexAcquire ac(global_fc_map_lock);
        global_fc_typefaces[id] = typeface;
    }

    return typeface;
}

// static
SkTypeface* SkFontHost::CreateTypefaceFromStream(SkStream* stream)
{
    SkASSERT(!"SkFontHost::CreateTypefaceFromStream unimplemented");
    return NULL;
}

// static
SkTypeface* SkFontHost::CreateTypefaceFromFile(const char path[])
{
    SkASSERT(!"SkFontHost::CreateTypefaceFromFile unimplemented");
    return NULL;
}

// static
bool SkFontHost::ValidFontID(SkFontID uniqueID) {
    SkAutoMutexAcquire ac(global_fc_map_lock);
    return global_fc_typefaces.find(uniqueID) != global_fc_typefaces.end();
}

void SkFontHost::Serialize(const SkTypeface*, SkWStream*) {
    SkASSERT(!"SkFontHost::Serialize unimplemented");
}

SkTypeface* SkFontHost::Deserialize(SkStream* stream) {
    SkASSERT(!"SkFontHost::Deserialize unimplemented");
    return NULL;
}

// static
uint32_t SkFontHost::NextLogicalFont(SkFontID fontID) {
    // We don't handle font fallback, WebKit does.
    return 0;
}

///////////////////////////////////////////////////////////////////////////////

class SkFileDescriptorStream : public SkStream {
  public:
    SkFileDescriptorStream(int fd)
        : fd_(fd) {
    }

    ~SkFileDescriptorStream() {
        close(fd_);
    }

    virtual bool rewind() {
        if (lseek(fd_, 0, SEEK_SET) == -1)
            return false;
        return true;
    }

    // SkStream implementation.
    virtual size_t read(void* buffer, size_t size) {
        if (!buffer && !size) {
            // This is request for the length of the stream.
            struct stat st;
            if (fstat(fd_, &st) == -1)
                return 0;
            return st.st_size;
        }

        if (!buffer) {
            // This is a request to skip bytes.
            const off_t current_position = lseek(fd_, 0, SEEK_CUR);
            if (current_position == -1)
                return 0;
            const off_t new_position = lseek(fd_, size, SEEK_CUR);
            if (new_position == -1)
                return 0;
            if (new_position < current_position) {
                lseek(fd_, current_position, SEEK_SET);
                return 0;
            }
            return new_position;
        }

        // This is a request to read bytes.
        return ::read(fd_, buffer, size);
    }

  private:
    const int fd_;
};

///////////////////////////////////////////////////////////////////////////////

// static
SkStream* SkFontHost::OpenStream(uint32_t id)
{
    const unsigned fileid = UniqueIdToFileId(id);
    const int fd = GetFcImpl()->Open(fileid);
    if (fd < 0)
        return NULL;

    return SkNEW_ARGS(SkFileDescriptorStream, (fd));
}

size_t SkFontHost::ShouldPurgeFontCache(size_t sizeAllocatedSoFar)
{
    if (sizeAllocatedSoFar > kFontCacheMemoryBudget)
        return sizeAllocatedSoFar - kFontCacheMemoryBudget;
    else
        return 0;   // nothing to do
}
