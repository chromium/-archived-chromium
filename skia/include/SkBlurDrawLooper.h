#ifndef SkBlurDrawLooper_DEFINED
#define SkBlurDrawLooper_DEFINED

#include "SkDrawLooper.h"
#include "SkColor.h"

class SkMaskFilter;

/** \class SkBlurDrawLooper
    This class draws a shadow of the object (possibly offset), and then draws
    the original object in its original position.
    <reed> should there be an option to just draw the shadow/blur layer? webkit?
*/
class SkBlurDrawLooper : public SkDrawLooper {
public:
    SkBlurDrawLooper(SkScalar radius, SkScalar dx, SkScalar dy, SkColor color);
    virtual ~SkBlurDrawLooper();

    // overrides from SkDrawLooper
    virtual void init(SkCanvas*, SkPaint*);
    virtual bool next();
    virtual void restore();

protected:
    SkBlurDrawLooper(SkFlattenableReadBuffer&);
    // overrides from SkFlattenable
    virtual void flatten(SkFlattenableWriteBuffer& );
    virtual Factory getFactory() { return CreateProc; }

private:
    static SkFlattenable* CreateProc(SkFlattenableReadBuffer& buffer) {
        return SkNEW_ARGS(SkBlurDrawLooper, (buffer)); }

    SkCanvas*       fCanvas;
    SkPaint*        fPaint;
    SkMaskFilter*   fBlur;
    SkScalar        fDx, fDy;
    SkColor         fBlurColor;
    SkColor         fSavedColor;    // remember the original
    int             fSaveCount;

    enum State {
        kBeforeEdge,
        kAfterEdge,
        kDone
    };
    State   fState;
    
    typedef SkDrawLooper INHERITED;
};

#endif
