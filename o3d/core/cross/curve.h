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


// This file contains the Curve declaration.

#ifndef O3D_CORE_CROSS_CURVE_H_
#define O3D_CORE_CROSS_CURVE_H_

#include <vector>
#include "core/cross/param_object.h"
#include "core/cross/function.h"

namespace o3d {

class Curve;
class MemoryReadStream;
class RawData;

// CurveKey is the abstract base class for all types of CurveKeys.
// Each Concrete CurveKey is responsible for computing outputs between it
// and the next type of key.
class CurveKey : public ObjectBase {
 public:
  typedef SmartPointer<CurveKey> Ref;

  // These enum values are only used for binary serialization/deserialization
  // and are not exposed through Javascript
  enum KeyType {
    TYPE_UNKNOWN = 0,
    TYPE_STEP = 1,
    TYPE_LINEAR = 2,
    TYPE_BEZIER = 3,
  };

  virtual ~CurveKey() { }

  // Destroy's this Key (removing it from its owner).
  void Destroy();

  // Gets the input of this key.
  inline float input() const {
    return input_;
  }

  // Sets the input of this key. This has the side effect of telling the
  // Curve this key belongs to resort the keys.
  // Parameters:
  //   new_input: new input for key.
  void SetInput(float new_input);

  // Gets the output of the key.
  inline float output() const {
    return output_;
  }

  // Sets the output of the key.
  void SetOutput(float new_output);

  // Gets the Curve that owns this key.
  Curve* owner() const {
    return owner_;
  }

  // Given an offset from this key's input to the next key (key_index + 1)
  // returns a output between this key and the next key.
  // Parameters:
  //   offset: Offset to next key.
  //   key_index: Index of this key. There must be a key at key_index + 1!!!
  virtual float GetOutputAtOffset(float offset, unsigned key_index) const = 0;

 protected:
  CurveKey(ServiceLocator* service_locator, Curve* owner);

 private:
  // Curve we are owned by.
  Curve* owner_;

  // Input of key
  float input_;

  // Output of key at input.
  float output_;

  O3D_DECL_CLASS(CurveKey, ObjectBase);
  DISALLOW_COPY_AND_ASSIGN(CurveKey);
};

typedef std::vector<CurveKey::Ref> CurveKeyRefArray;
typedef std::vector<CurveKey*> CurveKeyArray;

// An CurveKey that holds its output (is not interpolated between this key
// and the next.)
class StepCurveKey : public CurveKey {
 public:
  typedef SmartPointer<StepCurveKey> Ref;

  StepCurveKey(ServiceLocator* service_locator, Curve* owner);

  // Overridden from CurveKey
  virtual float GetOutputAtOffset(float offset, unsigned key_index) const;

  static CurveKey::Ref Create(ServiceLocator* service_locator, Curve* owner);

 private:
  O3D_DECL_CLASS(StepCurveKey, CurveKey);
  DISALLOW_COPY_AND_ASSIGN(StepCurveKey);
};

// An CurveKey that linearly interpolates between this key and the next key.
class LinearCurveKey : public CurveKey {
 public:
  typedef SmartPointer<LinearCurveKey> Ref;

  LinearCurveKey(ServiceLocator* service_locator, Curve* owner);

  // Overridden from CurveKey
  virtual float GetOutputAtOffset(float offset, unsigned key_index) const;

  static CurveKey::Ref Create(ServiceLocator* service_locator, Curve* owner);

 private:
  O3D_DECL_CLASS(LinearCurveKey, CurveKey);
  DISALLOW_COPY_AND_ASSIGN(LinearCurveKey);
};

// An CurveKey that uses a bezier curve for interpolation between this key
// and the next.
class BezierCurveKey : public CurveKey {
 public:
  typedef SmartPointer<BezierCurveKey> Ref;

  BezierCurveKey(ServiceLocator* service_locator, Curve* owner);

  // Overridden from CurveKey
  virtual float GetOutputAtOffset(float offset, unsigned key_index) const;

  inline const Float2& in_tangent() const {
    return in_tangent_;
  }

  // Sets the in tanget of the key.
  void SetInTangent(const Float2& value);

  inline const Float2& out_tangent() const {
    return out_tangent_;
  }

