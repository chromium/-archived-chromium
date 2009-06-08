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


// This file contains the implementation of class Skin.

#include "core/cross/precompile.h"
#include "core/cross/skin.h"
#include "core/cross/error.h"
#include "import/cross/memory_stream.h"
#include "import/cross/raw_data.h"

namespace o3d {

O3D_DEFN_CLASS(Skin, NamedObject);
O3D_DEFN_CLASS(ParamSkin, RefParamBase);
O3D_DEFN_CLASS(SkinEval, VertexSource);

const char *Skin::kSerializationID = "SKIN";

Skin::Skin(ServiceLocator* service_locator)
    : NamedObject(service_locator),
      weak_pointer_manager_(this),
      highest_matrix_index_(0),
      highest_influences_(0),
      info_valid_(false) {
}

const Skin::Influences* Skin::GetVertexInfluences(unsigned vertex_index) const {
  return (vertex_index < influences_array_.size()) ?
      &influences_array_[vertex_index] : NULL;
}

void Skin::SetVertexInfluences(unsigned vertex_index,
                               const Skin::Influences& influences) {
  if (influences_array_.size() <= vertex_index) {
    influences_array_.resize(vertex_index + 1);
  }

  // Should we normalize the influences? I think no. The user can always
  // normalize them and if they don't maybe that's the way they wanted them to
  // achieve some effect.
  influences_array_[vertex_index] = influences;

  info_valid_ = false;
}

void Skin::UpdateInfo() const {
  if (!info_valid_) {
    info_valid_ = true;
    highest_matrix_index_ = 0;
    highest_influences_ = 0;
    for (unsigned ii = 0; ii < influences_array_.size(); ++ii) {
      const Influences& influences = influences_array_[ii];
      if (influences.size() > highest_influences_) {
        highest_influences_ = influences.size();
      }
      for (unsigned jj = 0; jj < influences.size(); ++jj) {
        if (influences[jj].matrix_index > highest_matrix_index_) {
          highest_matrix_index_ = influences[jj].matrix_index;
        }
      }
    }
  }
}

unsigned Skin::GetHighestMatrixIndex() const {
  UpdateInfo();
  return highest_matrix_index_;
}

unsigned Skin::GetHighestInfluences() const {
  UpdateInfo();
  return highest_influences_;
}

void Skin::SetInverseBindPoseMatrix(unsigned index, const Matrix4& matrix) {
  if (inverse_bind_pose_matrices_.size() <= index) {
    inverse_bind_pose_matrices_.resize(index + 1, Matrix4::identity());
  }
  inverse_bind_pose_matrices_[index] = matrix;
}

ObjectBase::Ref Skin::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Skin(service_locator));
}

SkinEval::StreamInfo::StreamInfo()
    : data_(NULL),
      buffer_(NULL),
      values_(NULL),
      stride_(0),
      compute_function_(NULL),
      copy_function_(NULL) {
}

namespace {

void ComputeFloat3AsVector3(float* destination,
                            const float* source,
                            const Matrix4& matrix) {
  Vector4 result(matrix * Vector3(source[0], source[1], source[2]));
  destination[0] = result.getElem(0);
  destination[1] = result.getElem(1);
  destination[2] = result.getElem(2);
}

void ComputeFloat3AsPoint3(float* destination,
                           const float* source,
                           const Matrix4& matrix) {
  Vector4 result(matrix * Point3(source[0], source[1], source[2]));
  destination[0] = result.getElem(0);
  destination[1] = result.getElem(1);
  destination[2] = result.getElem(2);
}

void ComputeFloat4AsVector4(float* destination,
                            const float* source,
                            const Matrix4& matrix) {
  Vector4 result(matrix * Vector4(source[0], source[1], source[2], source[3]));
  destination[0] = result.getElem(0);
  destination[1] = result.getElem(1);
  destination[2] = result.getElem(2);
  destination[3] = result.getElem(3);
}

void CopyFloat3(float* destination,
                const float* source) {
  destination[0] = source[0];
  destination[1] = source[1];
  destination[2] = source[2];
}

void CopyFloat4(float* destination,
                const float* source) {
  destination[0] = source[0];
  destination[1] = source[1];
  destination[2] = source[2];
  destination[3] = source[3];
}

}  // anonymous namespace

