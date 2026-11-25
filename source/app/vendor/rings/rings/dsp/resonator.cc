// Copyright 2015 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Resonator.

#include "rings/dsp/resonator.h"
#include "plat.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/cosine_oscillator.h"
#include "stmlib/dsp/parameter_interpolator.h"

#include "rings/resources.h"

namespace rings {

using namespace std;
using namespace stmlib;

void Resonator::Init() {
  cached_modes_ = -1;
  for (int32_t i = 0; i < kMaxModes; ++i) {
    state1_[i] = state2_[i] = 0.0f;
  }

  set_frequency(220.0f / kSampleRate);
  set_structure(0.25f);
  set_brightness(0.5f);
  set_damping(0.3f);
  set_position(0.999f);
  previous_position_ = 0.0f;
  set_resolution(kMaxModes);
}

PLAT_HOT int32_t Resonator::ComputeFilters() {
  if (cached_modes_ >= 0 && cached_resolution_ == resolution_ &&
      cached_frequency_ == frequency_ && cached_structure_ == structure_ &&
      cached_brightness_ == brightness_ && cached_damping_ == damping_) {
    return cached_modes_;
  }
  cached_resolution_ = resolution_;
  cached_frequency_ = frequency_;
  cached_structure_ = structure_;
  cached_brightness_ = brightness_;
  cached_damping_ = damping_;
  float stiffness = Interpolate(lut_stiffness, structure_, 256.0f);
  float harmonic = frequency_;
  float stretch_factor = 1.0f; 
  float q = 500.0f * Interpolate(
      lut_4_decades,
      damping_,
      256.0f);
  float brightness_attenuation = 1.0f - structure_;
  // Reduces the range of brightness when structure is very low, to prevent
  // clipping.
  brightness_attenuation *= brightness_attenuation;
  brightness_attenuation *= brightness_attenuation;
  brightness_attenuation *= brightness_attenuation;
  float brightness = brightness_ * (1.0f - 0.2f * brightness_attenuation);
  float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;
  float q_loss_damping_rate = structure_ * (2.0f - structure_) * 0.1f;
  int32_t num_modes = 0;
  for (int32_t i = 0; i < min(kMaxModes, resolution_); ++i) {
    float partial_frequency = harmonic * stretch_factor;
    if (partial_frequency >= 0.49f) {
      partial_frequency = 0.49f;
    } else {
      num_modes = i + 1;
    }
    float g = OnePole::tan<FREQUENCY_FAST>(partial_frequency);
    float r = 1.0f / (1.0f + partial_frequency * q);
    g_[i] = g;
    r_[i] = r;
    h_[i] = 1.0f / (1.0f + r * g + g * g);
    stretch_factor += stiffness;
    if (stiffness < 0.0f) {
      // Make sure that the partials do not fold back into negative frequencies.
      stiffness *= 0.93f;
    } else {
      // This helps adding a few extra partials in the highest frequencies.
      stiffness *= 0.98f;
    }
    // This prevents the highest partials from decaying too fast.
    q_loss += q_loss_damping_rate * (1.0f - q_loss);
    harmonic += frequency_;
    q *= q_loss;
  }
  
  cached_modes_ = num_modes;
  return num_modes;
}

// Upstream Svf::Process<BAND_PASS> arithmetic and addition order. With a
// stationary pickup, four independent partials can run through a block
// together while their histories stay local.
static inline __attribute__((always_inline)) float ModalBand(float input, float g, float r, float h,
                                                            float& state1, float& state2) {
  float hp = (input - r * state1 - g * state1 - state2) * h;
  float v = g * hp;
  float bp = v + state1;
  state1 = v + bp;
  float w = g * bp;
  float lp = w + state2;
  state2 = w + lp;
  return bp;
}

inline float Resonator::BandPass(int i, float input) {
  return ModalBand(input, g_[i], r_[i], h_[i], state1_[i], state2_[i]);
}

template<int lanes>
void Resonator::AddFixedModes(int first, const float* weights, const float* input,
                              float* odd, float* even, size_t size) {
  float g[lanes], r[lanes], h[lanes], s1[lanes], s2[lanes];
  for (int k = 0; k < lanes; ++k) {
    g[k] = g_[first+k]; r[k] = r_[first+k]; h[k] = h_[first+k];
    s1[k] = state1_[first+k]; s2[k] = state2_[first+k];
  }
  for (size_t j = 0; j < size; ++j) {
    float y[lanes];
    for (int k = 0; k < lanes; ++k)
      y[k] = weights[k] * ModalBand(input[j], g[k], r[k], h[k], s1[k], s2[k]);
    /* genAMB: hold the two partial sums in registers across the lane pairs
       rather than reading and writing odd[j] and even[j] once per pair. The
       additions keep their order, so this is bit-exact. */
    float o = odd[j], e = even[j];
    for (int k = 0; k < lanes; k += 2) {
      o += y[k];
      e += y[k+1];
    }
    odd[j] = o;
    even[j] = e;
  }
  for (int k = 0; k < lanes; ++k) {
    state1_[first+k] = s1[k]; state2_[first+k] = s2[k];
  }
}

/* genAMB: the modal resonator is the single hottest loop in the firmware, so it
   runs from executable SRAM (plat.h, PLAT_HOT). Placement only - the arithmetic
   is Emilie Gillet's unchanged. Part::Process runs from the flash and cannot
   branch this far, so it calls through rings_resonator_process_ptr below. */
PLAT_HOT void Resonator::Process(const float* in, float* out, float* aux, size_t size) {
  int32_t num_modes = ComputeFilters();
  
  if (!size) return;
  if (size <= kMaxBlockSize &&
      previous_position_ + (position_ - previous_position_) / static_cast<float>(size) == previous_position_) {
    // Each partial then has one pickup weight for the entire block. The
    // addition order and SVF state updates match the interpolated path.
    float excitation[kMaxBlockSize];
    for (size_t j = 0; j < size; ++j) {
      excitation[j] = in[j] * 0.125f;
      out[j] = aux[j] = 0.0f;
    }
    CosineOscillator amplitudes;
    amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(previous_position_);
    int32_t i = 0;
    for (; i + 2 < num_modes; i += 4) {
      float weights[4];
      for (int j = 0; j < 4; ++j) weights[j] = amplitudes.Next();
      AddFixedModes<4>(i, weights, excitation, out, aux, size);
    }
    if (i < num_modes) {
      float weights[2] = {amplitudes.Next(), amplitudes.Next()};
      AddFixedModes<2>(i, weights, excitation, out, aux, size);
    }
    return;
  }
  ParameterInterpolator position(&previous_position_, position_, size);
  while (size--) {
    CosineOscillator amplitudes;
    amplitudes.Init<COSINE_OSCILLATOR_APPROXIMATE>(position.Next());
    
    float input = *in++ * 0.125f;
    float odd = 0.0f;
    float even = 0.0f;
    amplitudes.Start();
    for (int32_t i = 0; i < num_modes;) {
      odd += amplitudes.Next() * BandPass(i++, input);
      even += amplitudes.Next() * BandPass(i++, input);
    }
    *out++ = odd;
    *aux++ = even;
  }
}

}  // namespace rings

namespace rings {
/* Trampoline out of the flash into the SRAM section. One indirect call per
   voice per block; the volatile pointer stops the compiler folding it back. */
PLAT_HOT void rings_resonator_process_hot(Resonator* r, const float* in, float* out, float* aux, size_t size) {
  r->Process(in, out, aux, size);
}
void (* volatile const rings_resonator_process_ptr)(Resonator*, const float*, float*, float*, size_t) =
    rings_resonator_process_hot;
}  // namespace rings