  // Sets the out tanget of the key.
  void SetOutTangent(const Float2& value);

  static CurveKey::Ref Create(ServiceLocator* service_locator, Curve* owner);

 private:
  Float2 in_tangent_;
  Float2 out_tangent_;

  O3D_DECL_CLASS(BezierCurveKey, CurveKey);
  DISALLOW_COPY_AND_ASSIGN(BezierCurveKey);
};

// A CurveFunctionContext is used by Curve to help with evaluation.
// In the case of Curve it holds the index to the last key so that
// the next time the curve is evaluated we can usually avoid looking for the
// correct key pair.
class CurveFunctionContext : public FunctionContext {
 public:
  typedef SmartPointer<CurveFunctionContext> Ref;

  explicit CurveFunctionContext(ServiceLocator* service_locator);

  unsigned last_key_index() const {
    return last_key_index_;
  }

  void set_last_key_index(unsigned index) {
    last_key_index_ = index;
  }

 private:
  unsigned last_key_index_;

  O3D_DECL_CLASS(CurveFunctionContext, FunctionContext);
  DISALLOW_COPY_AND_ASSIGN(CurveFunctionContext);
};

// An Curve stores a bunch of spline keys and given a value
// representing a point on the spline returns the value of the spline for that
// point. Curve is a data only. It is use by 1 or more
// AnimationChannels.
class Curve : public Function {
 public:
  typedef SmartPointer<Curve> Ref;

  // Animations that are cached default to using this sample rate.
  static const float kDefaultSampleRate;
  static const float kMinimumSampleRate;

  // A four-character identifier used in the binary serialization format
  // (not exposed to Javascript)
  static const char *kSerializationID;


  enum Infinity {
    CONSTANT,        // Uses the output value of the first or last animation
                     // key.
                     //
    LINEAR,          // Takes the distance between the closest animation key
                     // input value and the evaluation time. Multiplies this
                     // distance against the instant slope at the closest
                     // animation key and offsets the result with the closest
                     // animation key output value.
                     //
    CYCLE,           // Cycles over the first and last keys using
                     // input = (input - first) % (last - first) + first;
                     // Note that in CYCLE mode you can never get the end value
                     // because a cycle goes from start to end exclusive of end.
                     //
    CYCLE_RELATIVE,  // Same as cycle except
                     // value = (last.value - first.value) *
                     //     (input - first) / (last - first)
                     //
    OSCILLATE,       // Ping Pongs between the first and last keys.
                     // input = (input - first) % ((last - first) * 2)
                     // if (input > (last - first)) {
                     //   input = (last - first) * 2 - input;
                     // }
                     // input += first;
  };

  ~Curve();

  Infinity pre_infinity() const {
    return pre_infinity_;
  }

  void set_pre_infinity(Infinity infinity) {
    pre_infinity_ = infinity;
  }

  Infinity post_infinity() const {
    return post_infinity_;
  }

  void set_post_infinity(Infinity infinity) {
    post_infinity_ = infinity;
  }

  bool use_cache() const {
    return use_cache_;
  }

  // Sets whether or not to use the cache.
  void set_use_cache(bool use_cache) {
    use_cache_ = use_cache;
  }

  // The sample rate for the cache.
  float sample_rate() const {
    return sample_rate_;
  }

  // Sets the sample rate used for the cache. By default Animation data is
  // cached so that using the animation is fast. To do this the keys that
  // represent the animation are sampled. The higher the frequency of the
  // samples the closer the cache will match the actual keys.
  // The default is 1/30 (30hz). You can set it anywhere from 1/240th (240hz) to
  // any larger value. Note: Setting the sample rate has the side effect of
  // invalidating the cache thereby causing it to get rebuilt.
  // Parameters:
  //   rate: sample rate. Must be 1/240 or greater. Default = 1/30.
  void SetSampleRate(float rate);


  // Returns whether or not the curve is discontinuous. A discontinuous curve
  // takes more time to evaluate.
  bool IsDiscontinuous() const;

  // Overriden from Function.
  // Gets a value for this curve at the given input on the curve.
  // Parameters:
  //   input: input to function.
  //   context: A CurveFunctionContext. May be null.
  // Returns:
  //   The output for the given input.
  float Evaluate(float input, FunctionContext* context) const;