bool SkinEval::StreamInfo::Init(const Stream& stream,
                                Buffer::AccessMode access_mode) {
  DCHECK(data_ == NULL);
  DCHECK(buffer_ == NULL);
  const Field& field = stream.field();
  Buffer* buffer = field.buffer();
  if (!buffer || !field.IsA(FloatField::GetApparentClass())) {
    // TODO: Figure out a way to return a better error.
    return false;
  }
  switch (field.num_components()) {
    case 3:
      copy_function_ = CopyFloat3;
      compute_function_ = (stream.semantic() == Stream::POSITION) ?
          ComputeFloat3AsPoint3 : ComputeFloat3AsVector3;
      break;
    case 4:
      compute_function_ = ComputeFloat4AsVector4;
      copy_function_ = CopyFloat4;
      break;
    default:
      // TODO: Figure out a way to return a better error.
      return false;
  }

  bool success = buffer->Lock(access_mode, &data_);
  if (success) {
    values_ = PointerFromVoidPointer<float*>(data_, field.offset());
    stride_ = buffer->stride();
    buffer_ = buffer;
  }
  return success;
}

void SkinEval::StreamInfo::Uninit() {
  if (data_) {
    DCHECK(buffer_);
    buffer_->Unlock();
    data_ = NULL;
    buffer_ = NULL;
  }
}

const char* SkinEval::kMatricesParamName = O3D_STRING_CONSTANT("matrices");
const char* SkinEval::kSkinParamName = O3D_STRING_CONSTANT("skin");
const char* SkinEval::kBaseParamName = O3D_STRING_CONSTANT("base");

SkinEval::SkinEval(ServiceLocator* service_locator)
    : VertexSource(service_locator) {
  RegisterParamRef(kMatricesParamName, &matrices_param_);
  RegisterParamRef(kSkinParamName, &skin_param_);
  RegisterParamRef(kBaseParamName, &base_param_);
}

bool SkinEval::SetVertexStream(Stream::Semantic semantic,
                               int semantic_index,
                               Field* field,
                               unsigned int start_index) {
  Buffer* buffer = field->buffer();
  if (!buffer) {
    O3D_ERROR(service_locator()) << "No buffer on field";
    return false;
  }
  if (!buffer->IsA(SourceBuffer::GetApparentClass())) {
    O3D_ERROR(service_locator()) << "Buffer is not a SourceBuffer";
    return false;
  }

  Stream::Ref stream(new Stream(service_locator(),
                                field,
                                start_index,
                                semantic,
                                semantic_index));

  // If a stream with the same semantic has already been set then remove it.
  RemoveVertexStream(semantic, semantic_index);

  ParamVertexBufferStream::Ref stream_param(new SlaveParamVertexBufferStream(
     service_locator(), this, stream));
  vertex_stream_params_.push_back(stream_param);

  return true;
}

bool SkinEval::RemoveVertexStream(Stream::Semantic stream_semantic,
                                  int semantic_index) {
  StreamParamVector::iterator iter, end = vertex_stream_params_.end();
  for (iter = vertex_stream_params_.begin(); iter != end; ++iter) {
    const Stream& stream = (*iter)->stream();
    if (stream.semantic() == stream_semantic &&
        stream.semantic_index() == semantic_index) {
      vertex_stream_params_.erase(iter);
      return true;
    }
  }
  return false;
}

const Stream* SkinEval::GetVertexStream(
    Stream::Semantic stream_semantic,
    int semantic_index) const {
  ParamVertexBufferStream* param = GetVertexStreamParam(stream_semantic,
                                                        semantic_index);
  return param ? &param->stream() : NULL;
}

ParamVertexBufferStream* SkinEval::GetVertexStreamParam(
    Stream::Semantic semantic,
    int semantic_index) const {
  StreamParamVector::const_iterator iter, end = vertex_stream_params_.end();
  for (iter = vertex_stream_params_.begin(); iter != end; ++iter) {
    const Stream& stream = (*iter)->stream();
    if (stream.semantic() == semantic &&
        stream.semantic_index() == semantic_index) {
      return *iter;
    }
  }
  return NULL;
}

