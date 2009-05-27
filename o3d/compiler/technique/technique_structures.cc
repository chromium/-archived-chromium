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


#include <algorithm>
#include "base/logging.h"
#include "base/cross/std_functional.h"
#include "compiler/technique/technique_structures.h"

namespace o3d {

void TechniqueDeclaration::clear() {
  name.clear();
  annotation.clear();
  pass.clear();
}

void TechniqueDeclaration::dump() {
  DLOG(INFO) << "Dump of Technique \"" << name << "\"";
  DLOG(INFO) << "Technique Annotation count = " << annotation.size();
  std::for_each(annotation.begin(),
                annotation.end(),
                std::mem_fun_ref(&Annotation::dump));
  DLOG(INFO) << "Pass count = " << pass.size();
  std::for_each(pass.begin(),
                pass.end(),
                std::mem_fun_ref(&PassDeclaration::dump));
}

void PassDeclaration::dump() {
  DLOG(INFO) << "Pass \"" << name << "\"";
  DLOG(INFO) << "Pass Annotation count = " << annotation.size();
  std::for_each(annotation.begin(),
                annotation.end(),
                std::mem_fun_ref(&Annotation::dump));
  DLOG(INFO) << "Vertex shader \"" << vertex_shader_entry << "\"";
  DLOG(INFO) << "Vertex profile \"" << vertex_shader_profile << "\"";
  DLOG(INFO) << "Vertex args \"" << vertex_shader_arguments << "\"";
  DLOG(INFO) << "Fragment shader \"" << fragment_shader_entry << "\"";
  DLOG(INFO) << "Fragment profile \"" << fragment_shader_profile << "\"";
  DLOG(INFO) << "Fragment args \"" << fragment_shader_arguments << "\"";
  DLOG(INFO) << "State Assignment count = " << state_assignment.size();
  std::for_each(state_assignment.begin(),
                state_assignment.end(),
                std::mem_fun_ref(&StateAssignment::dump));
}

void StateAssignment::dump() {
  DLOG(INFO) << "State assignment name \"" << name << "\"";
  DLOG(INFO) << "State assignment value \"" << value << "\"";
}

void Annotation::dump() {
  DLOG(INFO) << "Annotation name \"" << name << "\"";
  DLOG(INFO) << "Annotation type \"" << type << "\"";
  DLOG(INFO) << "Annotation value \"" << value << "\"";
}

}  // namespace o3d
