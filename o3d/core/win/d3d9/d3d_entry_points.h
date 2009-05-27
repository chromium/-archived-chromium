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


#ifndef O3D_CORE_WIN_D3D9_D3D_ENTRY_POINTS_H_
#define O3D_CORE_WIN_D3D9_D3D_ENTRY_POINTS_H_

#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>

namespace o3d {
typedef HRESULT (__stdcall *D3DXCreateEffect_Ptr)(
    LPDIRECT3DDEVICE9,
    LPCVOID,
    UINT,
    CONST D3DXMACRO *,
    LPD3DXINCLUDE,
    DWORD,
    LPD3DXEFFECTPOOL,
    LPD3DXEFFECT *,
    LPD3DXBUFFER *);

typedef HRESULT (__stdcall *D3DXGetShaderInputSemantics_Ptr)(
    CONST DWORD *,
    D3DXSEMANTIC *,
    UINT* pCount);

typedef HRESULT (__stdcall *D3DXCreateEffectCompilerFromFileA_Ptr)(
    LPCSTR,
    CONST D3DXMACRO*,
    LPD3DXINCLUDE,
    DWORD,
    LPD3DXEFFECTCOMPILER*,
    LPD3DXBUFFER*);

typedef HRESULT (__stdcall *D3DXCreateEffectCompilerFromFileW_Ptr)(
    LPCWSTR,
    CONST D3DXMACRO*,
    LPD3DXINCLUDE,
    DWORD,
    LPD3DXEFFECTCOMPILER*,
    LPD3DXBUFFER*);

typedef HRESULT (__stdcall *D3DXSaveSurfaceToFileA_Ptr)(
    LPCSTR,
    D3DXIMAGE_FILEFORMAT,
    LPDIRECT3DSURFACE9,
    CONST PALETTEENTRY*,
    CONST RECT*);

typedef HRESULT (__stdcall *D3DXSaveSurfaceToFileW_Ptr)(
    LPCWSTR,
    D3DXIMAGE_FILEFORMAT,
    LPDIRECT3DSURFACE9,
    CONST PALETTEENTRY*,
    CONST RECT*);

typedef HRESULT (__stdcall *D3DXGetShaderConstantTable_Ptr)(
    CONST DWORD*,
    LPD3DXCONSTANTTABLE*);

typedef HRESULT (__stdcall *D3DXCreateFontW_Ptr)(
    LPDIRECT3DDEVICE9 pDevice,
    INT Height,
    UINT Width,
    UINT Weight,
    UINT MipLevels,
    BOOL Italic,
    DWORD CharSet,
    DWORD OutputPrecision,
    DWORD Quality,
    DWORD PitchAndFamily,
    LPCTSTR pFacename,
    LPD3DXFONT * ppFont);

typedef HRESULT (__stdcall *D3DXCreateFontA_Ptr)(
    LPDIRECT3DDEVICE9 pDevice,
    INT Height,
    UINT Width,
    UINT Weight,
    UINT MipLevels,
    BOOL Italic,
    DWORD CharSet,
    DWORD OutputPrecision,
    DWORD Quality,
    DWORD PitchAndFamily,
    LPCTSTR pFacename,
    LPD3DXFONT * ppFont);

typedef HRESULT (__stdcall *D3DXCreateLine_Ptr)(
    LPDIRECT3DDEVICE9 pDevice,
    LPD3DXLINE * ppLine);

typedef IDirect3D9* (__stdcall *Direct3DCreate9_Ptr)(
    UINT SDKVersion);

extern D3DXCreateEffect_Ptr D3DXCreateEffect;
extern D3DXGetShaderInputSemantics_Ptr D3DXGetShaderInputSemantics;
extern D3DXCreateEffectCompilerFromFileW_Ptr D3DXCreateEffectCompilerFromFileW;
extern D3DXCreateEffectCompilerFromFileA_Ptr D3DXCreateEffectCompilerFromFileA;
extern D3DXSaveSurfaceToFileW_Ptr D3DXSaveSurfaceToFileW;
extern D3DXSaveSurfaceToFileA_Ptr D3DXSaveSurfaceToFileA;
extern D3DXGetShaderConstantTable_Ptr D3DXGetShaderConstantTable;
extern D3DXCreateFontW_Ptr D3DXCreateFontW;
extern D3DXCreateFontA_Ptr D3DXCreateFontA;
extern D3DXCreateLine_Ptr D3DXCreateLine;
extern Direct3DCreate9_Ptr Direct3DCreate9Software;

}  // namespace o3d

#endif  // O3D_CORE_WIN_D3D9_D3D_ENTRY_POINTS_H_
