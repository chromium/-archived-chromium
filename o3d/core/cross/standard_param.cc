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


// This file contains declarations for StandardParamMatrix4.

#include "core/cross/precompile.h"
#include "core/cross/transformation_context.h"
#include "core/cross/standard_param.h"
#include "core/cross/transform.h"

namespace o3d {
// create class defintions for RTTI
#undef O3D_STANDARD_ANNOTATION_ENTRY
#define O3D_STANDARD_ANNOTATION_ENTRY(enum_name, class_name)  \
  O3D_DEFN_CLASS(class_name ## ParamMatrix4, ParamMatrix4);

O3D_STANDARD_ANNOTATIONS;


template <>
void StandardParamMatrix4<WORLD>::ComputeValue() {
  set_read_only_value(transformation_context_->world());
}

template <>
void StandardParamMatrix4<WORLD_INVERSE>::ComputeValue() {
  set_read_only_value(inverse(transformation_context_->world()));
}

template <>
void StandardParamMatrix4<WORLD_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(transformation_context_->world()));
}

template <>
void StandardParamMatrix4<WORLD_INVERSE_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(inverse(transformation_context_->world())));
}

template <>
void StandardParamMatrix4<VIEW>::ComputeValue() {
  set_read_only_value(transformation_context_->view());
}

template <>
void StandardParamMatrix4<VIEW_INVERSE>::ComputeValue() {
  set_read_only_value(inverse(transformation_context_->view()));
}

template <>
void StandardParamMatrix4<VIEW_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(transformation_context_->view()));
}

template <>
void StandardParamMatrix4<VIEW_INVERSE_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(inverse(transformation_context_->view())));
}

template <>
void StandardParamMatrix4<PROJECTION>::ComputeValue() {
  set_read_only_value(transformation_context_->projection());
}

template <>
void StandardParamMatrix4<PROJECTION_INVERSE>::ComputeValue() {
  set_read_only_value(inverse(transformation_context_->projection()));
}

template <>
void StandardParamMatrix4<PROJECTION_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(transformation_context_->projection()));
}

template <>
void StandardParamMatrix4<PROJECTION_INVERSE_TRANSPOSE>::ComputeValue() {
  set_read_only_value(
      transpose(inverse(transformation_context_->projection())));
}

template <>
void StandardParamMatrix4<WORLD_VIEW>::ComputeValue() {
  set_read_only_value(transformation_context_->view() *
      transformation_context_->world());
}

template <>
void StandardParamMatrix4<WORLD_VIEW_INVERSE>::ComputeValue() {
  set_read_only_value(inverse(transformation_context_->view() *
      transformation_context_->world()));
}

template <>
void StandardParamMatrix4<WORLD_VIEW_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(transformation_context_->view() *
      transformation_context_->world()));
}

template <>
void StandardParamMatrix4<WORLD_VIEW_INVERSE_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(inverse(transformation_context_->view() *
      transformation_context_->world())));
}

template <>
void StandardParamMatrix4<VIEW_PROJECTION>::ComputeValue() {
  set_read_only_value(transformation_context_->view_projection());
}

template <>
void StandardParamMatrix4<VIEW_PROJECTION_INVERSE>::ComputeValue() {
  set_read_only_value(inverse(transformation_context_->view_projection()));
}

template <>
void StandardParamMatrix4<VIEW_PROJECTION_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(transformation_context_->view_projection()));
}

template <>
void StandardParamMatrix4<VIEW_PROJECTION_INVERSE_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(inverse(
      transformation_context_->view_projection())));
}

template <>
void StandardParamMatrix4<WORLD_VIEW_PROJECTION>::ComputeValue() {
  set_read_only_value(transformation_context_->world_view_projection());
}

template <>
void StandardParamMatrix4<WORLD_VIEW_PROJECTION_INVERSE>::ComputeValue() {
  set_read_only_value(inverse(transformation_context_->
      world_view_projection()));
}

template <>
void StandardParamMatrix4<WORLD_VIEW_PROJECTION_TRANSPOSE>::ComputeValue() {
  set_read_only_value(transpose(
      transformation_context_->world_view_projection()));
}

template <>
void StandardParamMatrix4<WORLD_VIEW_PROJECTION_INVERSE_TRANSPOSE>::
    ComputeValue() {
  set_read_only_value(transpose(inverse(
      transformation_context_->world_view_projection())));
}

}  // namespace o3d
