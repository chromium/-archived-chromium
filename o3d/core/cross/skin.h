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


// This file contains the declaration of class Skin.

#ifndef O3D_CORE_CROSS_SKIN_H_
#define O3D_CORE_CROSS_SKIN_H_

#include <vector>
#include "core/cross/buffer.h"
#include "core/cross/param_object.h"
#include "core/cross/vertex_source.h"
#include "core/cross/param_array.h"

namespace o3d {

class MemoryReadStream;
class RawData;

// A Skin holds an array of matrix indices and influences for vertices in a skin
// as well as the inverse bind pose matrices for each bone.
class Skin : public NamedObject {
 public:
  typedef SmartPointer<Skin> Ref;
  typedef WeakPointer<Skin> WeakPointerType;

  // A four-character identifier used in the binary serialization format
  // (not exposed to Javascript)
  static const char *kSerializationID;

  // One matrix-weight pair.
  struct Influence {
    Influence()
        : matrix_index(0),
          weight(0) {
    }
    Influence(unsigned index, float weight)
        : matrix_index(index),
          weight(weight) {
    }
    unsigned matrix_index;
    float weight;
  };

  typedef std::vector<Influence> Influences;
  typedef std::vector<Influences> InfluencesArray;

  typedef std::vector<Matrix4> MatrixArray;

  const InfluencesArray& influences() const {
    return influences_array_;
  }

  // Sets the data for an individual vertex.
  // Parameters:
  //   vertex_index: Index of vertex to set.
  //   influences: The matrix-weight pairs.
  void SetVertexInfluences(unsigned vertex_index,
                           const Influences& influences);

  // Gets the data for an individual vertex.
  // Parameters:
  //   vertex_index: Index of vertex to set.
  // Returns:
  //   The influence data for the requested vertex or NULL if the index is out
  //   of range.
  const Influences* GetVertexInfluences(unsigned vertex_index) const;

  // Gets the highest matrix index referenced by the influences.
  // Returns:
  //   The highest matrix index referenced by the influences.
  unsigned GetHighestMatrixIndex() const;

  // Gets the highest number of influences on any vertex.
  // Returns:
  //   The highest number of influences on any vertex.
  unsigned GetHighestInfluences() const;

  // Sets the inverse bind pose matrix for a particular joint/bone/transform.
  // Parameters:
  //   index: index of bone/joint/transform
  //   matrix: Inverse bind pose matrix for that joint.
  void SetInverseBindPoseMatrix(unsigned index, const Matrix4& matrix);

  // Gets the vector of inverse bind pose matrices.
  // Returns:
  //   the vector of inverse bind pose matrices.
  const MatrixArray& inverse_bind_pose_matrices() const {
    return inverse_bind_pose_matrices_;
  }

  // Sete the vector of inverse bind pose matrices.
  void set_inverse_bind_pose_matrices(const MatrixArray& matrices) {
    inverse_bind_pose_matrices_ = matrices;
  }

  // Gets a weak pointer to us.
  WeakPointerType GetWeakPointer() const {
    return weak_pointer_manager_.GetWeakPointer();
  }

  // De-serializes the data contained in |raw_data|
  // The entire contents of |raw_data| from start to finish will
  // be used
  bool Set(o3d::RawData *raw_data);

  // De-serializes the data contained in |raw_data|
  // starting at byte offset |offset| and using |length|
  // bytes
  bool Set(o3d::RawData *raw_data,
           size_t offset,
           size_t length);

  // De-serializes a skin from its persistent binary representation
  bool LoadFromBinaryData(MemoryReadStream *stream);

 private:
  explicit Skin(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Update the highest influences and highest matrix index.
  void UpdateInfo() const;

  // The vertex influences.
  InfluencesArray influences_array_;

  // The inverse bind poses.
  MatrixArray inverse_bind_pose_matrices_;

  // The highest matrix index.
  mutable unsigned highest_matrix_index_;

  // The highest number of influences.
  mutable unsigned highest_influences_;

  // True of the highest matrix index and highest influences is valid.
  mutable bool info_valid_;

  // Manager for weak pointers to us.
  WeakPointerType::WeakPointerManager weak_pointer_manager_;

  O3D_DECL_CLASS(Skin, NamedObject);
  DISALLOW_COPY_AND_ASSIGN(Skin);
};

// A Param that holds a weak reference to a Skin.
class ParamSkin : public TypedRefParam<Skin> {
 public:
  typedef SmartPointer<ParamSkin> Ref;

  ParamSkin(ServiceLocator* service_locator, bool dynamic, bool read_only)
      : TypedRefParam<Skin>(service_locator, dynamic, read_only) {}

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  O3D_DECL_CLASS(ParamSkin, RefParamBase);
};

// A SkinEval is a VertexSource that takes its Streams, a ParamArray of Matrix4s
// and a Skin and skins the vertices in its streams storing the results in bound
// output streams.
class SkinEval : public VertexSource {
 public:
  static const char* kMatricesParamName;
  static const char* kSkinParamName;
  static const char* kBaseParamName;

  Skin* skin() const {
    return skin_param_->value();
  }

