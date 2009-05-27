/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
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


// This file contains the declaration of the PrimitiveGL class.

#ifndef O3D_CORE_CROSS_GL_PRIMITIVE_GL_H_
#define O3D_CORE_CROSS_GL_PRIMITIVE_GL_H_

#include <map>
#include "core/cross/precompile.h"
#include "core/cross/primitive.h"
#include "core/cross/gl/param_cache_gl.h"

namespace o3d {

// PrimitiveGL is the OpenGL implementation of the Primitive.  It provides the
// necessary interfaces for setting the geometry streams on the Primitive.
class PrimitiveGL : public Primitive {
 public:
  explicit PrimitiveGL(ServiceLocator* service_locator);
  virtual ~PrimitiveGL();

  // Overridden from Element
  // Renders this Element using the parameters from override first, followed by
  // the draw_element, followed by params on this Primitive and material.
  virtual void Render(Renderer* renderer,
                      DrawElement* draw_element,
                      Material* material,
                      ParamObject* override,
                      ParamCache* param_cache);

 private:
};
}  // o3d

#endif  // O3D_CORE_CROSS_GL_PRIMITIVE_GL_H_