void SkinEval::UpdateStreams() {
  for (unsigned ii = 0; ii < vertex_stream_params_.size(); ++ii) {
    vertex_stream_params_[ii]->UpdateStream();
  }
}

void SkinEval::DoSkinning(Skin* skin) {
  const Skin::InfluencesArray& influences_array = skin->influences();
  unsigned num_streams = vertex_stream_params_.size();

  if (input_stream_infos_.size() != num_streams) {
    input_stream_infos_.resize(num_streams);
    output_stream_infos_.resize(num_streams);
  }

  unsigned num_vertices = influences_array.size();
  // Update our inputs, lock all the inputs and outputs and check that we have
  // the same number of vertices as vertex influences.
  for (unsigned ii = 0; ii < num_streams; ++ii) {
    ParamVertexBufferStream* source_param = vertex_stream_params_[ii];

    // Make sure our upstream streams are ready
    ParamVertexBufferStream* input = down_cast<ParamVertexBufferStream*>(
        source_param->input_connection());
    if (input) {
      input->UpdateStream();  // will automatically mark us as valid.
    } else {
      // Mark us as valid so we don't evaluate a second time.
      source_param->ValidateStream();
    }

    const Stream& source_stream = source_param->stream();
    if (source_stream.GetMaxVertices() != num_vertices) {
      // TODO: Change semantic below to semantic_name.
      O3D_ERROR(service_locator())
          << "stream " << source_stream.semantic() << " index "
          << source_stream.semantic_index() << " in SkinEval '" << name()
          << " does not have the same number of vertices as Skin '"
          << skin->name() << "'";
      return;
    }

    // Lock this input.
    if (!input_stream_infos_[ii].Init(source_stream, Buffer::READ_ONLY)) {
      String buffer_name;
      if (source_stream.field().buffer()) {
        buffer_name = source_stream.field().buffer()->name();
      }
      O3D_ERROR(service_locator())
          << "unable to lock buffer '" << buffer_name
          << " used by stream " << source_stream.semantic() << " index "
          << source_stream.semantic_index() << " in SkinEval '" << name()
          << "'";
      return;
    }

    // Lock the outputs to this input.
    const ParamVector& outputs = source_param->output_connections();
    if (output_stream_infos_[ii].size() != outputs.size()) {
      output_stream_infos_[ii].resize(outputs.size());
    }
    for (unsigned jj = 0; jj < outputs.size(); ++jj) {
      ParamVertexBufferStream* destination_param =
          down_cast<ParamVertexBufferStream*>(outputs[jj]);
      destination_param->ValidateStream();
      const Stream& destination_stream = destination_param->stream();
      if (destination_stream.GetMaxVertices() != num_vertices) {
        O3D_ERROR(service_locator())
            << "stream " << destination_stream.semantic() << " index "
            << destination_stream.semantic_index() << " targeted by SkinEval '"
            << name() << " does not have the same number of vertices as Skin '"
            << skin->name() << "'";
        return;
      }

      if (!output_stream_infos_[ii][jj].Init(destination_stream,
                                             Buffer::WRITE_ONLY)) {
        String buffer_name;
        if (destination_stream.field().buffer()) {
          buffer_name = destination_stream.field().buffer()->name();
        }
        O3D_ERROR(service_locator())
            << "unable to lock buffer '" << buffer_name
            << " used by stream " << destination_stream.semantic() << " index "
            << destination_stream.semantic_index() << " targeted by SkinEval '"
            << name() << "'";
        return;
      }
    }
  }

  // At this point, all our streams have been locked and everything has been
  // verified so we can skin without checking for errors.

  // TODO: Optimizations:
  //    It would be relatively easy to optimize for a few special cases.
  //
  //    * If there are no more than say 4 influences per bone, pretty common for
  //      games, we could cache the Skin data in format better suited
  //      to an SSE hardcoded loop.
  //
  //    * If all the streams are FLOAT4 we could do an SSE pass
  //
  //    * If all the streams are from the same buffer (interleved) we
  //      could special case 1 pointer instead of 1 per stream.
  //
  //    * If there is only one output stream per input stream use a
  //      code path that assumes that.

  // skin.
  for (unsigned ii = 0; ii < num_vertices; ++ii) {
    const Skin::Influences& influences = influences_array[ii];
    if (!influences.empty()) {
      // Get the first matrix.
      unsigned matrix_index = influences[0].matrix_index;

      // combine the matrixes for this vertex.
      Matrix4 accumulated_matrix(bones_[matrix_index] * influences[0].weight);
      unsigned num_influences = influences.size();
      for (unsigned jj = 1; jj < num_influences; ++jj) {
        const Skin::Influence& influence = influences[jj];
        accumulated_matrix += bones_[influence.matrix_index] *
                              influence.weight;
      }

      // for each source, compute and copy to destination.
      for (unsigned jj = 0; jj < num_streams; ++jj) {
        StreamInfo& input_stream_info = input_stream_infos_[jj];
        input_stream_info.Compute(accumulated_matrix);
        StreamInfoVector& output_streams = output_stream_infos_[jj];
        unsigned num_output_streams = output_streams.size();
        for (unsigned ll = 0; ll < num_output_streams; ++ll) {
          output_streams[ll].Copy(input_stream_info);
        }
      }
    }
  }
}

