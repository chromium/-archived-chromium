/*
** Copyright 2006, Google Inc.
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

// Don't include config.h here because there should be no dependency on WebKit
// at all.

#include "config.h"

#include "GraphicsContextPrivate.h"

#include "GraphicsContext.h"
#include "PlatformContextSkia.h"

namespace WebCore {

GraphicsContextPlatformPrivate::GraphicsContextPlatformPrivate(PlatformContextSkia* pgc)
    : SkPaintContext(pgc ? pgc->canvas() : NULL),
      m_context(pgc),
      m_should_delete(false)
{
    // Set back-reference.
    if (m_context)
        m_context->setPaintContext(this);
}

GraphicsContextPlatformPrivate::~GraphicsContextPlatformPrivate()
{
    if (m_should_delete)
        delete m_context;
}

}
