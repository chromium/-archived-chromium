// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "config.h"

#if ENABLE(SVG)
#include "SVGPaintServerGradient.h"
#include "SVGPaintServerLinearGradient.h"
#include "SVGPaintServerRadialGradient.h"

#include "GraphicsContext.h"
#include "Path.h"
#include "PlatformContextSkia.h"
#include "RenderObject.h"
#include "RenderPath.h"
#include "RenderStyle.h"
#include "SVGGradientElement.h"
#include "SkiaSupport.h"

#include "SkGradientShader.h"
#include "SkPath.h"
#include "SkMatrix.h"
#include "SkiaUtils.h"

namespace WebCore {

// Helper class used by linear & radial gradient
class SkiaGradientBuilder
{
public:
    SkiaGradientBuilder(const Vector<SVGGradientStop>& stops, float opacity) {
        count_ = stops.size();
        unsigned cur = 0;
        
        // If the gradient doesn't span the full width, add pseudo-stops
        // at the endpoints
        if (stops[0].first > 0) {
            count_++;
            cur++;
        }

        if (stops[stops.size()-1].first < 1) 
            count_++;

        colors_ = new SkColor[count_];
        pos_ = new SkScalar[count_];
        float last = 0.0;

        // Loop through the WebKit-provided array of SVG gradient stops and
        // put it in a format suitable for providing to Skia
        for (unsigned i = 0; i < stops.size(); ++i, ++cur) {
            float offset = stops[i].first;
            Color color = stops[i].second;

            colors_[cur]=SkColorSetARGB(static_cast<int>(opacity*color.alpha()),
              color.red(),
              color.green(),
              color.blue());
            last=pos_[cur]=std::max(last, offset);
        }

        if (stops[0].first > 0) {
            colors_[0] = colors_[1];
            pos_[0] = 0;
        }
        if (stops[stops.size()-1].first < 1) {
            colors_[count_-1] = colors_[count_-2];
            pos_[count_-1] = 1;
        }
    }

    ~SkiaGradientBuilder() {
        delete[] colors_;
        delete[] pos_;
    }

    const SkColor* colors() const { return colors_; }
    const SkScalar* pos() const { return pos_; }
    unsigned count() const { return count_; }

private:
    SkColor* colors_;
    SkScalar* pos_;
    unsigned count_;
};

bool SVGPaintServerGradient::setup(GraphicsContext*& context, 
    const RenderObject* object, 
    SVGPaintTargetType type, 
    bool isPaintingText) const
{
    m_ownerElement->buildGradient();

    RenderStyle* style = object->style();
    bool isFilled = 
        (type & ApplyToFillTargetType) && 
         style->svgStyle()->hasFill();
    bool isStroked = 
        (type & ApplyToStrokeTargetType) && 
         style->svgStyle()->hasStroke();

    if(!gradientStops().size())
        return false;

    if(gradientStops().size()==1) {
        context->setFillColor(gradientStops()[0].second);
        return true;
    }

    // Create a gradient builder helper to generate the data 
    // we'll need to provide Skia
    SkiaGradientBuilder builder(gradientStops(), 
        isFilled ? style->svgStyle()->fillOpacity() : 
                   style->svgStyle()->strokeOpacity());

    SkShader::TileMode tile_mode;

    // Convert SVG spread modes to Skia tile modes
    switch(spreadMethod())
    {
    default:
    case SpreadMethodPad:
        tile_mode = SkShader::kClamp_TileMode; break;
    case SpreadMethodReflect:
        tile_mode = SkShader::kMirror_TileMode; break;
    case SpreadMethodRepeat:
        tile_mode = SkShader::kRepeat_TileMode; break;
    }

    SkShader* shader = NULL;

    SkMatrix matrix;

    // Calculate a matrix to transform a gradient to fit the bounding box
    if (boundingBoxMode()) {
        matrix.reset();
        SkRect rc = boundingBoxForCurrentStroke(context);

        matrix.preTranslate(rc.fLeft, rc.fTop);
        matrix.preScale(rc.width(), rc.height());
        matrix.preConcat(gradientTransform());
    } else
        matrix = gradientTransform();

    if (this->type() == LinearGradientPaintServer) {
        const SVGPaintServerLinearGradient* linear = 
            static_cast<const SVGPaintServerLinearGradient*>(this);

        SkPoint pts[2];

        pts[0].fX = linear->gradientStart().x();
        pts[0].fY = linear->gradientStart().y();
        pts[1].fX = linear->gradientEnd().x();
        pts[1].fY = linear->gradientEnd().y();

        shader = SkGradientShader::CreateLinear(pts, 
            builder.colors(), builder.pos(), builder.count(), tile_mode);
    } else if (this->type() == RadialGradientPaintServer) {
        const SVGPaintServerRadialGradient* radial = 
            static_cast<const SVGPaintServerRadialGradient*>(this);

        SkPoint center;
        SkScalar radius;

        center.fX = radial->gradientCenter().x();
        center.fY = radial->gradientCenter().y();
        radius = radial->gradientRadius();

        shader = SkGradientShader::CreateRadial(
            center, radius, builder.colors(), builder.pos(), 
            builder.count(), tile_mode);

    } else {
        return false;
    }

    if (isPaintingText) {
        if (isFilled) {
            context->setTextDrawingMode(cTextFill);
        }

        if (isStroked) {
            context->setTextDrawingMode(cTextStroke);
        }
    }

    if (isStroked) {
        applyStrokeStyleToContext(context, style, object);
    }

    if (shader) {
        shader->setLocalMatrix(matrix);
        context->platformContext()->setGradient(shader);

        return true;
    }

    return false;
}

} // namespace WebCore

#endif
