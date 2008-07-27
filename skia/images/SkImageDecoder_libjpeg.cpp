/*
 * Copyright 2007, Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); 
 * you may not use this file except in compliance with the License. 
 * You may obtain a copy of the License at 
 *
 *     http://www.apache.org/licenses/LICENSE-2.0 
 *
 * Unless required by applicable law or agreed to in writing, software 
 * distributed under the License is distributed on an "AS IS" BASIS, 
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
 * See the License for the specific language governing permissions and 
 * limitations under the License.
 */

#include "SkImageDecoder.h"
#include "SkColorPriv.h"
#include "SkDither.h"
#include "SkScaledBitmapSampler.h"
#include "SkStream.h"
#include "SkTemplates.h"
#include "SkUtils.h"

#include <stdio.h>
extern "C" {
    #include "jpeglib.h"
}

// this enables timing code to report milliseconds for an encode
//#define TIME_ENCODE
//#define TIME_DECODE

// this enables our rgb->yuv code, which is faster than libjpeg on ARM
// disable for the moment, as we have some glitches when width != multiple of 4
#define WE_CONVERT_TO_YUV

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SkJPEGImageDecoder : public SkImageDecoder {
public:
    virtual Format getFormat() const {
        return kJPEG_Format;
    }

protected:
    virtual bool onDecode(SkStream* stream, SkBitmap* bm,
                          SkBitmap::Config pref, Mode);
};

SkImageDecoder* SkImageDecoder_JPEG_Factory(SkStream* stream) { 
    // !!! unimplemented; rely on PNG test first for now
    return SkNEW(SkJPEGImageDecoder);
}

//////////////////////////////////////////////////////////////////////////

#include "SkTime.h"

class AutoTimeMillis {
public:
    AutoTimeMillis(const char label[]) : fLabel(label) {
        if (!fLabel) {
            fLabel = "";
        }
        fNow = SkTime::GetMSecs();
    }
    ~AutoTimeMillis() {
        SkDebugf("---- Time (ms): %s %d\n", fLabel, SkTime::GetMSecs() - fNow);
    }
private:
    const char* fLabel;
    SkMSec      fNow;
};

/* our source struct for directing jpeg to our stream object
*/
struct sk_source_mgr : jpeg_source_mgr {
    sk_source_mgr(SkStream* stream);

    SkStream*   fStream;

    enum {
        kBufferSize = 1024
    };
    char    fBuffer[kBufferSize];
};

/* Automatically clean up after throwing an exception */
class JPEGAutoClean {
public:
    JPEGAutoClean(): cinfo_ptr(NULL) {}
    ~JPEGAutoClean() {
        if (cinfo_ptr) {
            jpeg_destroy_decompress(cinfo_ptr);
        }
    }
    void set(jpeg_decompress_struct* info) {
        cinfo_ptr = info;
    }
private:
    jpeg_decompress_struct* cinfo_ptr;
};

static void sk_init_source(j_decompress_ptr cinfo) {
    sk_source_mgr*  src = (sk_source_mgr*)cinfo->src;
    src->next_input_byte = (const JOCTET*)src->fBuffer;
    src->bytes_in_buffer = 0;
}

static boolean sk_fill_input_buffer(j_decompress_ptr cinfo) {
    sk_source_mgr* src = (sk_source_mgr*)cinfo->src;
    size_t bytes = src->fStream->read(src->fBuffer, sk_source_mgr::kBufferSize);
    // note that JPEG is happy with less than the full read,
    // as long as the result is non-zero
    if (bytes == 0) {
        cinfo->err->error_exit((j_common_ptr)cinfo);
        return FALSE;
    }

    src->next_input_byte = (const JOCTET*)src->fBuffer;
    src->bytes_in_buffer = bytes;
    return TRUE;
}

static void sk_skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
    SkASSERT(num_bytes > 0);

    sk_source_mgr*  src = (sk_source_mgr*)cinfo->src;

    long skip = num_bytes - src->bytes_in_buffer;

    if (skip > 0) {
        size_t bytes = src->fStream->read(NULL, skip);
        if (bytes != (size_t)skip) {
            cinfo->err->error_exit((j_common_ptr)cinfo);
        }
        src->next_input_byte = (const JOCTET*)src->fBuffer;
        src->bytes_in_buffer = 0;
    } else {
        src->next_input_byte += num_bytes;
        src->bytes_in_buffer -= num_bytes;
    }
}

