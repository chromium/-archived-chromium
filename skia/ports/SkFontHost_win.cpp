/* libs/graphics/ports/SkFontHost_win.cpp

**

** Copyright 2006, Google Inc.

**

*/



#include "SkString.h"

//#include "SkStream.h"



#include "SkFontHost.h"

#include "SkDescriptor.h"

#include "SkThread.h"



#ifdef WIN32

#include "windows.h"

#include "tchar.h"



static SkMutex      gFTMutex;



static LOGFONT gDefaultFont;



static const uint16_t BUFFERSIZE = (16384 - 32);

static uint8_t glyphbuf[BUFFERSIZE]; 



#ifndef SK_FONTKEY

    #define SK_FONTKEY "Windows Font Key"

#endif



inline FIXED SkFixedToFIXED(SkFixed x) {

    return *(FIXED*)(&x);

}



class FontFaceRec_Typeface : public SkTypeface {

public:

#if 0

    FontFaceRec_Typeface(const LOGFONT& face) : fFace(face)

    {

        int style = 0;

        if (face.lfWeight == FW_SEMIBOLD || face.lfWeight == FW_DEMIBOLD || face.lfWeight == FW_BOLD)

            style |= SkTypeface::kBold;

        if (face.lfItalic)

            style |= SkTypeface::kItalic;

        this->setStyle((SkTypeface::Style)style); 

    }

#endif

    ~FontFaceRec_Typeface() {};



    TCHAR* GetFontName() { return fFace.lfFaceName; }



    SkTypeface::Style GetFontStyle() {         

        int style = SkTypeface::kNormal;

        if (fFace.lfWeight == FW_SEMIBOLD || fFace.lfWeight == FW_DEMIBOLD || fFace.lfWeight == FW_BOLD)

            style |= SkTypeface::kBold;

        if (fFace.lfItalic)

            style |= SkTypeface::kItalic;



        return (SkTypeface::Style)style;

    }



    long GetFontSize() { return fFace.lfHeight; }



    LOGFONT fFace;

};





static const LOGFONT* get_default_font()

{

    // don't hardcode on Windows, Win2000, XP, Vista, and international all have different default

    // and the user could change too



    NONCLIENTMETRICS ncm;

    ncm.cbSize = sizeof(NONCLIENTMETRICS);

    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);



    memcpy(&gDefaultFont, &(ncm.lfMessageFont), sizeof(LOGFONT));



    return &gDefaultFont;

}



static uint32_t FontFaceChecksum(const LOGFONT& face)

{

    uint32_t cs = 0;

    uint32_t bytesize = sizeof(LOGFONT);

    bytesize >>= 2;

    uint32_t *p32 = (uint32_t*)&face;



    while (bytesize) {

        bytesize --;

        cs ^= *p32;

        p32 ++;

    }



    return cs;

}



class SkScalerContext_Windows : public SkScalerContext {

public:

    SkScalerContext_Windows(const SkDescriptor* desc);

    virtual ~SkScalerContext_Windows();



protected:

    virtual unsigned generateGlyphCount() const;

    virtual uint16_t generateCharToGlyph(SkUnichar uni);

    virtual void generateMetrics(SkGlyph* glyph);

    virtual void generateImage(const SkGlyph& glyph);

    virtual void generatePath(const SkGlyph& glyph, SkPath* path);

    virtual void generateLineHeight(SkPoint* ascent, SkPoint* descent);



private:

    LOGFONT*    plf;              

    MAT2        mat22;

};



SkScalerContext_Windows::SkScalerContext_Windows(const SkDescriptor* desc)

    : SkScalerContext(desc), plf(NULL)

{

    SkAutoMutexAcquire  ac(gFTMutex);



    const LOGFONT** face = (const LOGFONT**)desc->findEntry(kTypeface_SkDescriptorTag, NULL);

    plf = (LOGFONT*)*face;

    SkASSERT(plf);

  

    mat22.eM11 = SkFixedToFIXED(fRec.fPost2x2[0][0]);    

    mat22.eM12 = SkFixedToFIXED(-fRec.fPost2x2[0][1]);

    mat22.eM21 = SkFixedToFIXED(fRec.fPost2x2[1][0]);

    mat22.eM22 = SkFixedToFIXED(-fRec.fPost2x2[1][1]);

}



