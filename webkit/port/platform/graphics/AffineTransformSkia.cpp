// Copyright (c) 2008, Google Inc.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
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
#include "AffineTransform.h"

#include "FloatRect.h"
#include "IntRect.h"

#include "SkiaUtils.h"

namespace WebCore {

static const double deg2rad = 0.017453292519943295769; // pi/180

AffineTransform::AffineTransform()
{
    m_transform.reset();
}

AffineTransform::AffineTransform(double a, double b, double c, double d,
                                 double e, double f)
{
    setMatrix(a, b, c, d, e, f);
}

AffineTransform::AffineTransform(const SkMatrix& matrix) : m_transform(matrix)
{
}

void AffineTransform::setMatrix(double a, double b, double c, double d,
                                double e, double f)
{
    m_transform.reset();

    m_transform.setScaleX(WebCoreDoubleToSkScalar(a));
    m_transform.setSkewX(WebCoreDoubleToSkScalar(c));
    m_transform.setTranslateX(WebCoreDoubleToSkScalar(e));

    m_transform.setScaleY(WebCoreDoubleToSkScalar(d));
    m_transform.setSkewY(WebCoreDoubleToSkScalar(b));
    m_transform.setTranslateY(WebCoreDoubleToSkScalar(f));
}

void AffineTransform::map(double x, double y, double *x2, double *y2) const
{
    SkPoint src, dst;
    src.set(WebCoreDoubleToSkScalar(x), WebCoreDoubleToSkScalar(y));
    m_transform.mapPoints(&dst, &src, 1);

    *x2 = SkScalarToDouble(dst.fX);
    *y2 = SkScalarToDouble(dst.fY);
}

IntRect AffineTransform::mapRect(const IntRect& src) const
{
    SkRect  dst;
    m_transform.mapRect(&dst, src);
    return enclosingIntRect(dst);
}

FloatRect AffineTransform::mapRect(const FloatRect& src) const
{
    SkRect dst;
    m_transform.mapRect(&dst, src);
    return dst;
}

bool AffineTransform::isIdentity() const
{
    return m_transform.isIdentity();
}

void AffineTransform::reset()
{
    m_transform.reset();
}

AffineTransform &AffineTransform::scale(double sx, double sy)
{
    m_transform.preScale(WebCoreDoubleToSkScalar(sx), WebCoreDoubleToSkScalar(sy), 0, 0);
    return *this;
}

AffineTransform &AffineTransform::rotate(double d)
{
    m_transform.preRotate(WebCoreDoubleToSkScalar(d), 0, 0);
    return *this;
}

AffineTransform &AffineTransform::translate(double tx, double ty)
{
    m_transform.preTranslate(WebCoreDoubleToSkScalar(tx), WebCoreDoubleToSkScalar(ty));
    return *this;
}

AffineTransform &AffineTransform::shear(double sx, double sy)
{
    m_transform.preSkew(WebCoreDoubleToSkScalar(sx), WebCoreDoubleToSkScalar(sy), 0, 0);
    return *this;
}

double AffineTransform::det() const
{
    return  SkScalarToDouble(m_transform.getScaleX()) * SkScalarToDouble(m_transform.getScaleY()) -
            SkScalarToDouble(m_transform.getSkewY())  * SkScalarToDouble(m_transform.getSkewX());
}

AffineTransform AffineTransform::inverse() const
{
    AffineTransform inverse;
    m_transform.invert(&inverse.m_transform);
    return inverse;
}

AffineTransform::operator SkMatrix() const
{
    return m_transform;
}

bool AffineTransform::operator==(const AffineTransform& m2) const
{
    return m_transform == m2.m_transform;
}

AffineTransform &AffineTransform::operator*=(const AffineTransform& m2)
{
    m_transform.setConcat(m2.m_transform, m_transform);
    return *this;
}

AffineTransform AffineTransform::operator*(const AffineTransform& m2)
{
    AffineTransform cat;
    
    cat.m_transform.setConcat(m2.m_transform, m_transform);
    return cat;
}

double AffineTransform::a() const
{
    return SkScalarToDouble(m_transform.getScaleX());
}
void AffineTransform::setA(double a)
{
    m_transform.setScaleX(WebCoreDoubleToSkScalar(a));
}

double AffineTransform::b() const
{
    return SkScalarToDouble(m_transform.getSkewY());
}
void AffineTransform::setB(double b)
{
    m_transform.setSkewY(WebCoreDoubleToSkScalar(b));
}

double AffineTransform::c() const
{
    return SkScalarToDouble(m_transform.getSkewX());
}
void AffineTransform::setC(double c)
{
    m_transform.setSkewX(WebCoreDoubleToSkScalar(c));
}

double AffineTransform::d() const
{
    return SkScalarToDouble(m_transform.getScaleY());
}
void AffineTransform::setD(double d)
{
    m_transform.setScaleY(WebCoreDoubleToSkScalar(d));
}

double AffineTransform::e() const
{
    return SkScalarToDouble(m_transform.getTranslateX());
}
void AffineTransform::setE(double e)
{
    m_transform.setTranslateX(WebCoreDoubleToSkScalar(e));
}

double AffineTransform::f() const
{
    return SkScalarToDouble(m_transform.getTranslateY());
}
void AffineTransform::setF(double f)
{
    m_transform.setTranslateY(WebCoreDoubleToSkScalar(f));
}

} // namespace WebCore
