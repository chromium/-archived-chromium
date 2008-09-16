/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AtomicString.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "webkit_glue.h"

//
// This file contains implementations of methods in the "pending" version
// of FontCache. These implementations are derrived from Apple code to avoid
// having to fork the entire FontCacheMac.mm file just to add these additional
// methods, hence its copyright at the top of the file. 
//

namespace WebCore {

// TODO(jungshik): This may not be the best place to put this function. See
// TODO in pending/FontCache.h.
AtomicString FontCache::getGenericFontForScript(UScriptCode script, const FontDescription& description)
{
  if (webkit_glue::IsLayoutTestMode())
    return emptyAtom;
  // TODO(pinkerton) -- flesh this out with some script handling code
  return emptyAtom;
}

// default implementation taken from 
// WebCore/port/platform/graphics/FontCache.cpp. Windows makes lots of changes
// due to their font representations, we can probably stick with the original
// fallbacks for Mac. 
const AtomicString& FontCache::alternateFamilyName(const AtomicString& familyName)
{
    // Alias Courier <-> Courier New
    static AtomicString courier("Courier"), courierNew("Courier New");
    if (equalIgnoringCase(familyName, courier))
        return courierNew;
    if (equalIgnoringCase(familyName, courierNew))
        return courier;

    // Alias Times and Times New Roman.
    static AtomicString times("Times"), timesNewRoman("Times New Roman");
    if (equalIgnoringCase(familyName, times))
        return timesNewRoman;
    if (equalIgnoringCase(familyName, timesNewRoman))
        return times;
    
    // Alias Arial and Helvetica
    static AtomicString arial("Arial"), helvetica("Helvetica");
    if (equalIgnoringCase(familyName, arial))
        return helvetica;
    if (equalIgnoringCase(familyName, helvetica))
        return arial;

    return emptyAtom;
}

}  // namespace WebCore