SkScalerContext_Windows::~SkScalerContext_Windows() {

}



unsigned SkScalerContext_Windows::generateGlyphCount() const {

    return 0xFFFF;

//    return fFace->num_glyphs;

}



uint16_t SkScalerContext_Windows::generateCharToGlyph(SkUnichar uni) {



    // let's just use the uni as index on Windows

    return SkToU16(uni);

}



void SkScalerContext_Windows::generateMetrics(SkGlyph* glyph) {



    HDC ddc = ::CreateCompatibleDC(NULL);

    SetBkMode(ddc, TRANSPARENT);



    SkASSERT(plf);

    plf->lfHeight = -SkFixedFloor(fRec.fTextSize);



    HFONT font = CreateFontIndirect(plf);

    HFONT oldfont = (HFONT)SelectObject(ddc, font);

    

    GLYPHMETRICS gm;

    memset(&gm, 0, sizeof(gm));



    glyph->fRsbDelta = 0;

    glyph->fLsbDelta = 0;



    // Note: need to use GGO_GRAY8_BITMAP instead of GGO_METRICS because GGO_METRICS returns a smaller

    // BlackBlox; we need the bigger one in case we need the image.  fAdvance is the same.

    uint32_t ret = GetGlyphOutlineW(ddc, glyph->f_GlyphID, GGO_GRAY8_BITMAP, &gm, 0, NULL, &mat22);

    

    if (GDI_ERROR != ret) {

        if (ret == 0) {

            // for white space, ret is zero and gmBlackBoxX, gmBlackBoxY are 1 incorrectly!

            gm.gmBlackBoxX = gm.gmBlackBoxY = 0;

        }

        glyph->fWidth   = gm.gmBlackBoxX;

        glyph->fHeight  = gm.gmBlackBoxY;

        glyph->fTop     = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;

        glyph->fLeft    = gm.gmptGlyphOrigin.x;

        glyph->fAdvanceX = SkIntToFixed(gm.gmCellIncX);

        glyph->fAdvanceY = -SkIntToFixed(gm.gmCellIncY);

    }



    ::SelectObject(ddc, oldfont);

    ::DeleteObject(font);

    ::DeleteDC(ddc);

}



void SkScalerContext_Windows::generateImage(const SkGlyph& glyph) {



    SkAutoMutexAcquire  ac(gFTMutex);



    SkASSERT(plf);



    HDC ddc = ::CreateCompatibleDC(NULL);

    SetBkMode(ddc, TRANSPARENT);



    plf->lfHeight = -SkFixedFloor(fRec.fTextSize);



    HFONT font = CreateFontIndirect(plf);

    HFONT oldfont = (HFONT)SelectObject(ddc, font);



    GLYPHMETRICS gm;

    memset(&gm, 0, sizeof(gm));



    uint32_t total_size = GetGlyphOutlineW(ddc, glyph.f_GlyphID, GGO_GRAY8_BITMAP, &gm, 0, NULL, &mat22);

    if (GDI_ERROR != total_size && total_size > 0) {

        uint8_t *pBuff = new uint8_t[total_size];

        if (NULL != pBuff) {

            total_size = GetGlyphOutlineW(ddc, glyph.f_GlyphID, GGO_GRAY8_BITMAP, &gm, total_size, pBuff, &mat22);

    

            SkASSERT(total_size != GDI_ERROR);



            uint8_t* dst = (uint8_t*)glyph.fImage;

            uint32_t pitch = (gm.gmBlackBoxX + 3) & ~0x3;



            for (int32_t y = gm.gmBlackBoxY - 1; y >= 0; y--) {

                uint8_t* src = pBuff + pitch * y;



                for (uint32_t x = 0; x < gm.gmBlackBoxX; x++) {

                    if (*src > 63) {

                        *dst = 0xFF;

                    }

                    else {

                        *dst = *src << 2; // scale to 0-255

                    }

                    dst++;

                    src++;

                }

            }



            delete[] pBuff;

        }

    }

            

    SkASSERT(GDI_ERROR != total_size && total_size >= 0);



    ::SelectObject(ddc, oldfont);

    ::DeleteObject(font);

    ::DeleteDC(ddc);

}



