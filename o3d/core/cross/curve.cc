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


// This file contains the implementation of Curve.
// This code is heavily influenced by code from FCollada.

#include "core/cross/precompile.h"
#include "core/cross/curve.h"
#include "core/cross/error.h"
#include "import/cross/memory_stream.h"
#include "import/cross/raw_data.h"

#undef min
#undef max

namespace o3d {

O3D_DEFN_CLASS(CurveKey, ObjectBase);
O3D_DEFN_CLASS(StepCurveKey, CurveKey);
O3D_DEFN_CLASS(LinearCurveKey, CurveKey);
O3D_DEFN_CLASS(BezierCurveKey, CurveKey);
O3D_DEFN_CLASS(CurveFunctionContext, FunctionContext);
O3D_DEFN_CLASS(Curve, Function);

const char *Curve::kSerializationID = "CURV";

namespace {

const float kEpsilon = 0.00001f;

float Clamp(float value, float min_value, float max_value) {
  return value > max_value ? max_value :
      (value < min_value ? min_value : value);
}

// Uses iterative method to accurately pin-point the 't' of the Bezier
// equation that corresponds to the current time.
float FindT(float control_point_0_x,
            float control_point_1_x,
            float control_point_2_x,
            float control_point_3_x,
            float input,
            float initial_guess) {
  float local_tolerance = 0.001f;
  float high_t = 1.0f;
  float low_t = 0.0f;

  // TODO: Optimize here, start with a more intuitive value than 0.5
  //     (comment left over from original code)
  float mid_t = 0.5f;
  if (initial_guess <= 0.1) {
    mid_t = 0.1f;  // clamp to 10% or 90%, because if miss, the cost is
                   // too high.
  } else if (initial_guess >= 0.9) {
    mid_t = 0.9f;
  } else {
    mid_t = initial_guess;
  }
  bool once = true;
  while ((high_t-low_t) > local_tolerance) {
    if (once) {
      once = false;
    } else {
      mid_t = (high_t - low_t) / 2.0f + low_t;
    }
    float ti = 1.0f - mid_t;  // (1 - t)
    float calculated_time = control_point_0_x * ti * ti * ti +
                            3 * control_point_1_x * mid_t * ti * ti +
                            3 * control_point_2_x * mid_t * mid_t * ti +
                            control_point_3_x * mid_t * mid_t * mid_t;
    if (fabsf(calculated_time - input) <= local_tolerance) {
      break;  // If we 'fall' very close, we like it and break.
    }
    if (calculated_time > input) {
      high_t = mid_t;
    } else {
      low_t = mid_t;
    }
  }
  return mid_t;
}

typedef CurveKey::Ref (*KeyCreatorFunc)(ServiceLocator* service_locator,
                                        Curve* onwer);
struct KeyCreator {
  const ObjectBase::Class* key_type;
  KeyCreatorFunc create_function;
};
static KeyCreator g_creators[] = {
  { StepCurveKey::GetApparentClass(), StepCurveKey::Create, },
  { LinearCurveKey::GetApparentClass(), LinearCurveKey::Create, },
  { BezierCurveKey::GetApparentClass(), BezierCurveKey::Create, },
};

}  // anonymous namespace

CurveKey::CurveKey(ServiceLocator* service_locator, Curve* owner)
    : ObjectBase(service_locator),
      owner_(owner),
      input_(0.0f),
      output_(0.0f) {
}

void CurveKey::Destroy() {
  owner_->RemoveKey(this);
}

void CurveKey::SetInput(float new_input) {
  if (new_input != input_) {
    input_ = new_input;
    owner_->MarkAsUnsorted();
  }
}

void CurveKey::SetOutput(float new_output) {
  if (new_output != output_) {
    output_ = new_output;
    owner_->InvalidateCache();
  }
}

// StepCurveKey ------------------------------------------------------------

StepCurveKey::StepCurveKey(ServiceLocator* service_locator, Curve* owner)
    : CurveKey(service_locator, owner) {
}

CurveKey::Ref StepCurveKey::Create(ServiceLocator* service_locator,
                                   Curve* owner) {
  return CurveKey::Ref(new StepCurveKey(service_locator, owner));
}

float StepCurveKey::GetOutputAtOffset(float offset, unsigned key_index) const {
  return output();
}

// LinearCurveKey ----------------------------------------------------------

LinearCurveKey::LinearCurveKey(ServiceLocator* service_locator, Curve* owner)
    : CurveKey(service_locator, owner) {
}

CurveKey::Ref LinearCurveKey::Create(ServiceLocator* service_locator,
                                     Curve* owner) {
  return CurveKey::Ref(new LinearCurveKey(service_locator, owner));
}

float LinearCurveKey::GetOutputAtOffset(float offset,
                                        unsigned key_index) const {
  const CurveKey* next_key(owner()->GetKey(key_index + 1));
  DCHECK(next_key);

  float input_span = next_key->input() - input();
  float output_span = next_key->output() - output();
  return output() + offset / input_span * output_span;
}

// BezierCurveKey ----------------------------------------------------------

BezierCurveKey::BezierCurveKey(ServiceLocator* service_locator, Curve* owner)
    : CurveKey(service_locator, owner),
      in_tangent_(0, 0),
      out_tangent_(0, 0) {
}

CurveKey::Ref BezierCurveKey::Create(ServiceLocator* service_locator,
                                     Curve* owner) {
  return CurveKey::Ref(new BezierCurveKey(service_locator, owner));
}

void BezierCurveKey::SetInTangent(const Float2& value) {
  in_tangent_ = value;
  owner()->InvalidateCache();
}

void BezierCurveKey::SetOutTangent(const Float2& value) {
  out_tangent_ = value;
  owner()->InvalidateCache();
}

float BezierCurveKey::GetOutputAtOffset(float offset,
                                        unsigned key_index) const {
  const CurveKey* next_key(owner()->GetKey(key_index + 1));
  DCHECK(next_key);

  float input_span = next_key->input() - input();
  float output_span = next_key->output() - output();
  Float2 in_tangent;

  // We check bezier first because it's the most likely match for another
  // bezier key.
  if (next_key->GetClass() == BezierCurveKey::GetApparentClass()) {
    in_tangent = (down_cast<const BezierCurveKey*>(next_key)->in_tangent());
  } else if (next_key->GetClass() == LinearCurveKey::GetApparentClass() ||
             next_key->GetClass() == StepCurveKey::GetApparentClass()) {
    in_tangent.setX(next_key->input() - input_span / 3.0f);
    in_tangent.setY(next_key->output() - output_span / 3.0f);
  } else {
    DCHECK(false);  // Bad Key.
    return output();
  }

  // Do a bezier calculation.
  float t = offset / input_span;
  t = FindT(input(),
            out_tangent_.getX(),
            in_tangent.getX(),
            next_key->input(),
            input() + offset,
            t);
  float b = out_tangent_.getY();
  float c = in_tangent.getY();
  float ti = 1.0f - t;
  float br = 3.0f;
  float cr = 3.0f;
  return output() * ti * ti * ti + br * b * ti * ti * t +
      cr * c * ti * t * t + next_key->output() * t * t * t;
}

// CurveFunctionContext ------------------------------------------------

CurveFunctionContext::CurveFunctionContext(ServiceLocator* service_locator)
    : FunctionContext(service_locator),
      last_key_index_(0) {
}

// Curve --------------------------------------------------------------

const float Curve::kDefaultSampleRate = 1.0f / 30.0f;  // 30hz
const float Curve::kMinimumSampleRate = 1.0f / 240.0f;  // 240hz

Curve::Curve(ServiceLocator* service_locator)
    : Function(service_locator),
      pre_infinity_(CONSTANT),
      post_infinity_(CONSTANT),
      sorted_(true),
      use_cache_(true),
      sample_rate_(kDefaultSampleRate),
      cache_valid_(false),
      check_discontinuity_(false),
      discontinuous_(false),
      num_step_keys_(0),
      last_key_index_(0) {
}

Curve::~Curve() {
}

const ObjectBase::Class* Curve::GetFunctionContextClass() const {
  return CurveFunctionContext::GetApparentClass();
}

FunctionContext* Curve::CreateFunctionContext() const {
  return new CurveFunctionContext(service_locator());
}

void Curve::AddKey(CurveKey::Ref key) {
  keys_.push_back(key);
  MarkAsUnsorted();
  if (key->IsA(StepCurveKey::GetApparentClass())) {
    ++num_step_keys_;
  }
}

CurveKey* Curve::CreateKeyByClass(const ObjectBase::Class* key_type) {
  for (unsigned ii = 0; ii < arraysize(g_creators); ++ii) {
    if (g_creators[ii].key_type == key_type) {
      CurveKey::Ref key(g_creators[ii].create_function(
          service_locator(),
          this));
      AddKey(key);
      return key.Get();
    }
  }

  O3D_ERROR(service_locator())
      << "unrecognized key type '" << (key_type ? key_type->name() : "NULL")
      << "'";
  return NULL;
}

CurveKey* Curve::CreateKeyByClassName(const String& key_type) {
  for (unsigned ii = 0; ii < arraysize(g_creators); ++ii) {
    if (!key_type.compare(g_creators[ii].key_type->name()) ||
        !key_type.compare(g_creators[ii].key_type->unqualified_name())) {
      CurveKey::Ref key(g_creators[ii].create_function(
          service_locator(),
          this));
      AddKey(key);
      return key.Get();
    }
  }

  O3D_ERROR(service_locator())
      << "unrecognized key type '" << key_type << "'";
  return NULL;
}

void Curve::RemoveKey(CurveKey* key) {
  // keep a reference to the key so it doesn't get deleted early.
  CurveKey::Ref temp(key);
  CurveKeyRefArray::iterator end = std::remove(keys_.begin(),
                                               keys_.end(),
                                               CurveKey::Ref(key));

  // key should never be in the key array more than once.
  DLOG_ASSERT(std::distance(end, keys_.end()) <= 1);

  // The key was never found.
  DCHECK(end != keys_.end());

  if (key->IsA(StepCurveKey::GetApparentClass())) {
    --num_step_keys_;
  }

  // Actually remove the key from the key array
  keys_.erase(end, keys_.end());

  InvalidateCache();

  // The key will get destroyed here since the last reference to
  // it, temp, will be released.
}

void Curve::SetSampleRate(float rate) {
  if (rate < kMinimumSampleRate) {
    // TODO: should we just silently set it to the minimum?
    O3D_ERROR(service_locator())
        << "attempt to set sample rate to " << rate
        << " which is lower than the minimum of "
        << kMinimumSampleRate;
  } else if (rate != sample_rate_) {
    sample_rate_ = rate;
    InvalidateCache();
  }
}

void Curve::MarkAsUnsorted() const {
  sorted_ = false;
}

void Curve::InvalidateCache() const {
  cache_valid_ = false;
  check_discontinuity_ = true;
}

bool Curve::IsDiscontinuous() const {
  UpdateCurveInfo();
  return discontinuous_;
}

inline static bool CompareByInput(const CurveKey::Ref& lhs,
                                  const CurveKey::Ref& rhs) {
  return lhs->input() < rhs->input();
}

void Curve::ResortKeys() const {
  std::sort(keys_.begin(), keys_.end(), CompareByInput);
  sorted_ = true;
  InvalidateCache();
}

void Curve::CheckDiscontinuity() const {
  // Mark the curve as discontinuous if any 2 keys share the same input and
  // if their outputs are different.
  check_discontinuity_ = false;
  discontinuous_ = num_step_keys_ > 0 && num_step_keys_ != keys_.size();
  if (!discontinuous_ && keys_.size() > 1) {
    for (unsigned ii = 0; ii < keys_.size() - 1; ++ii) {
      if (keys_[ii]->input() == keys_[ii + 1]->input() &&
          keys_[ii]->output() != keys_[ii + 1]->output()) {
        discontinuous_ = true;
        break;
      }
    }
  }
}

void Curve::CreateCache() const {
  float start_input = keys_.front()->input();
  float end_input = keys_.back()->input();
  float input_span = end_input - start_input;

  unsigned samples =
      static_cast<unsigned>(ceilf(input_span / sample_rate_) + 1);
  cache_samples_.clear();
  cache_samples_.resize(samples, 0.0f);

  FunctionContext::Ref context = FunctionContext::Ref(CreateFunctionContext());

  for (unsigned ii = 0; ii < samples; ++ii) {
    cache_samples_[ii] = GetOutputInSpan(
        start_input + sample_rate_ * static_cast<float>(ii),
        down_cast<CurveFunctionContext*>(context.Get()));
  }

  cache_valid_ = true;
}

float Curve::GetOutputInSpan(float input, CurveFunctionContext* context) const {
  DCHECK(input >= keys_.front()->input());

  if (input >= keys_.back()->input()) {
    return keys_.back()->output();
  }

  // use the keys directly.
  unsigned start = 0;
  unsigned end = keys_.size();
  unsigned key_index;
  bool found = false;

  static const unsigned kKeysToSearch = 3U;

  // See if the context already has a index to the correct key.
  if (context) {
    key_index = context->last_key_index();
    // is that index in range.
    if (key_index < end - 1) {
      // Are we between these keys.
      if (keys_[key_index]->input() <= input &&
          keys_[key_index + 1]->input() > input) {
        // Yes!
        found = true;
      } else {
        // No, so check which way we need to go.
        if (input > keys_[key_index]->input()) {
          // search forward a few keys. If it's not within a few keys give up.
          unsigned check_end = std::max(key_index + kKeysToSearch, end);
          for (++key_index; key_index < check_end; ++key_index) {
            if (keys_[key_index]->input() <= input &&
                keys_[key_index + 1]->input() > input) {
              // Yes!
              found = true;
              break;
            }
          }
        } else if (key_index > 0) {
          // search backward a few keys. If it's not within a few keys give up.
          unsigned check_end = std::min(key_index - kKeysToSearch, 0U);
          for (--key_index; key_index >= check_end; --key_index) {
            if (keys_[key_index]->input() <= input &&
                keys_[key_index + 1]->input() > input) {
              // Yes!
              found = true;
              break;
            }
          }
        }
      }
    }
  }

  if (!found) {
    // TODO: If we assume the most common case is sampled keys and
    // constant intervals we can make a quick guess where that key is.

    // Find the current the keys that cover our input.
    while (start <= end) {
      unsigned mid = (start + end) / 2;
      if (input > keys_[mid]->input()) {
        start = mid + 1;
      } else {
        if (mid == 0) {
          break;
        }
        end = mid - 1;
      }
    }

    end = keys_.size();
    while (start < end) {
      if (keys_[start]->input() > input) {
        break;
      }
      ++start;
    }

    DCHECK(start > 0);
    DCHECK(start < end);

    key_index = start - 1;
  }

  const CurveKey* key = keys_[key_index];
  if (context) {
    context->set_last_key_index(key_index);
  }
  return key->GetOutputAtOffset(input - key->input(), key_index);
}

float Curve::Evaluate(float input, FunctionContext* context) const {
  if (context && !context->IsA(CurveFunctionContext::GetApparentClass())) {
    O3D_ERROR(service_locator())
        << "function context '" << context->GetClassName()
        << "' is wrong type for Curve";
    context = NULL;
  }

  if (keys_.empty()) {
    return 0.0f;
  }

  if (keys_.size() == 1) {
    return keys_.front()->output();
  }

  UpdateCurveInfo();

  float start_input = keys_.front()->input();
  float end_input = keys_.back()->input();
  float input_span = end_input - start_input;
  float start_output = keys_.front()->output();
  float end_output = keys_.back()->output();
  float output_delta = end_output - start_output;

  float output_offset = 0.0f;
  // check for pre-infinity
  if (input < start_input) {
    if (input_span <= 0.0f) {
      return start_output;
    }
    float pre_infinity_offset = start_input - input;
    switch (pre_infinity_) {
      case CONSTANT:
        return start_output;
      case LINEAR: {
        const CurveKey* second_key = keys_[1];
        float input_delta = second_key->input() - start_input;
        if (input_delta > kEpsilon) {
          return start_output - pre_infinity_offset *
              (second_key->output() - start_output) / input_delta;
        } else {
          return start_output;
        }
      }
      case CYCLE: {
        float cycle_count = ceilf(pre_infinity_offset / input_span);
        input += cycle_count * input_span;
        input = start_input + fmodf(input - start_input, input_span);
        break;
      }
      case CYCLE_RELATIVE: {
        float cycle_count = ceilf(pre_infinity_offset / input_span);
        input += cycle_count * input_span;
        input = start_input + fmodf(input - start_input, input_span);
        output_offset -= cycle_count * output_delta;
        break;
      }
      case OSCILLATE: {
        float cycle_count = ceilf(pre_infinity_offset / (2.0f * input_span));
        input += cycle_count * 2.0f * input_span;
        input = end_input - fabsf(input - end_input);
        break;
      }
      default:
        O3D_ERROR(service_locator()) << "invalid value for pre-infinity";
        return start_output;
    }
  } else if (input >= end_input) {
    // check for post-infinity
    if (input_span <= 0.0f) {
      return end_output;
    }
    float post_infinity_offset = input - end_input;
    switch (post_infinity_) {
      case CONSTANT:
        return end_output;
      case LINEAR: {
        const CurveKey* next_to_last_key = keys_[keys_.size() - 2];
        float input_delta = end_input - next_to_last_key->input();
        if (input_delta > kEpsilon) {
          return end_output + post_infinity_offset *
              (end_output - next_to_last_key->output()) /
              input_delta;
        } else {
          return end_output;
        }
      }
      case CYCLE: {
        float cycle_count = ceilf(post_infinity_offset / input_span);
        input -= cycle_count * input_span;
        input = start_input + fmodf(input - start_input, input_span);
        break;
      }
      case CYCLE_RELATIVE: {
        float cycle_count = floorf((input - start_input) / input_span);
        input -= cycle_count * input_span;
        input = start_input + fmodf(input - start_input, input_span);
        output_offset += cycle_count * output_delta;
        break;
      }
      case OSCILLATE: {
        float cycle_count = ceilf(post_infinity_offset / (2.0f *
                                                          input_span));
        input -= cycle_count * 2.0f * input_span;
        input = start_input + fabsf(input - start_input);
        break;
      }
      default:
        O3D_ERROR(service_locator()) << "invalid value for post-infinity";
        return end_output;
    }
  }

  // At this point input should be between start_input and end_input
  // inclusive.

  // If we are at end_input then just return end_output since we can't
  // interpolate end_input to anything past it.
  if (input >= end_input) {
    return end_output + output_offset;
  }

  if (!discontinuous_ && use_cache_) {
    // use the cache. The cache is implemented as a sampling of outputs at
    // specific intervals (sample_rate_). Linear interpolation is used between
    // samples. The is significantly faster than using the real keys but it
    // takes a lot more memory and it can't handle discontinuous animation like
    // camera cuts.
    if (!cache_valid_) {
      CreateCache();
    }
    float span_input = input - start_input;
    unsigned sample = static_cast<unsigned>(span_input / sample_rate_);

    DCHECK(sample < cache_samples_.size() - 1);

    float current_sample = cache_samples_[sample];
    if (num_step_keys_ == keys_.size()) {
      // It's all step keys so don't interpolate.
      return current_sample + output_offset;
    } else {
      float offset = fmodf(span_input, sample_rate_);
      float next_sample = cache_samples_[sample + 1];
      return current_sample + (next_sample - current_sample) *
          offset / sample_rate_ + output_offset;
    }
  } else {
    return GetOutputInSpan(
        input, down_cast<CurveFunctionContext*>(context)) + output_offset;
  }
}

ObjectBase::Ref Curve::Create(ServiceLocator* service_locator) {
  return ObjectBase::Ref(new Curve(service_locator));
}


static Float2 ReadFloat2(MemoryReadStream *stream) {
  float v1 = stream->ReadLittleEndianFloat32();
  float v2 = stream->ReadLittleEndianFloat32();
  Float2 value;
  value.setX(v1);
  value.setY(v2);
  return value;
}

// Some constants for error checking
const size_t kFloat2Size = 2 * sizeof(float);
const size_t kStepDataSize = 2 * sizeof(float);
const size_t kLinearDataSize = 2 * sizeof(float);

// not using sizeof(Float2) just in case this class adds ivars
// we're being more explicit here
const size_t kBezierDataSize = 2 * sizeof(float) + 2 * kFloat2Size;

bool Curve::LoadFromBinaryData(MemoryReadStream *stream) {
  // Make sure we have enough data for serialization ID and version
  if (stream->GetRemainingByteCount() < 4 + sizeof(int32)) {
    O3D_ERROR(service_locator()) << "invalid empty curve data";
    return false;
  }

  // To insure data integrity we expect four characters kSerializationID
  uint8 id[4];
  stream->Read(id, 4);

  if (memcmp(id, kSerializationID, 4)) {
    O3D_ERROR(service_locator())
        << "data object does not contain curve data";
    return false;
  }

  int32 version = stream->ReadLittleEndianInt32();
  if (version != 1) {
    O3D_ERROR(service_locator()) << "unknown version for curve data";
    return false;
  }

  while (!stream->EndOfStream()) {
    // switch on types of keys
    CurveKey::KeyType key_type =
        static_cast<CurveKey::KeyType>(stream->ReadByte());
    size_t available_bytes = stream->GetRemainingByteCount();

    switch (key_type) {
      case CurveKey::TYPE_STEP: {
        if (available_bytes < kStepDataSize) {
          O3D_ERROR(service_locator()) << "unexpected end of curve data";
          return false;
        }
        float input = stream->ReadLittleEndianFloat32();
        float output = stream->ReadLittleEndianFloat32();
        StepCurveKey* key = Create<StepCurveKey>();
        key->SetInput(input);
        key->SetOutput(output);
        break;
      }
      case CurveKey::TYPE_LINEAR: {
        if (available_bytes < kLinearDataSize) {
          O3D_ERROR(service_locator()) << "unexpected end of curve data";
          return false;
        }
        float input = stream->ReadLittleEndianFloat32();
        float output = stream->ReadLittleEndianFloat32();
        LinearCurveKey* key = Create<LinearCurveKey>();
        key->SetInput(input);
        key->SetOutput(output);
        break;
      }
      case CurveKey::TYPE_BEZIER: {
        if (available_bytes < kBezierDataSize) {
          O3D_ERROR(service_locator()) << "unexpected end of curve data";
          return false;
        }
        float input = stream->ReadLittleEndianFloat32();
        float output = stream->ReadLittleEndianFloat32();
        Float2 in_tangent = ReadFloat2(stream);
        Float2 out_tangent = ReadFloat2(stream);
        BezierCurveKey* key = Create<BezierCurveKey>();
        key->SetInput(input);
        key->SetOutput(output);
        key->SetInTangent(in_tangent);
        key->SetOutTangent(out_tangent);
        break;
      }
      default: {
        O3D_ERROR(service_locator()) << "invalid curve data";
        return false;  // unknown key type
      }
    }
  }
  return true;
}

bool Curve::Set(o3d::RawData *raw_data) {
  if (!raw_data) {
    O3D_ERROR(service_locator()) << "data object is null";
    return false;
  }
  return Set(raw_data, 0, raw_data->GetLength());
}

bool Curve::Set(o3d::RawData *raw_data,
                size_t offset,
                size_t length) {
  if (!raw_data) {
    O3D_ERROR(service_locator()) << "data object is null";
    return false;
  }

  if (!raw_data->IsOffsetLengthValid(offset, length)) {
    O3D_ERROR(service_locator()) << "illegal curve data offset or size";
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