void SkinEval::UpdateOutputs() {
  // Get our matrices.
  ParamArray* param_array = matrices();
  if (!param_array) {
    O3D_ERROR(service_locator()) << "no matrices for SkinEval '"
                                 << name() << "'";
    return;
  }

  Skin* the_skin = skin();
  if (!the_skin) {
    O3D_ERROR(service_locator()) << "no skin specified in SkinEval '"
                                 << name() << "'";
    return;
  }

  // Make sure the bone indcies are in range.
  if (the_skin->GetHighestMatrixIndex() >= param_array->size()) {
    O3D_ERROR(service_locator())
        << "skin '" << the_skin->name() << " specified in SkinEval '"
        << name()
        << "' references matrices outside the valid range in ParamArray '"
        << param_array->name() << "'";
    return;
  }

  // Make sure the bind pose array size matches the matrices
  const Skin::MatrixArray& inverse_bind_pose_array =
      the_skin->inverse_bind_pose_matrices();
  if (inverse_bind_pose_array.size() != param_array->size()) {
    O3D_ERROR(service_locator())
        << "skin '" << the_skin->name() << " specified in SkinEval '"
        << name() << "' and the ParamArray '"
        << param_array->name() << "' do not have the same number of matrices.";
    return;
  }

  // Get all the bones
  if (bones_.size() < param_array->size()) {
    bones_.resize(param_array->size());
  }

  // Get the inverse of our base to remove from the bones.
  Matrix4 inverse_base(inverse(base()));

  for (unsigned ii = 0; ii < param_array->size(); ++ii) {
    ParamMatrix4* param = param_array->GetParam<ParamMatrix4>(ii);
    if (!param) {
      O3D_ERROR(service_locator())
          << "In SkinEval '" << name() << "' param at index " << ii
          << " in ParamArray '" << param_array->name()
          << " is not a ParamMatrix4";
      return;
    }
    bones_[ii] = param->value() * inverse_base * inverse_bind_pose_array[ii];
  }

  DoSkinning(the_skin);

  // Unlock any buffers that were locked during skinning
  for (unsigned ii = 0; ii < input_stream_infos_.size(); ++ii) {
    input_stream_infos_[ii].Uninit();
  }
  for (unsigned ii = 0; ii < output_stream_infos_.size(); ++ii) {
    StreamInfoVector& output_streams = output_stream_infos_[ii];
    for (unsigned jj = 0; jj < output_streams.size(); ++jj) {
      output_streams[jj].Uninit();
    }
  }
}

bool SkinEval::ParamIsStreamParam(const Param* param) const {
  for (unsigned ii = 0; ii < vertex_stream_params_.size(); ++ii) {
    if (vertex_stream_params_[ii].Get() == param) {
      return true;
    }
  }
  return false;
}

