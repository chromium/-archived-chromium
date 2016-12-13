/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebFontInfo.h"

#include <string.h>

#include <unicode/utf16.h>
#include <fontconfig/fontconfig.h>

namespace WebKit {

WebString WebFontInfo::familyForChars(const WebUChar* characters, size_t numCharacters)
{
    FcCharSet* cset = FcCharSetCreate();
    for (int i = 0; i < numCharacters; ++i) {
        if (U16_IS_SURROGATE(characters[i])
         && U16_IS_SURROGATE_LEAD(characters[i])
         && i != numCharacters - 1
         && U16_IS_TRAIL(characters[i + 1])) {
              FcCharSetAddChar(cset, U16_GET_SUPPLEMENTARY(characters[i], characters[i+1]));
          i++;
        } else
              FcCharSetAddChar(cset, characters[i]);
    }
    FcPattern* pattern = FcPatternCreate();

    FcValue fcvalue;
    fcvalue.type = FcTypeCharSet;
    fcvalue.u.c = cset;
    FcPatternAdd(pattern, FC_CHARSET, fcvalue, 0);

    fcvalue.type = FcTypeBool;
    fcvalue.u.b = FcTrue;
    FcPatternAdd(pattern, FC_SCALABLE, fcvalue, 0);

    FcConfigSubstitute(0, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern* match = FcFontMatch(0, pattern, &result);
    FcPatternDestroy(pattern);
    FcCharSetDestroy(cset);

    if (match) {
        FcChar8* family;
        WebString result;
        if (FcPatternGetString(match, FC_FAMILY, 0, &family) == FcResultMatch) {
            const char* charFamily = reinterpret_cast<char*>(family);
            result = WebString::fromUTF8(charFamily, strlen(charFamily));
        }
        FcPatternDestroy(match);
        return result;
    }

    return WebString();
}

} // namespace WebKit