static boolean sk_resync_to_restart(j_decompress_ptr cinfo, int desired) {
    sk_source_mgr*  src = (sk_source_mgr*)cinfo->src;

    // what is the desired param for???

    if (!src->fStream->rewind()) {
        printf("------------- sk_resync_to_restart: stream->rewind() failed\n");
        cinfo->err->error_exit((j_common_ptr)cinfo);
        return FALSE;
    }
    return TRUE;
}

static void sk_term_source(j_decompress_ptr /*cinfo*/) {}

sk_source_mgr::sk_source_mgr(SkStream* stream)
        : fStream(stream) {
    init_source = sk_init_source;
    fill_input_buffer = sk_fill_input_buffer;
    skip_input_data = sk_skip_input_data;
    resync_to_restart = sk_resync_to_restart;
    term_source = sk_term_source;
}

#include <setjmp.h>

struct sk_error_mgr : jpeg_error_mgr {
    jmp_buf fJmpBuf;
};

static void sk_error_exit(j_common_ptr cinfo) {
    sk_error_mgr* error = (sk_error_mgr*)cinfo->err;

    (*error->output_message) (cinfo);

    /* Let the memory manager delete any temp files before we die */
    jpeg_destroy(cinfo);

    longjmp(error->fJmpBuf, -1);
}

///////////////////////////////////////////////////////////////////////////////

static void skip_src_rows(jpeg_decompress_struct* cinfo, void* buffer,
                          int count) {
    for (int i = 0; i < count; i++) {
        JSAMPLE* rowptr = (JSAMPLE*)buffer;
        int row_count = jpeg_read_scanlines(cinfo, &rowptr, 1);
        SkASSERT(row_count == 1);
    }
}    

bool SkJPEGImageDecoder::onDecode(SkStream* stream, SkBitmap* bm,
                                  SkBitmap::Config prefConfig, Mode mode) {
#ifdef TIME_DECODE
    AutoTimeMillis atm("JPEG Decode");
#endif

    SkAutoMalloc  srcStorage;
    JPEGAutoClean autoClean;

    jpeg_decompress_struct  cinfo;
    sk_error_mgr            sk_err;
    sk_source_mgr           sk_stream(stream);

    cinfo.err = jpeg_std_error(&sk_err);
    sk_err.error_exit = sk_error_exit;

    // All objects need to be instantiated before this setjmp call so that
    // they will be cleaned up properly if an error occurs.
    if (setjmp(sk_err.fJmpBuf)) {
        return false;
    }

    jpeg_create_decompress(&cinfo);
    autoClean.set(&cinfo);

    //jpeg_stdio_src(&cinfo, file);
    cinfo.src = &sk_stream;

    jpeg_read_header(&cinfo, true);

    /*  Try to fulfill the requested sampleSize. Since jpeg can do it (when it
        can) much faster that we, just use their num/denom api to approximate
        the size.
    */
    int sampleSize = this->getSampleSize();

    cinfo.dct_method = JDCT_IFAST;
    cinfo.scale_num = 1;
    cinfo.scale_denom = sampleSize;

    /*  image_width and image_height are the original dimensions, available
        after jpeg_read_header(). To see the scaled dimensions, we have to call
        jpeg_start_decompress(), and then read output_width and output_height.
    */
    jpeg_start_decompress(&cinfo);

    /*  If we need to better match the request, we might examine the image and
        output dimensions, and determine if the downsampling jpeg provided is
        not sufficient. If so, we can recompute a modified sampleSize value to
        make up the difference.
        
        To skip this additional scaling, just set sampleSize = 1; below.
    */
    sampleSize = sampleSize * cinfo.output_width / cinfo.image_width;

    // check for supported formats
    bool isRGB; // as opposed to gray8
    if (3 == cinfo.num_components && JCS_RGB == cinfo.out_color_space) {
        isRGB = true;
    } else if (1 == cinfo.num_components &&
               JCS_GRAYSCALE == cinfo.out_color_space) {
        isRGB = false;  // could use Index8 config if we want...
    } else {
        SkDEBUGF(("SkJPEGImageDecoder: unsupported jpeg colorspace %d with %d components\n",
                    cinfo.jpeg_color_space, cinfo.num_components));
        return false;
    }
    
    SkBitmap::Config config = prefConfig;
    // if no user preference, see what the device recommends
    if (config == SkBitmap::kNo_Config)
        config = SkImageDecoder::GetDeviceConfig();

    // only these make sense for jpegs
    if (config != SkBitmap::kARGB_8888_Config &&
            config != SkBitmap::kARGB_4444_Config &&
            config != SkBitmap::kRGB_565_Config) {
        config = SkBitmap::kARGB_8888_Config;
    }

    // should we allow the Chooser (if present) to pick a config for us???
    if (!this->chooseFromOneChoice(config, cinfo.output_width,
                                   cinfo.output_height)) {
        return false;
    }

    SkScaledBitmapSampler sampler(cinfo.output_width, cinfo.output_height,
                                  sampleSize);
    
    bm->setConfig(config, sampler.scaledWidth(), sampler.scaledHeight());
    // jpegs are always opauqe (i.e. have no per-pixel alpha)
    bm->setIsOpaque(true);

    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        return true;
    }
    if (!this->allocPixelRef(bm, NULL)) {
        return false;
    }

    SkAutoLockPixels alp(*bm);
    
    if (!sampler.begin(bm,
                      isRGB ? SkScaledBitmapSampler::kRGB :
                              SkScaledBitmapSampler::kGray,
                      this->getDitherImage())) {
        return false;
    }

    uint8_t* srcRow = (uint8_t*)srcStorage.alloc(cinfo.output_width * 3);

    skip_src_rows(&cinfo, srcRow, sampler.srcY0());
    for (int y = 0;; y++) {
        JSAMPLE* rowptr = (JSAMPLE*)srcRow;
        int row_count = jpeg_read_scanlines(&cinfo, &rowptr, 1);
        SkASSERT(row_count == 1);
        
        sampler.next(srcRow);
        if (bm->height() - 1 == y) {
            break;
        }
        skip_src_rows(&cinfo, srcRow, sampler.srcDY() - 1);
    }

    // ??? If I don't do this, I get an error from finish_decompress
    skip_src_rows(&cinfo, srcRow, cinfo.output_height - cinfo.output_scanline);
        
    jpeg_finish_decompress(&cinfo);

    return true;
}

