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

#include "SkPaintContext.h"

class PlatformContextSkia;

namespace WebCore {

// This class is the interface to communicate to Skia. It is meant to be as
// opaque as possible. This class accept Skia native data format and not WebKit
// format.
// Every functions assume painting is enabled, callers should check this before
// calling any member function.
class GraphicsContextPlatformPrivate : public SkPaintContext {
public:
    GraphicsContextPlatformPrivate(PlatformContextSkia* pgc);
    ~GraphicsContextPlatformPrivate();

    PlatformContextSkia* platformContext() {
        return m_context;
    }

    void setShouldDelete(bool should_delete) {
        m_should_delete = should_delete;
    }

    // TODO(maruel): Eventually GraphicsContext should touch the canvas at all
    // to support serialization.
    using SkPaintContext::canvas;
private:
    PlatformContextSkia* m_context;
    bool m_should_delete;
};

}