void SkScalerContext_Windows::generatePath(const SkGlyph& glyph, SkPath* path) {



    SkAutoMutexAcquire  ac(gFTMutex);



    SkASSERT(&glyph && path);



    SkASSERT(plf);



    path->reset();



    HDC ddc = ::CreateCompatibleDC(NULL);

    SetBkMode(ddc, TRANSPARENT);



    plf->lfHeight = -SkFixedFloor(fRec.fTextSize);



    HFONT font = CreateFontIndirect(plf);

    HFONT oldfont = (HFONT)SelectObject(ddc, font);



    GLYPHMETRICS gm;



    uint32_t total_size = GetGlyphOutlineW(ddc, glyph.f_GlyphID, GGO_NATIVE, &gm, BUFFERSIZE, glyphbuf, &mat22);



    if (GDI_ERROR != total_size) {

    

        const uint8_t* cur_glyph = glyphbuf;

        const uint8_t* end_glyph = glyphbuf + total_size;

    

        while(cur_glyph < end_glyph) {

            const TTPOLYGONHEADER* th = (TTPOLYGONHEADER*)cur_glyph;

        

            const uint8_t* end_poly = cur_glyph + th->cb;

            const uint8_t* cur_poly = cur_glyph + sizeof(TTPOLYGONHEADER);

        

            path->moveTo(*(SkFixed*)(&th->pfxStart.x), *(SkFixed*)(&th->pfxStart.y));

        

            while(cur_poly < end_poly) {

                const TTPOLYCURVE* pc = (const TTPOLYCURVE*)cur_poly;

            

                if (pc->wType == TT_PRIM_LINE) {

                    for (uint16_t i = 0; i < pc->cpfx; i++) {

                        path->lineTo(*(SkFixed*)(&pc->apfx[i].x), *(SkFixed*)(&pc->apfx[i].y));

                    }

                }

            

                if (pc->wType == TT_PRIM_QSPLINE) {

                    for (uint16_t u = 0; u < pc->cpfx - 1; u++) { // Walk through points in spline

                        POINTFX pnt_b = pc->apfx[u];    // B is always the current point

                        POINTFX pnt_c = pc->apfx[u+1];

                    

                        if (u < pc->cpfx - 2) {          // If not on last spline, compute C                            

                            pnt_c.x = SkFixedToFIXED(SkFixedAve(*(SkFixed*)(&pnt_b.x), *(SkFixed*)(&pnt_c.x)));

                            pnt_c.y = SkFixedToFIXED(SkFixedAve(*(SkFixed*)(&pnt_b.y), *(SkFixed*)(&pnt_c.y)));

                        }



                        path->quadTo(*(SkFixed*)(&pnt_b.x), *(SkFixed*)(&pnt_b.y), *(SkFixed*)(&pnt_c.x), *(SkFixed*)(&pnt_c.y));

                    }

                }

                cur_poly += sizeof(uint16_t) * 2 + sizeof(POINTFX) * pc->cpfx;

            }

            cur_glyph += th->cb;

        }

    }

    else {

        SkASSERT(false);

    }



    path->close();



    ::SelectObject(ddc, oldfont);

    ::DeleteObject(font);

    ::DeleteDC(ddc);

}





// Note:  not sure this is the correct implementation

void SkScalerContext_Windows::generateLineHeight(SkPoint* ascent, SkPoint* descent) {



    HDC ddc = ::CreateCompatibleDC(NULL);

    SetBkMode(ddc, TRANSPARENT);



    SkASSERT(plf);

    plf->lfHeight = -SkFixedFloor(fRec.fTextSize);



    HFONT font = CreateFontIndirect(plf);

    HFONT oldfont = (HFONT)SelectObject(ddc, font);



    OUTLINETEXTMETRIC otm;



    uint32_t ret = GetOutlineTextMetrics(ddc, sizeof(otm), &otm);



    if (sizeof(otm) == ret) {

        if (ascent)

            ascent->iset(0, otm.otmAscent);

        if (descent)

            descent->iset(0, otm.otmDescent);

    }



    ::SelectObject(ddc, oldfont);

    ::DeleteObject(font);

    ::DeleteDC(ddc);



    return;

}