///////////////////////////////////////////////////////////////////////////////

#ifdef SK_SUPPORT_IMAGE_ENCODE

#include "SkColorPriv.h"

// taken from jcolor.c in libjpeg
#if 0   // 16bit - precise but slow
    #define CYR     19595   // 0.299
    #define CYG     38470   // 0.587
    #define CYB      7471   // 0.114

    #define CUR    -11059   // -0.16874
    #define CUG    -21709   // -0.33126
    #define CUB     32768   // 0.5

    #define CVR     32768   // 0.5
    #define CVG    -27439   // -0.41869
    #define CVB     -5329   // -0.08131

    #define CSHIFT  16
#else      // 8bit - fast, slightly less precise
    #define CYR     77    // 0.299
    #define CYG     150    // 0.587
    #define CYB      29    // 0.114

    #define CUR     -43    // -0.16874
    #define CUG    -85    // -0.33126
    #define CUB     128    // 0.5

    #define CVR      128   // 0.5
    #define CVG     -107   // -0.41869
    #define CVB      -21   // -0.08131

    #define CSHIFT  8
#endif

static void rgb2yuv_32(uint8_t dst[], SkPMColor c) {
    int r = SkGetPackedR32(c);
    int g = SkGetPackedG32(c);
    int b = SkGetPackedB32(c);

    int  y = ( CYR*r + CYG*g + CYB*b ) >> CSHIFT;
    int  u = ( CUR*r + CUG*g + CUB*b ) >> CSHIFT;
    int  v = ( CVR*r + CVG*g + CVB*b ) >> CSHIFT;

    dst[0] = SkToU8(y);
    dst[1] = SkToU8(u + 128);
    dst[2] = SkToU8(v + 128);
}

static void rgb2yuv_4444(uint8_t dst[], U16CPU c) {
    int r = SkGetPackedR4444(c);
    int g = SkGetPackedG4444(c);
    int b = SkGetPackedB4444(c);
    
    int  y = ( CYR*r + CYG*g + CYB*b ) >> (CSHIFT - 4);
    int  u = ( CUR*r + CUG*g + CUB*b ) >> (CSHIFT - 4);
    int  v = ( CVR*r + CVG*g + CVB*b ) >> (CSHIFT - 4);
    
    dst[0] = SkToU8(y);
    dst[1] = SkToU8(u + 128);
    dst[2] = SkToU8(v + 128);
}

