#ifndef SkPaintFlagsDrawFilter_DEFINED
#define SkPaintFlagsDrawFilter_DEFINED

#include "SkDrawFilter.h"

class SkPaintFlagsDrawFilter : public SkDrawFilter {
public:
    SkPaintFlagsDrawFilter(uint32_t clearFlags, uint32_t setFlags);
    
    // overrides
    virtual bool filter(SkCanvas*, SkPaint*, Type);
    virtual void restore(SkCanvas*, SkPaint*, Type);
    
private:
    uint32_t    fPrevFlags;     // local cache for filter/restore
    uint16_t    fClearFlags;    // user specified
    uint16_t    fSetFlags;      // user specified
};

#endif

