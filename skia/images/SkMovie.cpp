#include "SkMovie.h"
#include "SkCanvas.h"
#include "SkPaint.h"

// We should never see this in normal operation since our time values are
// 0-based. So we use it as a sentinal.
#define UNINITIALIZED_MSEC ((SkMSec)-1)

SkMovie::SkMovie()
{
    fInfo.fDuration = UNINITIALIZED_MSEC;  // uninitialized
    fCurrTime = UNINITIALIZED_MSEC; // uninitialized
    fNeedBitmap = true;
}

void SkMovie::ensureInfo()
{
    if (fInfo.fDuration == UNINITIALIZED_MSEC && !this->onGetInfo(&fInfo))
        memset(&fInfo, 0, sizeof(fInfo));   // failure
}

SkMSec SkMovie::duration()
{
    this->ensureInfo();
    return fInfo.fDuration;
}

int SkMovie::width()
{
    this->ensureInfo();
    return fInfo.fWidth;
}

int SkMovie::height()
{
    this->ensureInfo();
    return fInfo.fHeight;
}

int SkMovie::isOpaque()
{
    this->ensureInfo();
    return fInfo.fIsOpaque;
}

bool SkMovie::setTime(SkMSec time)
{
    SkMSec dur = this->duration();
    if (time > dur)
        time = dur;
        
    bool changed = false;
    if (time != fCurrTime)
    {
        fCurrTime = time;
        changed = this->onSetTime(time);
        fNeedBitmap |= changed;
    }
    return changed;
}

const SkBitmap& SkMovie::bitmap()
{
    if (fCurrTime == UNINITIALIZED_MSEC)    // uninitialized
        this->setTime(0);

    if (fNeedBitmap)
    {
        if (!this->onGetBitmap(&fBitmap))   // failure
            fBitmap.reset();
        fNeedBitmap = false;
    }
    return fBitmap;
}

////////////////////////////////////////////////////////////////////

#include "SkStream.h"

SkMovie* SkMovie::DecodeFile(const char path[])
{
    SkMovie* movie = NULL;

    // since the movie might hold onto the stream
    // we dynamically allocate it and then unref()
    SkFILEStream* stream = new SkFILEStream(path);
    if (stream->isValid())
        movie = SkMovie::DecodeStream(stream);
#ifdef SK_DEBUG
    else
        SkDebugf("Movie file not found <%s>\n", path);
#endif
    stream->unref();

    return movie;
}

