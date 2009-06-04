/* libs/graphics/ports/SkFontHost_fontconfig_direct.h
**
** Copyright 2009, Google Inc.
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

#ifndef FontConfigIPC_DEFINED
#define FontConfigIPC_DEFINED

// http://code.google.com/p/chromium/wiki/LinuxSandboxIPC

#include <map>
#include <string>

#include "SkFontHost_fontconfig_impl.h"

class FontConfigIPC : public FontConfigInterface {
  public:
    FontConfigIPC(int fd);
    ~FontConfigIPC();

    // FontConfigInterface implementation.
    virtual bool Match(std::string* result_family, unsigned* result_fileid,
                       bool fileid_valid, unsigned fileid,
                       const std::string& family, int is_bold, int is_italic);
    virtual int Open(unsigned fileid);

    enum Method {
        METHOD_MATCH = 0,
        METHOD_OPEN = 1,
    };

    struct MatchRequest {
        uint16_t method;
        uint8_t fileid_valid;
        uint32_t fileid;
        int8_t is_bold;
        int8_t is_italic;
        uint8_t family_len;
        // char family[0] follows.
    };

    struct MatchReply {
        uint8_t result;
        uint32_t result_fileid;
        uint16_t filename_len;
        // char filename[0] follows.
    };

    struct OpenRequest {
        uint16_t method;
        unsigned fileid;
    };

    struct OpenReply {
        uint8_t result;
    };

  private:
    const int fd_;
};

#endif  // FontConfigIPC_DEFINED
