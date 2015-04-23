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


#ifndef O3D_IMPORT_CROSS_PRECOMPILE_H_
#define O3D_IMPORT_CROSS_PRECOMPILE_H_

#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <d3dx9.h>
#endif

#ifdef __cplusplus

#include <Cg/cg.h>
#include <Cg/cgGL.h>
#include <FCollada.h>
#include <FCDocument/FCDocument.h>
#include <FCDocument/FCDocumentTools.h>
#include <FCDocument/FCDAnimated.h>
#include <FCDocument/FCDAnimationCurve.h>
#include <FCDocument/FCDAnimationKey.h>
#include <FCDocument/FCDEffect.h>
#include <FCDocument/FCDEffectCode.h>
#include <FCDocument/FCDEffectParameter.h>
#include <FCDocument/FCDEffectParameterSurface.h>
#include <FCDocument/FCDEffectParameterSampler.h>
#include <FCDocument/FCDEffectPass.h>
#include <FCDocument/FCDEffectPassShader.h>
#include <FCDocument/FCDEffectPassState.h>
#include <FCDocument/FCDEffectProfile.h>
#include <FCDocument/FCDEffectProfileFX.h>
#include <FCDocument/FCDEffectStandard.h>
#include <FCDocument/FCDEffectTechnique.h>
#include <FCDocument/FCDExtra.h>
#include <FCDocument/FCDEntity.h>
#include <FCDocument/FCDImage.h>
#include <FCDocument/FCDMaterial.h>
#include <FCDocument/FCDMaterialInstance.h>
#include <FCDocument/FCDLibrary.h>
#include <FCDocument/FCDSceneNode.h>
#include <FCDocument/FCDCamera.h>
#include <FCDocument/FCDGeometry.h>
#include <FCDocument/FCDGeometryInstance.h>
#include <FCDocument/FCDGeometryMesh.h>
#include <FCDocument/FCDGeometryPolygons.h>
#include <FCDocument/FCDGeometryPolygonsInput.h>
#include <FCDocument/FCDGeometryPolygonsTools.h>
#include <FCDocument/FCDGeometrySource.h>
#include <FCDocument/FCDController.h>
#include <FCDocument/FCDControllerInstance.h>
#include <FCDocument/FCDSkinController.h>
#include <FCDocument/FCDTexture.h>
#include <FCDocument/FCDTransform.h>
#include <FUtils/FUFileManager.h>
#include <FUtils/FUUri.h>
#include <FUtils/FUXmlParser.h>

#ifndef OS_WIN
#include <vector>
using std::min;
using std::max;
#endif

#include <sstream>

#endif  // __cplusplus

#endif  // O3D_IMPORT_CROSS_PRECOMPILE_H_
