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


// This file contains the declaration for the Canvas class.

#ifndef O3D_CORE_CROSS_CANVAS_H_
#define O3D_CORE_CROSS_CANVAS_H_

#include <vector>

#include "core/cross/param_object.h"
#include "core/cross/param.h"
#include "core/cross/texture.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace o3d {

class CanvasPaint;

// The Canvas class provides an API for drawing text and 2D primitives onto
// a 2D bitmap surface whose contents can be transfered to a compatible
// Texture2D object via the CopyToTexture() method.  Each Canvas object 
// maintains a stack of 2D transformation matrices which allow fine control over
// the placement of drawable elements.  Both geometry and drawing coordinates
// provided to every draw call are transformed by the concatenation of
// all matrices in the stack.
class Canvas : public ParamObject {
 public:
  typedef SmartPointer<Canvas> Ref;

  virtual ~Canvas();

  // Sets the size of the bitmap surface this Canvas object will use.
  bool SetSize(int width, int height);

  // Initializes the bitmap's pixels with the specified color.
  // Parameters:
  //  red: Red color value (between 0 and 1.0)
  //  green: Green color value (between 0 and 1.0)
  //  blue: Blue color value (between 0 and 1.0)
  //  alpha: Transparency value (0 = transparent, 1 = opaque)
  void Clear(float red, float green, float blue, float alpha);

  // Draws a rectangle using the specified coordinates. The rectangle will be
  // filled based on the color and shader of the specified paint.
  // Parameters:
  //  left The left side of the rectangle to be drawn
  //  top The top side of the rectangle to be drawn
  //  right The right side of the rectangle to be drawn
  //  bottom The bottom side of the rectangle to be drawn
  //  paint The CanvasPaint object used to draw the rectangle.
  void DrawRect(float left,
                float top,
                float right,
                float bottom,
                CanvasPaint* paint);

  // Draws the text, with origin at (x,y), using the specified paint. The origin
  // is interpreted based on the textAlign property in the paint.
  // Parameters:
  //   text: String of text to be drawn
  //   x: The x coordinate for the text origin
  //   y: The y coordinate for the text origin
  //   paint: The CanvasPaint object that specifies the text style, size, etc
  void DrawText(const String& text, float x, float y, CanvasPaint* paint);

  // Draws the text with its baseline along the
  // specified path. The paint's textAlign property determines where along the
  // path to start the text.  The path must contain at least two positions.
  // Parameters:
  //  text: String of text to be drawn
  //  positions: An array of x,y positions making up the path.
  //  horizonal_offset: The distance along the path to add to the text starting
  //                    position.
  //  vertical_offset: The distance above(-) or below(+) the path to position
  //                   the text.
  //  paint: The CanvasPaint object that specifies the text style, size, etc.
  void DrawTextOnPath(const String& text,
                      std::vector<Float2> positions,
                      float horizontal_offset,
                      float vertical_offset,
                      CanvasPaint* paint);

  // Draws the contents of the specified texture onto the canvas surface.
  // The bottom left corner of the bitmap will be at (x, y) and transformed by
  // the current matrix.
  // Parameters:
  //   texture: Pointer to Texture2D object where the bitmap is extracted from
  //   left: The position of the left side of the bitmap.
  //   bottom: The position of the bottom side of the bitmap.
  void DrawBitmap(Texture2D* texture,
                  float left,
                  float bottom);

  // This call saves the current matrix information, and pushes a copy onto a
  // private stack. Subsequent calls to translate, scale, rotate, all operate on
  // this copy. When the balancing call to RestoreState() is made, this copy is
  // deleted and the previous matrix is restored.
  void SaveMatrix() {  sk_canvas_.save();  }

  // This call balances a previous call to SaveMatrix(), and is used to remove
  // all modifications to the matrix since the last SaveMatrix call.
  void RestoreMatrix() {  sk_canvas_.restore();  }

  // Preconcats the current canvas matrix with the specified rotation.
  // Parameters:
  //   degrees: The amount to rotate, in degrees
  void Rotate(float degrees);

  // Preconcats the current canvas matrix with the specified scale.
  // Parameters:
  //   sx: The amount to scale in x
  //   sy: The amount to scale in y
  void Scale(float sx, float sy);

  // Preconcats the current canvas matrix with the specified translation.
  // Parameters:
  //   dx: The amount to translate in x
  //   dy: The amount to translate in y
  void Translate(float dx, float dy);

  // Copies the contents of the Canvas bitmap to a Texture2D object.  The
  // texture object must have the same size as the canvas and a ARGB8 or XRGB8
  // format.  All mip levels of the the texture will be filled.
  // Parameters:
  //   texture_2d:  The texture object to copy the bitmap to.
  bool CopyToTexture(Texture2D* texture_2d);

  // Returns the width of the canvas bitmap.
  int width() { return width_; }

  // Returns the height of the canvas bitmap.
  int height() { return height_; }

 protected:
  explicit Canvas(ServiceLocator* service_locator);

 private:
  friend class IClassManager;
  static ObjectBase::Ref Create(ServiceLocator* service_locator);

  // Local area in memory to perform drawing operations.
  SkBitmap sk_bitmap_;
  SkCanvas sk_canvas_;

  // bitmap width and height.
  int width_;
  int height_;

  O3D_DECL_CLASS(Canvas, ParamObject)
  DISALLOW_COPY_AND_ASSIGN(Canvas);
};


}  // namespace o3d

#endif  // O3D_CORE_CROSS_CANVAS_H_
