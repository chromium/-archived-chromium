#ifndef SkDrawFilter_DEFINED
#define SkDrawFilter_DEFINED

#include "SkRefCnt.h"

////////////////// EXPERIMENTAL <reed> //////////////////////////

class SkCanvas;
class SkPaint;

/** Right before something is being draw, filter() is called with the
    current canvas and paint. If it returns true, then drawing proceeds
    with the (possibly modified) canvas/paint, and then restore() is called
    to restore the canvas/paint to their state before filter() was called.
    If filter returns false, canvas/paint should not have been changed, and
    restore() will not be called.
*/
class SkDrawFilter : public SkRefCnt {
public:
    enum Type {
        kPaint_Type,
        kPoint_Type,
        kLine_Type,
        kBitmap_Type,
        kRect_Type,
        kPath_Type,
        kText_Type
    };

    /** Return true to allow the draw to continue (with possibly modified
        canvas/paint). If true is returned, then restore() will be called.
    */
    virtual bool filter(SkCanvas*, SkPaint*, Type) = 0;
    /** If filter() returned true, then restore() will be called to restore the
        canvas/paint to their previous states
    */
    virtual void restore(SkCanvas*, SkPaint*, Type) = 0;
};

#endif