SkTypeface* SkFontHost::CreateTypeface( const SkTypeface* familyFace, const char familyName[], SkTypeface::Style style) {



    FontFaceRec_Typeface* ptypeface = new FontFaceRec_Typeface;



    if (NULL == ptypeface) {

        SkASSERT(false);

        return NULL;

    }



    memset(&ptypeface->fFace, 0, sizeof(LOGFONT));



    // default

    ptypeface->fFace.lfHeight = -11; // default

    ptypeface->fFace.lfWeight = (style & SkTypeface::kBold) != 0 ? FW_BOLD : FW_NORMAL ; 

    ptypeface->fFace.lfItalic = ((style & SkTypeface::kItalic) != 0);

    ptypeface->fFace.lfQuality = PROOF_QUALITY;



    _tcscpy(ptypeface->fFace.lfFaceName, familyName);





    return ptypeface;

}



uint32_t SkFontHost::FlattenTypeface(const SkTypeface* tface, void* buffer) {

    const LOGFONT* face;

    

    if (tface)

        face = &((const FontFaceRec_Typeface*)tface)->fFace;

    else

       face = get_default_font();



    size_t size = sizeof(face);



    size += sizeof(uint32_t);



    if (buffer) {

        uint8_t* buf = (uint8_t*)buffer;

        memcpy(buf, &face, sizeof(face));

        uint32_t cs = FontFaceChecksum(*face);



        memcpy(buf+sizeof(face), &cs, sizeof(cs));

    }



    return size;

}



SkScalerContext* SkFontHost::CreateScalerContext(const SkDescriptor* desc) {

    return SkNEW_ARGS(SkScalerContext_Windows, (desc));

}



void SkFontHost::GetDescriptorKeyString(const SkDescriptor* desc, SkString* keyString) {

    const LOGFONT** face = (const LOGFONT**)desc->findEntry(kTypeface_SkDescriptorTag, NULL);

    LOGFONT*lf = (LOGFONT*)*face;

    keyString->set(SK_FONTKEY);

    if (lf) {

        keyString->append(lf->lfFaceName);

    }

}



SkScalerContext* SkFontHost::CreateFallbackScalerContext(const SkScalerContext::Rec& rec) {

    const LOGFONT* face = get_default_font();



    SkAutoDescriptor    ad(sizeof(rec) + sizeof(face) + SkDescriptor::ComputeOverhead(2));

    SkDescriptor*       desc = ad.getDesc();



    desc->init();

    desc->addEntry(kRec_SkDescriptorTag, sizeof(rec), &rec);

    desc->addEntry(kTypeface_SkDescriptorTag, sizeof(face), &face);

    desc->computeChecksum();



    return SkFontHost::CreateScalerContext(desc);

}



SkStream* SkFontHost::OpenDescriptorStream(const SkDescriptor* desc, const char keyString[]) {

    SkASSERT(!"SkFontHost::OpenDescriptorStream unimplemented");

    return NULL;

}



uint32_t SkFontHost::TypefaceHash(const SkTypeface* face) {

    

//    FontFaceRec_Typeface *ptypeface = dynamic_cast<FontFaceRec_Typeface*>(face); 

    FontFaceRec_Typeface *ptypeface = (FontFaceRec_Typeface*)(face); 

    SkASSERT(ptypeface);



    return FontFaceChecksum(ptypeface->fFace);

}



bool SkFontHost::TypefaceEqual(const SkTypeface* facea, const SkTypeface* faceb) {

    

    FontFaceRec_Typeface *ptypefaceA = (FontFaceRec_Typeface*)facea; 

    SkASSERT(ptypefaceA);



    FontFaceRec_Typeface *ptypefaceB = (FontFaceRec_Typeface*)faceb; 

    SkASSERT(ptypefaceB);



    if (_tcscmp(ptypefaceA->GetFontName(), ptypefaceB->GetFontName())) return false;

    if (ptypefaceA->GetFontStyle() != ptypefaceB->GetFontStyle()) return false;

    if (ptypefaceA->GetFontSize() != ptypefaceB->GetFontSize()) return false;



    return true;

}



int SkFontHost::ComputeGammaFlag(const SkPaint& paint)

{

    return 0;

}



void SkFontHost::GetGammaTables(const uint8_t* tables[2])

{

    tables[0] = NULL;   // black gamma (e.g. exp=1.4)

    tables[1] = NULL;   // white gamma (e.g. exp= 1/1.4)

}



#endif // WIN32



