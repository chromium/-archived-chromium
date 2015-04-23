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


// This file implements the render state-related GAPI functions on GL.

#include "command_buffer/common/cross/cmd_buffer_format.h"
#include "command_buffer/service/cross/gl/gapi_gl.h"

namespace o3d {
namespace command_buffer {

namespace {

GLenum kGLPolygonModes[] = {
  GL_POINT,
  GL_LINE,
  GL_FILL,
};
COMPILE_ASSERT(GAPIInterface::NUM_POLYGON_MODE == arraysize(kGLPolygonModes),
               kGLPolygonModes_does_not_match_GAPIInterface_PolygonMode);

GLenum kGLComparison[] = {
  GL_NEVER,
  GL_LESS,
  GL_EQUAL,
  GL_LEQUAL,
  GL_GREATER,
  GL_NOTEQUAL,
  GL_GEQUAL,
  GL_ALWAYS,
};
COMPILE_ASSERT(GAPIInterface::NUM_COMPARISON == arraysize(kGLComparison),
               kGLComparison_does_not_match_GAPIInterface_Comparison);

GLenum kGLBlendFunc[] = {
  GL_ZERO,
  GL_ONE,
  GL_SRC_COLOR,
  GL_ONE_MINUS_SRC_COLOR,
  GL_SRC_ALPHA,
  GL_ONE_MINUS_SRC_ALPHA,
  GL_DST_ALPHA,
  GL_ONE_MINUS_DST_ALPHA,
  GL_DST_COLOR,
  GL_ONE_MINUS_DST_COLOR,
  GL_SRC_ALPHA_SATURATE,
  GL_CONSTANT_COLOR,
  GL_ONE_MINUS_CONSTANT_COLOR,
};
COMPILE_ASSERT(GAPIInterface::NUM_BLEND_FUNC == arraysize(kGLBlendFunc),
               kGLBlendFunc_does_not_match_GAPIInterface_BlendFunc);

GLenum kGLBlendEq[] = {
  GL_FUNC_ADD,
  GL_FUNC_SUBTRACT,
  GL_FUNC_REVERSE_SUBTRACT,
  GL_MIN,
  GL_MAX,
};
COMPILE_ASSERT(GAPIInterface::NUM_BLEND_EQ == arraysize(kGLBlendEq),
               kGLBlendEq_does_not_match_GAPIInterface_BlendEq);

GLenum kGLStencilOp[] = {
  GL_KEEP,
  GL_ZERO,
  GL_REPLACE,
  GL_INCR,
  GL_DECR,
  GL_INVERT,
  GL_INCR_WRAP,
  GL_DECR_WRAP,
};
COMPILE_ASSERT(GAPIInterface::NUM_STENCIL_OP == arraysize(kGLStencilOp),
               kGLStencilOp_does_not_match_GAPIInterface_StencilOp);

// Check that the definition of the counter-clockwise func/ops match the
// clockwise ones, just shifted by 16 bits, so that we can use
// DecodeStencilFuncOps on both of them.
#define CHECK_CCW_MATCHES_CW(FIELD)                                       \
  COMPILE_ASSERT(set_stencil_test::CW ## FIELD::kLength ==                \
                 set_stencil_test::CCW ## FIELD::kLength,                 \
                 CCW ## FIELD ## _length_does_not_match_ ## CW ## FIELD); \
  COMPILE_ASSERT(set_stencil_test::CW ## FIELD::kShift + 16 ==            \
                 set_stencil_test::CCW ## FIELD::kShift,                  \
                 CCW ## FIELD ## _shift_does_not_match_ ## CW ## FIELD)

CHECK_CCW_MATCHES_CW(Func);
CHECK_CCW_MATCHES_CW(PassOp);
CHECK_CCW_MATCHES_CW(FailOp);
CHECK_CCW_MATCHES_CW(ZFailOp);

#undef CHECK_CCW_MATCHES_CW

// Decodes stencil test function and operations from the bitfield.
void DecodeStencilFuncOps(Uint32 params,
                          GLenum *func,
                          GLenum *pass,
                          GLenum *fail,
                          GLenum *zfail) {
  namespace cmd = set_stencil_test;
  // Sanity check. The value has already been tested in
  // GAPIDecoder::DecodeSetStencilTest in gapi_decoder.cc.
  DCHECK_EQ(cmd::Unused1::Get(params), 0);
  // Check that the bitmask get cannot generate values outside of the allowed
  // range.
  COMPILE_ASSERT(cmd::CWFunc::kMask < GAPIInterface::NUM_COMPARISON,
                 set_stencil_test_CWFunc_may_produce_invalid_values);
  *func = kGLComparison[cmd::CWFunc::Get(params)];

  COMPILE_ASSERT(cmd::CWPassOp::kMask < GAPIInterface::NUM_STENCIL_OP,
                 set_stencil_test_CWPassOp_may_produce_invalid_values);
  *pass = kGLStencilOp[cmd::CWPassOp::Get(params)];

  COMPILE_ASSERT(cmd::CWFailOp::kMask < GAPIInterface::NUM_STENCIL_OP,
                 set_stencil_test_CWFailOp_may_produce_invalid_values);
  *fail = kGLStencilOp[cmd::CWFailOp::Get(params)];

  COMPILE_ASSERT(cmd::CWZFailOp::kMask < GAPIInterface::NUM_STENCIL_OP,
                 set_stencil_test_CWZFailOp_may_produce_invalid_values);
  *zfail = kGLStencilOp[cmd::CWZFailOp::Get(params)];
}

}  // anonymous namespace

void GAPIGL::SetViewport(unsigned int x,
                         unsigned int y,
                         unsigned int width,
                         unsigned int height,
                         float z_min,
                         float z_max) {
  glViewport(x, y, width, height);
  glDepthRange(z_min, z_max);
  // Update the helper constant used for the D3D -> GL remapping.
  // See effect_gl.cc for details.
  glProgramEnvParameter4fARB(GL_VERTEX_PROGRAM_ARB, 0,
                             1.f / width, 1.f / height, 2.f, 0.f);
  CHECK_GL_ERROR();
}

void GAPIGL::SetScissor(bool enable,
                        unsigned int x,
                        unsigned int y,
                        unsigned int width,
                        unsigned int height) {
  if (enable) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
  } else {
    glDisable(GL_SCISSOR_TEST);
  }
}

void GAPIGL::SetPointLineRaster(bool line_smooth,
                                bool point_sprite,
                                float point_size) {
  if (line_smooth) {
    glEnable(GL_LINE_SMOOTH);
  } else {
    glDisable(GL_LINE_SMOOTH);
  }
  if (point_sprite) {
    glEnable(GL_POINT_SPRITE);
    // TODO: check which TC gets affected by point sprites in D3D.
    glActiveTextureARB(GL_TEXTURE0);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_TRUE);
  } else {
    glActiveTextureARB(GL_TEXTURE0);
    glTexEnvi(GL_POINT_SPRITE, GL_COORD_REPLACE, GL_FALSE);
    glDisable(GL_POINT_SPRITE);
  }
  glPointSize(point_size);
}

void GAPIGL::SetPolygonOffset(float slope_factor, float units) {
  glPolygonOffset(slope_factor, units);
}

void GAPIGL::SetPolygonRaster(PolygonMode fill_mode,
                              FaceCullMode cull_mode) {
  DCHECK_LT(fill_mode, NUM_POLYGON_MODE);
  glPolygonMode(GL_FRONT_AND_BACK, kGLPolygonModes[fill_mode]);
  DCHECK_LT(cull_mode, NUM_FACE_CULL_MODE);
  switch (cull_mode) {
    case CULL_CW:
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
      break;
    case CULL_CCW:
      glEnable(GL_CULL_FACE);
      glCullFace(GL_FRONT);
      break;
    default:
      glDisable(GL_CULL_FACE);
      break;
  }
}

void GAPIGL::SetAlphaTest(bool enable,
                          float reference,
                          Comparison comp) {
  DCHECK_LT(comp, NUM_COMPARISON);
  if (enable) {
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(kGLComparison[comp], reference);
  } else {
    glDisable(GL_ALPHA_TEST);
  }
}

void GAPIGL::SetDepthTest(bool enable,
                          bool write_enable,
                          Comparison comp) {
  DCHECK_LT(comp, NUM_COMPARISON);
  if (enable) {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(kGLComparison[comp]);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  glDepthMask(write_enable);
}

void GAPIGL::SetStencilTest(bool enable,
                            bool separate_ccw,
                            unsigned int write_mask,
                            unsigned int compare_mask,
                            unsigned int ref,
                            Uint32 func_ops) {
  if (enable) {
    glEnable(GL_STENCIL_TEST);
    glStencilMask(write_mask);

    GLenum func;
    GLenum pass;
    GLenum fail;
    GLenum zfail;
    DecodeStencilFuncOps(func_ops, &func, &pass, &fail, &zfail);
    if (separate_ccw) {
      glStencilFuncSeparate(GL_FRONT, func, ref, compare_mask);
      glStencilOpSeparate(GL_FRONT, pass, fail, zfail);
      // Extract upper 16 bits.
      Uint32 ccw_func_ops = BitField<16, 16>::Get(func_ops);
      GLenum ccw_func;
      GLenum ccw_pass;
      GLenum ccw_fail;
      GLenum ccw_zfail;
      DecodeStencilFuncOps(ccw_func_ops, &ccw_func, &ccw_pass, &ccw_fail,
                           &ccw_zfail);
      glStencilFuncSeparate(GL_BACK, ccw_func, ref, compare_mask);
      glStencilOpSeparate(GL_BACK, ccw_pass, ccw_fail, ccw_zfail);
    } else {
      glStencilFunc(func, ref, compare_mask);
      glStencilOp(pass, fail, zfail);
    }
  } else {
    glDisable(GL_STENCIL_TEST);
  }
}

void GAPIGL::SetColorWrite(bool red,
                           bool green,
                           bool blue,
                           bool alpha,
                           bool dither) {
  glColorMask(red, green, blue, alpha);
  if (dither) {
    glEnable(GL_DITHER);
  } else {
    glDisable(GL_DITHER);
  }
}

void GAPIGL::SetBlending(bool enable,
                         bool separate_alpha,
                         BlendEq color_eq,
                         BlendFunc color_src_func,
                         BlendFunc color_dst_func,
                         BlendEq alpha_eq,
                         BlendFunc alpha_src_func,
                         BlendFunc alpha_dst_func) {
  DCHECK_LT(color_eq, NUM_BLEND_EQ);
  DCHECK_LT(color_src_func, NUM_BLEND_FUNC);
  DCHECK_LT(color_dst_func, NUM_BLEND_FUNC);
  DCHECK_LT(alpha_eq, NUM_BLEND_EQ);
  DCHECK_LT(alpha_src_func, NUM_BLEND_FUNC);
  DCHECK_LT(alpha_dst_func, NUM_BLEND_FUNC);
  if (enable) {
    glEnable(GL_BLEND);
    GLenum gl_color_eq = kGLBlendEq[color_eq];
    GLenum gl_color_src_func = kGLBlendFunc[color_src_func];
    GLenum gl_color_dst_func = kGLBlendFunc[color_dst_func];
    if (separate_alpha) {
      GLenum gl_alpha_eq = kGLBlendEq[alpha_eq];
      GLenum gl_alpha_src_func = kGLBlendFunc[alpha_src_func];
      GLenum gl_alpha_dst_func = kGLBlendFunc[alpha_dst_func];
      glBlendFuncSeparate(gl_color_src_func, gl_color_dst_func,
                          gl_alpha_src_func, gl_alpha_dst_func);
      glBlendEquationSeparate(gl_color_eq, gl_alpha_eq);
    } else {
      glBlendFunc(gl_color_src_func, gl_color_dst_func);
      glBlendEquation(gl_color_eq);
    }
  } else {
    glDisable(GL_BLEND);
  }
}

void GAPIGL::SetBlendingColor(const RGBA &color) {
  glBlendColor(color.red, color.green, color.blue, color.alpha);
}

}  // namespace command_buffer
}  // namespace o3d
