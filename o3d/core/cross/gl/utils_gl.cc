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


#include "core/cross/precompile.h"
#include "core/cross/stream.h"
#include "core/cross/types.h"
#include "core/cross/gl/utils_gl.h"

// Required OpenGL extensions:
// GL_ARB_vertex_buffer_object
// GL_ARB_vertex_program
// GL_ARB_texture_compression
// GL_EXT_texture_compression_dxt1

namespace o3d {

typedef std::pair<String, int> SemanticMapElement;
typedef std::map<String, int> SemanticMap;

// The map batween the semantics on vertex program varying parameters names
// and vertex attribute indices under the VP_30 profile.
SemanticMapElement semantic_map_vp_30[] = {
  SemanticMapElement("POSITION", 0),
  SemanticMapElement("ATTR0",    0),
  SemanticMapElement("BLENDWEIGHT", 1),
  SemanticMapElement("ATTR1",       1),
  SemanticMapElement("NORMAL", 2),
  SemanticMapElement("ATTR2",  2),
  SemanticMapElement("COLOR0",  3),
  SemanticMapElement("DIFFUSE", 3),
  SemanticMapElement("ATTR3",   3),
  SemanticMapElement("COLOR1",   4),
  SemanticMapElement("SPECULAR", 4),
  SemanticMapElement("ATTR4",    4),
  SemanticMapElement("TESSFACTOR", 5),
  SemanticMapElement("FOGCOORD",   5),
  SemanticMapElement("ATTR5",      5),
  SemanticMapElement("PSIZE", 6),
  SemanticMapElement("ATTR6", 6),
  SemanticMapElement("BLENDINDICES", 7),
  SemanticMapElement("ATTR7",        7),
  SemanticMapElement("TEXCOORD0", 8),
  SemanticMapElement("ATTR8",     8),
  SemanticMapElement("TEXCOORD1", 9),
  SemanticMapElement("ATTR9",     9),
  SemanticMapElement("TEXCOORD2", 10),
  SemanticMapElement("ATTR10",    10),
  SemanticMapElement("TEXCOORD3", 11),
  SemanticMapElement("ATTR11",    11),
  SemanticMapElement("TEXCOORD4", 12),
  SemanticMapElement("ATTR12",    12),
  SemanticMapElement("TEXCOORD5", 13),
  SemanticMapElement("ATTR13",    13),
  SemanticMapElement("TEXCOORD6", 14),
  SemanticMapElement("TANGENT",   14),
  SemanticMapElement("ATTR14",    14),
  SemanticMapElement("TEXCOORD7", 15),
  SemanticMapElement("BINORMAL",  15),
  SemanticMapElement("ATTR15",    15),
};

// The map batween the semantics on vertex program varying parameters names
// and vertex attribute indices under the VP_40 profile.
SemanticMapElement semantic_map_vp_40[] = {
  SemanticMapElement("POSITION", 0),
  SemanticMapElement("POSITION0", 0),
  SemanticMapElement("ATTR0",    0),
  SemanticMapElement("BLENDWEIGHT",  1),
  SemanticMapElement("BLENDWEIGHT0", 1),
  SemanticMapElement("ATTR1",        1),
  SemanticMapElement("NORMAL",  2),
  SemanticMapElement("NORMAL0", 2),
  SemanticMapElement("ATTR2",   2),
  SemanticMapElement("COLOR",   3),
  SemanticMapElement("COLOR0",  3),
  SemanticMapElement("DIFFUSE", 3),
  SemanticMapElement("ATTR3",   3),
  SemanticMapElement("COLOR1",   4),
  SemanticMapElement("SPECULAR", 4),
  SemanticMapElement("ATTR4",    4),
  SemanticMapElement("TESSFACTOR",  5),
  SemanticMapElement("FOGCOORD",    5),
  SemanticMapElement("TESSFACTOR0", 5),
  SemanticMapElement("FOGCOORD0",   5),
  SemanticMapElement("ATTR5",       5),
  SemanticMapElement("PSIZE",  6),
  SemanticMapElement("PSIZE0", 6),
  SemanticMapElement("ATTR6",  6),
  SemanticMapElement("BLENDINDICES",  7),
  SemanticMapElement("BLENDINDICES0", 7),
  SemanticMapElement("ATTR7",         7),
  SemanticMapElement("TEXCOORD",  8),
  SemanticMapElement("TEXCOORD0", 8),
  SemanticMapElement("ATTR8",     8),
  SemanticMapElement("TEXCOORD1", 9),
  SemanticMapElement("ATTR9",     9),
  SemanticMapElement("TEXCOORD2", 10),
  SemanticMapElement("ATTR10",    10),
  SemanticMapElement("TEXCOORD3", 11),
  SemanticMapElement("ATTR11",    11),
  SemanticMapElement("TEXCOORD4", 12),
  SemanticMapElement("ATTR12",    12),
  SemanticMapElement("TEXCOORD5", 13),
  SemanticMapElement("ATTR13",    13),
  SemanticMapElement("TANGENT",   14),
  SemanticMapElement("TANGENT0",  14),
  SemanticMapElement("TEXCOORD6", 14),
  SemanticMapElement("ATTR14",    14),
  SemanticMapElement("BINORMAL",  15),
  SemanticMapElement("BINORMAL0", 15),
  SemanticMapElement("TEXCOORD7", 15),
  SemanticMapElement("ATTR15",    15),
};

// The map batween OpenGL Vertex Attribute indexes under the VP_40 profile
// to Stream::Semantic identifiers (with index offsets).
struct AttrMapElement {
  AttrMapElement(Stream::Semantic s, int i) : semantic(s), index(i) {}
  Stream::Semantic semantic;
  int index;
};
AttrMapElement attr_map_vp_40[] = {
  AttrMapElement(Stream::POSITION, 0),
  AttrMapElement(Stream::UNKNOWN_SEMANTIC,  0),
  AttrMapElement(Stream::NORMAL,   0),
  AttrMapElement(Stream::COLOR,    0),
  AttrMapElement(Stream::COLOR,    1),
  AttrMapElement(Stream::UNKNOWN_SEMANTIC,  0),
  AttrMapElement(Stream::UNKNOWN_SEMANTIC,  0),
  AttrMapElement(Stream::UNKNOWN_SEMANTIC,  0),
  AttrMapElement(Stream::TEXCOORD, 0),
  AttrMapElement(Stream::TEXCOORD, 1),
  AttrMapElement(Stream::TEXCOORD, 2),
  AttrMapElement(Stream::TEXCOORD, 3),
  AttrMapElement(Stream::TEXCOORD, 4),
  AttrMapElement(Stream::TEXCOORD, 5),
  AttrMapElement(Stream::TANGENT,  0),
  AttrMapElement(Stream::BINORMAL, 0),
};

// TODO: make this choice a runtime decision in RendererGL
// initialisation.
static SemanticMap semantic_map(semantic_map_vp_40,
                                semantic_map_vp_40 +
                                sizeof(semantic_map_vp_40) /
                                sizeof(SemanticMapElement) );

// Converts a semantic string to an OpenGL vertex attribute number using the
// standard VP_40 shader semantic mappings. If the semantic is not
// recognised, it returns an index of -1.
int SemanticNameToGLVertexAttribute(const char* semantic) {
  SemanticMap::const_iterator i = semantic_map.find(semantic);
  if (i == semantic_map.end()) {
    return -1;
  }
  return i->second;
}

// Given a vertex attribute stream, convert it to a Stream::Semantic number
// and index. This is an imprecise operation.
Stream::Semantic GLVertexAttributeToStream(const unsigned int attr,
                                           int *index) {
  // kMaxAttrIndex is available from:
  //    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
  //
  // TODO: make this a runtime provided value discovered during
  // Renderer creation.
  const int kMaxAttrIndex = 15;
  if (attr > kMaxAttrIndex) {
    //TODO: Figure out how to get errors out of here to the client.
    DLOG(ERROR) << "Invalid vertex attribute index.";
    *index = 0;
    return Stream::UNKNOWN_SEMANTIC;
  }
  *index = attr_map_vp_40[attr].index;
  return attr_map_vp_40[attr].semantic;
}

#ifdef OS_WIN

// Given a CGcontext object, check to see if any errors have occurred since
// the last Cg API call, and report the message and any compiler errors (if
// necessary).
inline void CheckForCgError(const String& logmessage, CGcontext cg_context) {
  CGerror error = CG_NO_ERROR;
  const char *error_string = cgGetLastErrorString(&error);
  if (error == CG_NO_ERROR) {
    return;
  } else {
    DLOG(ERROR) << logmessage << ": " << error_string;
    if (error == CG_COMPILER_ERROR) {
      DLOG(ERROR) << "Compiler message:\n" << cgGetLastListing(cg_context);
    }
  }
}

#endif

}  // namespace o3d
