#ifndef SkMMapStream_DEFINED
#define SkMMapStream_DEFINED

#include "SkStream.h"

class SkMMAPStream : public SkMemoryStream {
public:
    SkMMAPStream(const char filename[]);
    virtual ~SkMMAPStream();

    virtual void setMemory(const void* data, size_t length);
private:
    int     fFildes;
    void*   fAddr;
    size_t  fSize;
    
    void closeMMap();
    
    typedef SkMemoryStream INHERITED;
};

#endif
