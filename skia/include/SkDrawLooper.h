#ifndef SkDrawLooper_DEFINED
#define SkDrawLooper_DEFINED

#include "SkFlattenable.h"

////////////////// EXPERIMENTAL <reed> //////////////////////////

class SkCanvas;
class SkPaint;

/** \class SkDrawLooper
    Subclasses of SkDrawLooper can be attached to a SkPaint. Where they are,
    and something is drawn to a canvas with that paint, the looper subclass will
    be called, allowing it to modify the canvas and/or paint for that draw call.
    More than that, via the next() method, the looper can modify the draw to be
    invoked multiple times (hence the name loop-er), allow it to perform effects
    like shadows or frame/fills, that require more than one pass.
*/
class SkDrawLooper : public SkFlattenable {
public:
    /** Called right before something is being drawn to the specified canvas
        with the specified paint. Subclass that want to modify either parameter
        can do so now.
    */
    virtual void init(SkCanvas*, SkPaint*) {}
    /** Called in a loop (after init()). Each time true is returned, the object
        is drawn (possibly with a modified canvas and/or paint). When false is
        finally returned, drawing for the object stops.
    */
    virtual bool next() { return false; }
    /** Called after the looper has finally returned false from next(), allowing
        the looper to restore the canvas/paint to their original states.
        <reed> is this required, since the subclass knows when it is done???
        <reed> should we pass the canvas/paint here, and/or to the next call
        so that subclasses don't need to retain pointers to them during the 
        loop?
    */
    virtual void restore() {}
};

#endif
