#ifndef SkPtrRecorder_DEFINED
#define SkPtrRecorder_DEFINED

#include "SkRefCnt.h"
#include "SkTDArray.h"

class SkPtrRecorder : public SkRefCnt {
public:
    uint32_t recordPtr(void*);
    
    size_t count() const { return fList.count(); }
    void getPtrs(void* array[]) const;

    void reset();

protected:
    virtual void incPtr(void* ptr) {}
    virtual void decPtr(void* ptr) {}

private:
    struct Pair {
        void*       fPtr;
        uint32_t    fIndex;
    };
    SkTDArray<Pair>  fList;
    
    static int Cmp(const Pair& a, const Pair& b);
    
    typedef SkRefCnt INHERITED;
};

#endif