  // Overriden from Function.
  virtual FunctionContext* CreateFunctionContext() const;

  // Overriden from Function.
  virtual const ObjectBase::Class* GetFunctionContextClass() const;

  // Creates a new Key. Do not use this directly, Use the Create template below.
  // Parameters:
  //   key_type: class of key to create.
  // Returns:
  //   pointer to created key.
  CurveKey* CreateKeyByClass(const ObjectBase::Class* key_type);

  // Creates a new Key for this curve. Use like this.
  //
  // StepCurveKey* key = curve->Create<StepCurveKey>();
  template<typename T>
  T* Create() {
    T* key = down_cast<T*>(CreateKeyByClass(T::GetApparentClass()));
    DCHECK(key);
    return key;
  }

  // Creates a new key for this curve. This function is for Javascript.
  // Parameters:
  //   key_type: class name of key to create.
  // Returns:
  //   pointer to created key.
  CurveKey* CreateKeyByClassName(const String& key_type);

  // This is an internal function. It is only called by CurveKey::Remove
  void RemoveKey(CurveKey* key);

  // Returns the array of keys for this curve.
  const CurveKeyRefArray& keys() {
    return keys_;
  }

  // Gets a particular key by index, returns NULL if the index is out of range.
  CurveKey* GetKey(unsigned key_index) const {
    return key_index < keys_.size() ? keys_[key_index].Get() : NULL;
  }

  // Invalidates the cache (internal use only)
  void InvalidateCache() const;

  // Marks the cache as unsorted (internal use only)
  void MarkAsUnsorted() const;

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

  // de-serializes from a memory stream
  bool LoadFromBinaryData(MemoryReadStream *stream);

 private:
  explicit Curve(ServiceLocator* service_locator);

  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Adds a key to the keys that belong to this curve.
  void AddKey(CurveKey::Ref key);

  // Checks if the keys represent a discontinuous curve.
  void CheckDiscontinuity() const;

  // Resorts the keys by input. Requires that keys that have the same
  // input remain in the same relative order. In otherwords, if key1 comes
  // before key2 and both have the same input then resorting keeps key1
  // before key2.
  void ResortKeys() const;

  // Returns a output for the given input. Input must be greater than or
  // equal to the input of the first key and any input >= the input of
  // the last key will return the output of the last key.
  float GetOutputInSpan(float input, CurveFunctionContext* context) const;

  // Caches the animation in an evaluation friendly format.
  void CreateCache() const;

  // Updates information about the curve such as sorting the keys
  // and checking for discontinuity.
  inline void UpdateCurveInfo() const {
    if (!sorted_) {
      ResortKeys();
    }
    if (check_discontinuity_) {
      CheckDiscontinuity();
    }
  }

  // What to do for inputs before the first key
  Infinity pre_infinity_;

  // What to do for inputs past the last key.
  Infinity post_infinity_;

  // true if the keys are sorted.
  mutable bool sorted_;

  // Animation keys. These keys are kept sorted by input at all times.
  mutable CurveKeyRefArray keys_;

  // -- Cache related fields --

  // true if we should use the cache vs recomputing from the curve each time.
  // Default is true. The only reason to set this to false is either that
  // it uses less memory not to cache or because the sampling is not
  // accurate enough.
  bool use_cache_;

  // how often to sample for the cache. Default = 1/30
  float sample_rate_;

  // true if the cache is valid.
  mutable bool cache_valid_;

  // true if we need to check whether or not the curve is discontinous
  mutable bool check_discontinuity_;

  // true if there are 2 keys at the same input with different outputs.
  mutable bool discontinuous_;

  // The number of keys at are Step keys. If all keys are all step keys we can
  // use the cache but must not interpolate. If there are one or more step keys
  // but all the keys are not step keys then the curve will be marked as
  // discontinous.
  mutable unsigned num_step_keys_;

  // The last key we used during evaluate.
  mutable unsigned last_key_index_;

  // Samples used for cache.
  mutable std::vector<float> cache_samples_;

  O3D_DECL_CLASS(Curve, Function);
  DISALLOW_COPY_AND_ASSIGN(Curve);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_CURVE_H_
