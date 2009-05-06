/*
  bsdiff.c -- Binary patch generator.

  Copyright 2003 Colin Percival

  For the terms under which this work may be distributed, please see
  the adjoining file "LICENSE".

  ChangeLog:
  2005-05-05 - Use the modified header struct from bspatch.h; use 32-bit
               values throughout.
                 --Benjamin Smedberg <benjamin@smedbergs.us>
  2005-05-18 - Use the same CRC algorithm as bzip2, and leverage the CRC table
               provided by libbz2.
                 --Darin Fisher <darin@meer.net>
  2007-11-14 - Changed to use Crc from Lzma library instead of Bzip library
                 --Rahul Kuchhal
  2009-03-31 - Change to use Streams.  Added lots of comments.
                 --Stephen Adams <sra@chromium.org>
*/

#include "third_party/courgette/bsdiff.h"

#include <stdlib.h>
#include <algorithm>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"

#include "third_party/courgette/crc.h"
#include "third_party/courgette/streams.h"

namespace courgette {

// ------------------------------------------------------------------------
//
// The following code is taken verbatim from 'bsdiff.c'. Please keep all the
// code formatting and variable names.  The only changes from the original are
// replacing tabs with spaces, indentation, and using 'const'.
//
// The code appears to be a rewritten version of the suffix array algorithm
// presented in "Faster Suffix Sorting" by N. Jesper Larsson and Kunihiko
// Sadakane, special-cased for bytes.

static void
split(int *I,int *V,int start,int len,int h)
{
  int i,j,k,x,tmp,jj,kk;

  if(len<16) {
    for(k=start;k<start+len;k+=j) {
      j=1;x=V[I[k]+h];
      for(i=1;k+i<start+len;i++) {
        if(V[I[k+i]+h]<x) {
          x=V[I[k+i]+h];
          j=0;
        };
        if(V[I[k+i]+h]==x) {
          tmp=I[k+j];I[k+j]=I[k+i];I[k+i]=tmp;
          j++;
        };
      };
      for(i=0;i<j;i++) V[I[k+i]]=k+j-1;
      if(j==1) I[k]=-1;
    };
    return;
  };

  x=V[I[start+len/2]+h];
  jj=0;kk=0;
  for(i=start;i<start+len;i++) {
    if(V[I[i]+h]<x) jj++;
    if(V[I[i]+h]==x) kk++;
  };
  jj+=start;kk+=jj;

  i=start;j=0;k=0;
  while(i<jj) {
    if(V[I[i]+h]<x) {
      i++;
    } else if(V[I[i]+h]==x) {
      tmp=I[i];I[i]=I[jj+j];I[jj+j]=tmp;
      j++;
    } else {
      tmp=I[i];I[i]=I[kk+k];I[kk+k]=tmp;
      k++;
    };
  };

  while(jj+j<kk) {
    if(V[I[jj+j]+h]==x) {
      j++;
    } else {
      tmp=I[jj+j];I[jj+j]=I[kk+k];I[kk+k]=tmp;
      k++;
    };
  };

  if(jj>start) split(I,V,start,jj-start,h);

  for(i=0;i<kk-jj;i++) V[I[jj+i]]=kk-1;
  if(jj==kk-1) I[jj]=-1;

  if(start+len>kk) split(I,V,kk,start+len-kk,h);
}

static void
qsufsort(int *I,int *V,const unsigned char *old,int oldsize)
{
  int buckets[256];
  int i,h,len;

  for(i=0;i<256;i++) buckets[i]=0;
  for(i=0;i<oldsize;i++) buckets[old[i]]++;
  for(i=1;i<256;i++) buckets[i]+=buckets[i-1];
  for(i=255;i>0;i--) buckets[i]=buckets[i-1];
  buckets[0]=0;

  for(i=0;i<oldsize;i++) I[++buckets[old[i]]]=i;
  I[0]=oldsize;
  for(i=0;i<oldsize;i++) V[i]=buckets[old[i]];
  V[oldsize]=0;
  for(i=1;i<256;i++) if(buckets[i]==buckets[i-1]+1) I[buckets[i]]=-1;
  I[0]=-1;

  for(h=1;I[0]!=-(oldsize+1);h+=h) {
    len=0;
    for(i=0;i<oldsize+1;) {
      if(I[i]<0) {
        len-=I[i];
        i-=I[i];
      } else {
        if(len) I[i-len]=-len;
        len=V[I[i]]+1-i;
        split(I,V,i,len,h);
        i+=len;
        len=0;
      };
    };
    if(len) I[i-len]=-len;
  };

  for(i=0;i<oldsize+1;i++) I[V[i]]=i;
}

static int
matchlen(const unsigned char *old,int oldsize,const unsigned char *newbuf,int newsize)
{
  int i;

  for(i=0;(i<oldsize)&&(i<newsize);i++)
    if(old[i]!=newbuf[i]) break;

  return i;
}

static int
search(int *I,const unsigned char *old,int oldsize,
       const unsigned char *newbuf,int newsize,int st,int en,int *pos)
{
  int x,y;

  if(en-st<2) {
    x=matchlen(old+I[st],oldsize-I[st],newbuf,newsize);
    y=matchlen(old+I[en],oldsize-I[en],newbuf,newsize);

    if(x>y) {
      *pos=I[st];
      return x;
    } else {
      *pos=I[en];
      return y;
    }
  }

  x=st+(en-st)/2;
  if(memcmp(old+I[x],newbuf,std::min(oldsize-I[x],newsize))<0) {
    return search(I,old,oldsize,newbuf,newsize,x,en,pos);
  } else {
    return search(I,old,oldsize,newbuf,newsize,st,x,pos);
  }
}

//  End of 'verbatim' code.
// ------------------------------------------------------------------------

static void WriteHeader(SinkStream* stream, MBSPatchHeader* header) {
  stream->Write(header->tag, sizeof(header->tag));
  stream->WriteVarint32(header->slen);
  stream->WriteVarint32(header->scrc32);
  stream->WriteVarint32(header->dlen);
  stream->WriteVarint32(header->cblen);
  stream->WriteVarint32(header->difflen);
  stream->WriteVarint32(header->extralen);
}

BSDiffStatus CreateBinaryPatch(SourceStream* old_stream,
                               SourceStream* new_stream,
                               SinkStream* patch_stream)
{
  base::Time start_bsdiff_time = base::Time::Now();
  LOG(INFO) << "Start bsdiff";
  size_t initial_patch_stream_length = patch_stream->Length();

  const uint8* old = old_stream->Buffer();
  const int oldsize = old_stream->Remaining();

  scoped_array<int> I(new(std::nothrow) int[oldsize + 1]);
  scoped_array<int> V(new(std::nothrow) int[oldsize + 1]);
  if (I == NULL || V == NULL)
    return MEM_ERROR;

  base::Time q_start_time = base::Time::Now();
  qsufsort(I.get(), V.get(), old, oldsize);
  LOG(INFO) << " done qsufsort "
            << (base::Time::Now() - q_start_time).InSecondsF();
  V.reset();

  const uint8* newbuf = new_stream->Buffer();
  const int newsize = new_stream->Remaining();

  // Allocate newsize+1 bytes instead of newsize bytes to ensure that we never
  // try to malloc(0) and get a NULL pointer.

  scoped_array<uint8> diff_bytes_array(new(std::nothrow) uint8[newsize + 1]);
  scoped_array<uint8> extra_bytes_array(new(std::nothrow) uint8[newsize + 1]);
  if (diff_bytes_array == NULL || extra_bytes_array == NULL)
    return MEM_ERROR;

  uint8* diff_bytes = diff_bytes_array.get();
  uint8* extra_bytes = extra_bytes_array.get();
  int control_length = 0;
  int diff_bytes_length = 0;
  int diff_bytes_nonzero = 0;
  int extra_bytes_length = 0;
  int eblen = 0;

  SinkStream control_stream;

  // The patch format is a sequence of triples <copy,extra,seek> where 'copy' is
  // the number of bytes to copy from the old file (possibly with mistakes),
  // 'extra' is the number of bytes to copy from a stream of fresh bytes, and
  // 'seek' is an offset to move to the position to copy for the next triple.
  //
  // The invariant at the top of this loop is that we are committed to emitting
  // a triple for the part of |newbuf| surrounding a 'seed' match near
  // |lastscan|.  We are searching for a second match that will be the 'seed' of
  // the next triple.  As we scan through |newbuf|, one of four things can
  // happen at the current position |scan|:
  //
  //  1. We find a nice match that appears to be consistent with the current
  //     seed.  Continue scanning.  It is likely that this match will become
  //     part of the 'copy'.
  //
  //  2. We find match which does much better than extending the current seed
  //     old match.  Emit a triple for the current seed and take this match as
  //     the new seed for a new triple.  By 'much better' we remove 8 mismatched
  //     bytes by taking the new seed.
  //
  //  3. There is not a good match.  Continue scanning.  These bytes will likely
  //     become part of the 'extra'.
  //
  //  4. There is no match because we reached the end of the input, |newbuf|.

  // This is how the loop advances through the bytes of |newbuf|:
  //
  // ...012345678901234567890123456789...
  //    ssssssssss                      Seed at |lastscan|
  //              xxyyyxxyyxy           |scan| forward, cases (3)(x) & (1)(y)
  //                         mmmmmmmm   New match will start new seed case (2).
  //    fffffffffffffff                 |lenf| = scan forward from |lastscan|
  //                     bbbb           |lenb| = scan back from new seed |scan|.
  //    ddddddddddddddd                 Emit diff bytes for the 'copy'.
  //                   xx               Emit extra bytes.
  //                     ssssssssssss   |lastscan = scan - lenb| is new seed.
  //                                 x  Cases (1) and (3) ....


  int lastscan = 0, lastpos = 0, lastoffset = 0;

  int scan = 0;
  int match_length = 0;

  while (scan < newsize) {
    int pos = 0;
    int oldscore = 0;  // Count of how many bytes of the current match at |scan|
                       // extend the match at |lastscan|.

    scan += match_length;
    for (int scsc = scan;  scan < newsize;  ++scan) {
      match_length = search(I.get(), old, oldsize,
                            newbuf + scan, newsize - scan,
                            0, oldsize, &pos);

      for ( ; scsc < scan + match_length ; scsc++)
        if ((scsc + lastoffset < oldsize) &&
            (old[scsc + lastoffset] == newbuf[scsc]))
          oldscore++;

      if ((match_length == oldscore) && (match_length != 0))
        break;  // Good continuing match, case (1)
      if (match_length > oldscore + 8)
        break;  // New seed match, case (2)

      if ((scan + lastoffset < oldsize) &&
          (old[scan + lastoffset] == newbuf[scan]))
        oldscore--;
      // Case (3) continues in this loop until we fall out of the loop (4).
    }

    if ((match_length != oldscore) || (scan == newsize)) {  // Cases (2) and (4)
      // This next chunk of code finds the boundary between the bytes to be
      // copied as part of the current triple, and the bytes to be copied as
      // part of the next triple.  The |lastscan| match is extended forwards as
      // far as possible provided doing to does not add too many mistakes.  The
      // |scan| match is extended backwards in a similar way.

      // Extend the current match (if any) backwards.  |lenb| is the maximal
      // extension for which less than half the byte positions in the extension
      // are wrong.
      int lenb = 0;
      if (scan < newsize) {  // i.e. not case (4); there is a match to extend.
        int score = 0, Sb = 0;
        for (int i = 1;  (scan >= lastscan + i) && (pos >= i);  i++) {
          if (old[pos - i] == newbuf[scan - i]) score++;
          if (score*2 - i > Sb*2 - lenb) { Sb = score; lenb = i; }
        }
      }

      // Extend the lastscan match forward; |lenf| is the maximal extension for
      // which less than half of the byte positions in entire lastscan match are
      // wrong.  There is a subtle point here: |lastscan| points to before the
      // seed match by |lenb| bytes from the previous iteration.  This is why
      // the loop measures the total number of mistakes in the the match, not
      // just the from the match.
      int lenf = 0;
      {
        int score = 0, Sf = 0;
        for (int i = 0;  (lastscan + i < scan) && (lastpos + i < oldsize);  ) {
          if (old[lastpos + i] == newbuf[lastscan + i]) score++;
          i++;
          if (score*2 - i > Sf*2 - lenf) { Sf = score; lenf = i; }
        }
      }

      // If the extended scans overlap, pick a position in the overlap region
      // that maximizes the exact matching bytes.
      if (lastscan + lenf > scan - lenb) {
        int overlap = (lastscan + lenf) - (scan - lenb);
        int score = 0;
        int Ss = 0, lens = 0;
        for (int i = 0;  i < overlap;  i++) {
          if (newbuf[lastscan + lenf - overlap + i] ==
              old[lastpos + lenf - overlap + i]) score++;
          if (newbuf[scan - lenb + i] ==  old[pos - lenb + i]) score--;
          if (score > Ss) { Ss = score; lens = i + 1; }
        }

        lenf += lens - overlap;
        lenb -= lens;
      };

      for (int i = 0;  i < lenf;  i++) {
        uint8 diff_byte = newbuf[lastscan + i] - old[lastpos + i];
        diff_bytes[diff_bytes_length + i] = diff_byte;
        if (diff_byte)
          ++diff_bytes_nonzero;
      }
      int gap = (scan - lenb) - (lastscan + lenf);
      for (int i = 0;  i < gap;  i++)
        extra_bytes[extra_bytes_length + i] = newbuf[lastscan + lenf + i];

      diff_bytes_length += lenf;
      extra_bytes_length += gap;

      uint32 copy_count = lenf;
      uint32 extra_count = gap;
      int32 seek_adjustment = ((pos - lenb) - (lastpos + lenf));

      control_stream.WriteVarint32(copy_count);
      control_stream.WriteVarint32(extra_count);
      control_stream.WriteVarint32Signed(seek_adjustment);
      ++control_length;
#ifdef DEBUG_bsmedberg
      LOG(INFO) << StringPrintf(
          "Writing a block:  copy: %-8u extra: %-8u seek: %+-9d",
          copy_count, extra_count, seek_adjustment);
#endif

      lastscan = scan - lenb;   // Include the backward extension in seed.
      lastpos = pos - lenb;     //  ditto.
      lastoffset = lastpos - lastscan;
    }
  }

  I.reset();

  MBSPatchHeader header;
  // The string will have a null terminator that we don't use, hence '-1'.
  COMPILE_ASSERT(sizeof(MBS_PATCH_HEADER_TAG) - 1 == sizeof(header.tag),
                 MBS_PATCH_HEADER_TAG_must_match_header_field_size);
  memcpy(header.tag, MBS_PATCH_HEADER_TAG, sizeof(header.tag));
  header.slen     = oldsize;
  header.scrc32   = CalculateCrc(old, oldsize);
  header.dlen     = newsize;
  header.cblen    = control_stream.Length();
  header.difflen  = diff_bytes_length;
  header.extralen = extra_bytes_length;

  WriteHeader(patch_stream, &header);

  patch_stream->Append(&control_stream);
  patch_stream->Write(diff_bytes, diff_bytes_length);
  patch_stream->Write(extra_bytes, extra_bytes_length);

  LOG(INFO) << "Control tuples: " << control_length
            << "  copy bytes: " << diff_bytes_length
            << "  mistakes: " << diff_bytes_nonzero
            << "  extra bytes: " << extra_bytes_length;

  LOG(INFO) << "Uncompressed bsdiff patch size "
            << patch_stream->Length() - initial_patch_stream_length;

  LOG(INFO) << "End bsdiff "
            << (base::Time::Now() - start_bsdiff_time).InSecondsF();

  return OK;
}

}  // namespace