  void set_skin(Skin* skin) {
    skin_param_->set_value(skin);
  }

  ParamArray* matrices() const {
    return matrices_param_->value();
  }

  void set_matrices(ParamArray* matrices) {
    matrices_param_->set_value(matrices);
  }

  Matrix4 base() const {
    return base_param_->value();
  }

  void set_base(const Matrix4& base) {
    base_param_->set_value(base);
  }

  // Binds the field of SourceBuffer and defines how the data in the buffer
  // should be accessed and interpreted.
  bool SetVertexStream(Stream::Semantic semantic,
                       int semantic_index,
                       Field* field,
                       unsigned int start_index);

  // Removes a vertex stream from this primitive.
  // Returns true if the specified stream existed.
  bool RemoveVertexStream(Stream::Semantic stream_semantic,
                          int semantic_index);

  // Searches the vertex streams bound to the shape for one with the given
  // stream semantic.  If a stream is not found then it returns NULL.
  const Stream* GetVertexStream(Stream::Semantic stream_semantic,
                                int semantic_index) const;

  // Updates all the VertexBuffers bound to streams on this VertexSource.
  void UpdateStreams();

  // Updates the VertexBuffers bound to streams on this VertexSource.
  void UpdateOutputs();

  // Overriden from VertexSource.
  virtual ParamVertexBufferStream* GetVertexStreamParam(
      Stream::Semantic semantic,
      int semantic_index) const;

  const StreamParamVector& vertex_stream_params() const {
    return vertex_stream_params_;
  }

 protected:
  // Overridden from ParamObject
  // For the given Param, returns all the inputs that affect that param through
  // this ParamObject.
  virtual void ConcreteGetInputsForParam(const Param* param,
                                         ParamVector* inputs) const;

  // Overridden from ParamCollection
  // For the given Param, returns all the outputs that the given param will
  // affect through this ParamObject.
  virtual void ConcreteGetOutputsForParam(const Param* param,
                                          ParamVector* outputs) const;

 private:
  class SlaveParamVertexBufferStream : public ParamVertexBufferStream {
   public:
    SlaveParamVertexBufferStream(ServiceLocator* service_locator,
                                 SkinEval* master,
                                 Stream* stream)
        : ParamVertexBufferStream(service_locator, stream, true, false),
          master_(master) {
      set_owner(master);
    }

    virtual void ComputeValue() {
      master_->UpdateOutputs();
    }

   private:
    SkinEval* master_;
    DISALLOW_COPY_AND_ASSIGN(SlaveParamVertexBufferStream);
  };

  typedef std::vector<Matrix4> Matrix4Vector;

  explicit SkinEval(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Returns true if param is one of the Stream params on this SkinEval.
  bool ParamIsStreamParam(const Param* param) const;

  void DoSkinning(Skin* skin);

  // The streams on this SkinEval
  StreamParamVector vertex_stream_params_;

  // The array of bone matrices
  ParamParamArray::Ref matrices_param_;

  // The base matrix that will be used to keep the vertices in object space.
  ParamMatrix4::Ref base_param_;

  // The skin
  ParamSkin::Ref skin_param_;

  // A place to store the bone calulations. We keep this in the struct
  // so we don't have to allocate it every time.
  Matrix4Vector bones_;

  // This class helps manage each stream. Because allocating memory is slow
  // we keep these around across calls and reuse them in place by calling
  // Init.
  class StreamInfo {
   public:
    StreamInfo();

    // Locks the stream.
    bool Init(const Stream& stream, Buffer::AccessMode access_mode);

    // Unlocks the stream.
    void Uninit();

    // Multiplies the current value by the matrix and stores it in result and
    // advances to the next value.
    void Compute(const Matrix4& matrix) {
      compute_function_(&result_[0], values_, matrix);
      Advance();
    }

    // Copies the result from source and advances to the next value.
    void Copy(const StreamInfo& source) {
      copy_function_(values_, &source.result_[0]);
      Advance();
    }

   private:
    // Advances the internal counter to the next set.
    void Advance() {
      values_ = reinterpret_cast<float*>(reinterpret_cast<char*>(
          values_) + stride_);
    }

    // A pointer to function the multipies the source by the matrix and stores
    // the result in destination.
    typedef void (*ComputeFunction)(float* destination,
                                    const float* source,
                                    const Matrix4& matrix);

    // A pointer to a function that copies a 3 or 4 floats from the source to
    // the destination.
    typedef void (*CopyFunction)(float* destination, const float* source);

    ComputeFunction compute_function_;
    CopyFunction copy_function_;
    void* data_;
    Buffer* buffer_;
    float* values_;
    unsigned stride_;
    float result_[4];
  };

  typedef std::vector<StreamInfo> StreamInfoVector;
  typedef std::vector<StreamInfoVector> StreamInfoVectorVector;

  StreamInfoVector input_stream_infos_;
  StreamInfoVectorVector output_stream_infos_;

  O3D_DECL_CLASS(SkinEval, VertexSource);
  DISALLOW_COPY_AND_ASSIGN(SkinEval);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_SKIN_H_
