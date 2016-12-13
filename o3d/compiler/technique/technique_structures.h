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


#ifndef O3D_COMPILER_TECHNIQUE_TECHNIQUE_STRUCTURES_H_
#define O3D_COMPILER_TECHNIQUE_TECHNIQUE_STRUCTURES_H_

#include <vector>
#include "core/cross/types.h"

namespace o3d {

// typedefs --------------------------------------------------------------------

class TechniqueDeclaration;
class SamplerState;

typedef std::vector<TechniqueDeclaration> TechniqueDeclarationList;
typedef std::vector<SamplerState> SamplerStateList;

// classes ---------------------------------------------------------------------

// A set of simple data classes that hold the structured information
// uncovered by the Technique parser. All values are public as providing
// accessors for each member would be pretty pointless. If a field is
// missing in the parsed FX file the matching field will either hold an
// empty string or have zero entries in the container.
//
// TODO: if it proves necessary, interpret the "value" fields into
// binary form as opposed to the current model of leaving them as strings.

class Annotation {
 public:
  Annotation() {}
  Annotation(String t, String n, String v) : type(t), name(n), value(v) {}
  ~Annotation() {}
  void dump();

  String type;
  String name;
  String value;
};

class StateAssignment {
 public:
  StateAssignment() {}
  StateAssignment(String n, String v) : name(n), value(v) {}
  ~StateAssignment() {}
  void dump();

  String name;
  String value;
};

class PassDeclaration {
 public:
  PassDeclaration() {}
  explicit PassDeclaration(String n) : name(n) {}
  ~PassDeclaration() {}
  void dump();

  String name;
  std::vector<Annotation> annotation;
  String vertex_shader_entry;
  String vertex_shader_profile;
  String vertex_shader_arguments;
  String fragment_shader_entry;
  String fragment_shader_profile;
  String fragment_shader_arguments;
  std::vector<StateAssignment> state_assignment;
};

class TechniqueDeclaration {
 public:
  TechniqueDeclaration() {}
  ~TechniqueDeclaration() {}
  void clear();
  void dump();

  String name;
  std::vector<Annotation> annotation;
  std::vector<PassDeclaration> pass;
};

class SamplerState {
 public:
  SamplerState() {}
  ~SamplerState() {}

  String name;
  String texture;
  String address_u;
  String address_v;
  String address_w;
  String min_filter;
  String mag_filter;
  String mip_filter;
  String border_color;
  String max_anisotropy;
};

}  // namespeace o3d

#endif  // O3D_COMPILER_TECHNIQUE_TECHNIQUE_STRUCTURES_H_