static void rgb2yuv_16(uint8_t dst[], U16CPU c) {
    int r = SkGetPackedR16(c);
    int g = SkGetPackedG16(c);
    int b = SkGetPackedB16(c);
    
    int  y = ( 2*CYR*r + CYG*g + 2*CYB*b ) >> (CSHIFT - 2);
    int  u = ( 2*CUR*r + CUG*g + 2*CUB*b ) >> (CSHIFT - 2);
    int  v = ( 2*CVR*r + CVG*g + 2*CVB*b ) >> (CSHIFT - 2);
    
    dst[0] = SkToU8(y);
    dst[1] = SkToU8(u + 128);
    dst[2] = SkToU8(v + 128);
}

///////////////////////////////////////////////////////////////////////////////

typedef void (*WriteScanline)(uint8_t* SK_RESTRICT dst,
                              const void* SK_RESTRICT src, int width,
                              const SkPMColor* SK_RESTRICT ctable);

static void Write_32_YUV(uint8_t* SK_RESTRICT dst,
                         const void* SK_RESTRICT srcRow, int width,
                         const SkPMColor*) {
    const uint32_t* SK_RESTRICT src = (const uint32_t*)srcRow;
    while (--width >= 0) {
#ifdef WE_CONVERT_TO_YUV
        rgb2yuv_32(dst, *src++);
#else
        uint32_t c = *src++;
        dst[0] = SkGetPackedR32(c);
        dst[1] = SkGetPackedG32(c);
        dst[2] = SkGetPackedB32(c);
#endif
        dst += 3;
    }
}

static void Write_4444_YUV(uint8_t* SK_RESTRICT dst,
                           const void* SK_RESTRICT srcRow, int width,
                           const SkPMColor*) {
    const SkPMColor16* SK_RESTRICT src = (const SkPMColor16*)srcRow;
    while (--width >= 0) {
#ifdef WE_CONVERT_TO_YUV
        rgb2yuv_4444(dst, *src++);
#else
        SkPMColor16 c = *src++;
        dst[0] = SkPacked4444ToR32(c);
        dst[1] = SkPacked4444ToG32(c);
        dst[2] = SkPacked4444ToB32(c);
#endif
        dst += 3;
    }
}

static void Write_16_YUV(uint8_t* SK_RESTRICT dst,
                         const void* SK_RESTRICT srcRow, int width,
                         const SkPMColor*) {
    const uint16_t* SK_RESTRICT src = (const uint16_t*)srcRow;
    while (--width >= 0) {
#ifdef WE_CONVERT_TO_YUV
        rgb2yuv_16(dst, *src++);
#else
        uint16_t c = *src++;
        dst[0] = SkPacked16ToR32(c);
        dst[1] = SkPacked16ToG32(c);
        dst[2] = SkPacked16ToB32(c);
#endif
        dst += 3;
    }
}

static void Write_Index_YUV(uint8_t* SK_RESTRICT dst,
                            const void* SK_RESTRICT srcRow, int width,
                            const SkPMColor* SK_RESTRICT ctable) {
    const uint8_t* SK_RESTRICT src = (const uint8_t*)srcRow;
    while (--width >= 0) {
#ifdef WE_CONVERT_TO_YUV
        rgb2yuv_32(dst, ctable[*src++]);
#else
        uint32_t c = ctable[*src++];
        dst[0] = SkGetPackedR32(c);
        dst[1] = SkGetPackedG32(c);
        dst[2] = SkGetPackedB32(c);
#endif
        dst += 3;
    }
}

static WriteScanline ChooseWriter(const SkBitmap& bm) {
    switch (bm.config()) {
        case SkBitmap::kARGB_8888_Config:
            return Write_32_YUV;
        case SkBitmap::kRGB_565_Config:
            return Write_16_YUV;
        case SkBitmap::kARGB_4444_Config:
            return Write_4444_YUV;
        case SkBitmap::kIndex8_Config:
            return Write_Index_YUV;
        default:
            return NULL;
    }
}

struct sk_destination_mgr : jpeg_destination_mgr {
    sk_destination_mgr(SkWStream* stream);

    SkWStream*  fStream;

    enum {
        kBufferSize = 1024
    };
    uint8_t fBuffer[kBufferSize];
};

static void sk_init_destination(j_compress_ptr cinfo) {
    sk_destination_mgr* dest = (sk_destination_mgr*)cinfo->dest;

    dest->next_output_byte = dest->fBuffer;
    dest->free_in_buffer = sk_destination_mgr::kBufferSize;
}