void SkinEval::ConcreteGetInputsForParam(const Param* param,
                                         ParamVector* inputs) const {
  // If it's a stream param then it's effected by all the other params.
  if (ParamIsStreamParam(param)) {
    inputs->push_back(base_param_);
    inputs->push_back(matrices_param_);
    inputs->push_back(skin_param_);
    ParamArray* param_array = matrices();
    if (param_array) {
      unsigned size = param_array->size();
      for (unsigned ii = 0; ii < size; ++ii) {
        Param* matrix_param = param_array->GetUntypedParam(ii);
        if (matrix_param) {
          inputs->push_back(matrix_param);
        }
      }
    }
  }
}

void SkinEval::ConcreteGetOutputsForParam(const Param* param,
                                          ParamVector* outputs) const {
  ParamArray* param_array = matrices();
  // If it's anything but a stream param it's outputs are all the stream params.
  if (param == base_param_ ||
      param == matrices_param_ ||
      param == skin_param_ ||
      (param_array && param_array->ParamInArray(param))) {
    for (unsigned ii = 0; ii < vertex_stream_params_.size(); ++ii) {
      outputs->push_back(vertex_stream_params_[ii]);
    }
  }
}

ObjectBase::Ref SkinEval::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new SkinEval(service_locator));
}

ObjectBase::Ref ParamSkin::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new ParamSkin(service_locator, false, false));
}

bool Skin::LoadFromBinaryData(MemoryReadStream *stream) {
  // Make sure we have enough data for serialization ID and version
  if (stream->GetRemainingByteCount() < 4 + sizeof(int32)) {
    O3D_ERROR(service_locator()) << "invalid empty skin data";
    return false;
  }

  // To insure data integrity we expect four characters kSerializationID
  uint8 id[4];
  stream->Read(id, 4);

  if (memcmp(id, kSerializationID, 4)) {
    O3D_ERROR(service_locator())
        << "data object does not contain skin data";
    return false;
  }

  int32 version = stream->ReadLittleEndianInt32();
  if (version != 1) {
    O3D_ERROR(service_locator()) << "unknown skin data version";
    return false;
  }

  int vertex_index = 0;  // start at vertex zero.

  while (!stream->EndOfStream()) {
    // Make sure stream has a uint32 to read (for num_influences)
    if (stream->GetRemainingByteCount() < sizeof(uint32)) {
      O3D_ERROR(service_locator()) << "unexpected end of skin data";
      return false;
    }

    uint32 num_influences = stream->ReadLittleEndianInt32();

    // Make sure the stream actually has num_influences data to read
    const size_t kInfluenceSize = sizeof(uint32) + sizeof(float);
    size_t data_size = num_influences * kInfluenceSize;
    if (stream->GetRemainingByteCount() < data_size) {
      O3D_ERROR(service_locator()) << "unexpected end of skin data";
      return false;
    }

    if (num_influences > 0) {
      Skin::Influences influences(num_influences);
      for (Skin::Influences::size_type i = 0; i < num_influences; ++i) {
        uint32 matrix_index = stream->ReadLittleEndianInt32();
        float weight = stream->ReadLittleEndianFloat32();
        influences[i] = Skin::Influence(matrix_index, weight);
      }
      SetVertexInfluences(vertex_index, influences);
    }

    ++vertex_index;
  }

  return true;
}

bool Skin::Set(o3d::RawData *raw_data) {
  if (!raw_data) {
    O3D_ERROR(service_locator()) << "data object is null";
    return false;
  }
  return Set(raw_data, 0, raw_data->GetLength());
}

bool Skin::Set(o3d::RawData *raw_data,
               size_t offset,
               size_t length) {
  if (!raw_data) {
    O3D_ERROR(service_locator()) << "data object is null";
    return false;
  }

  if (!raw_data->IsOffsetLengthValid(offset, length)) {
    O3D_ERROR(service_locator()) << "illegal skin data offset or size";
    return false;
  }

  const uint8 *data = raw_data->GetDataAs<uint8>(offset);
  if (!data) {
    return false;
  }

  MemoryReadStream stream(data, length);
  return LoadFromBinaryData(&stream);
}

}  // namespace o3d