static boolean sk_empty_output_buffer(j_compress_ptr cinfo) {
    sk_destination_mgr* dest = (sk_destination_mgr*)cinfo->dest;

//  if (!dest->fStream->write(dest->fBuffer, sk_destination_mgr::kBufferSize - dest->free_in_buffer))
    if (!dest->fStream->write(dest->fBuffer, sk_destination_mgr::kBufferSize)) {
        sk_throw();
    }
    //  ERREXIT(cinfo, JERR_FILE_WRITE);

    dest->next_output_byte = dest->fBuffer;
    dest->free_in_buffer = sk_destination_mgr::kBufferSize;
    return TRUE;
}

static void sk_term_destination (j_compress_ptr cinfo) {
    sk_destination_mgr* dest = (sk_destination_mgr*)cinfo->dest;

    size_t size = sk_destination_mgr::kBufferSize - dest->free_in_buffer;
    if (size > 0) {
        if (!dest->fStream->write(dest->fBuffer, size)) {
            sk_throw();
        }
    }
    dest->fStream->flush();
}

sk_destination_mgr::sk_destination_mgr(SkWStream* stream)
        : fStream(stream) {
    this->init_destination = sk_init_destination;
    this->empty_output_buffer = sk_empty_output_buffer;
    this->term_destination = sk_term_destination;
}

class SkAutoLockColors : public SkNoncopyable {
public:
    SkAutoLockColors(const SkBitmap& bm) {
        fCTable = bm.getColorTable();
        fColors = fCTable ? fCTable->lockColors() : NULL;
    }
    ~SkAutoLockColors() {
        if (fCTable) {
            fCTable->unlockColors(false);
        }
    }
    const SkPMColor* colors() const { return fColors; }
private:
    SkColorTable*    fCTable;
    const SkPMColor* fColors;
};

class SkJPEGImageEncoder : public SkImageEncoder {
protected:
    virtual bool onEncode(SkWStream* stream, const SkBitmap& bm, int quality) {
#ifdef TIME_ENCODE
        AutoTimeMillis atm("JPEG Encode");
#endif

        const WriteScanline writer = ChooseWriter(bm);
        if (NULL == writer) {
            return false;
        }

        SkAutoLockPixels alp(bm);
        if (NULL == bm.getPixels()) {
            return false;
        }

        jpeg_compress_struct    cinfo;
        sk_error_mgr            sk_err;
        sk_destination_mgr      sk_wstream(stream);

        cinfo.err = jpeg_std_error(&sk_err);
        sk_err.error_exit = sk_error_exit;
        if (setjmp(sk_err.fJmpBuf)) {
            return false;
        }
        jpeg_create_compress(&cinfo);

        cinfo.dest = &sk_wstream;
        cinfo.image_width = bm.width();
        cinfo.image_height = bm.height();
        cinfo.input_components = 3;
#ifdef WE_CONVERT_TO_YUV
        cinfo.in_color_space = JCS_YCbCr;
#else
        cinfo.in_color_space = JCS_RGB;
#endif
        cinfo.input_gamma = 1;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);
        cinfo.dct_method = JDCT_IFAST;
        
        jpeg_start_compress(&cinfo, TRUE);

        const int       width = bm.width();
        SkAutoMalloc    oneRow(width * 3);
        uint8_t*        oneRowP = (uint8_t*)oneRow.get();

        SkAutoLockColors alc(bm);
        const SkPMColor* colors = alc.colors();
        const void*      srcRow = bm.getPixels();
        
        while (cinfo.next_scanline < cinfo.image_height) {
            JSAMPROW row_pointer[1];    /* pointer to JSAMPLE row[s] */

            writer(oneRowP, srcRow, width, colors);
            row_pointer[0] = oneRowP;
            (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
            srcRow = (const void*)((const char*)srcRow + bm.rowBytes());
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);

        return true;
    }
};

SkImageEncoder* SkImageEncoder_JPEG_Factory();
SkImageEncoder* SkImageEncoder_JPEG_Factory() {
    return SkNEW(SkJPEGImageEncoder);
}

#endif /* SK_SUPPORT_IMAGE_ENCODE */

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef SK_DEBUG

void SkImageDecoder::UnitTest() {
    SkBitmap    bm;

    (void)SkImageDecoder::DecodeFile("logo.jpg", &bm);
}

#endif
