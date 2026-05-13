//covers_range.h
//
/*
 The MIT License (MIT)
 Copyright (c) 2025 HouSisong
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef covers_range_h
#define covers_range_h
#include "../../diff_types.h"
#include "../mem_buf.h"
namespace hdiff_private{

struct TCoversRange{
    struct TClipData{
        const size_t newWindowSize;
        const size_t oldWindowSize;
        const size_t kBigCoverSize;

        //todo: some temp datas
        inline TClipData(size_t _newWindowSize,size_t _oldWindowSize,size_t _kBigCoverSize)
        :newWindowSize(_newWindowSize),oldWindowSize(_oldWindowSize),kBigCoverSize(_kBigCoverSize){}
    };

    TCover*   cover_begin;
    TCover*   cover_end;
    uint64_t        cost;
    hpatch_TWindow  window; //limited by newWindowSize & oldWindowSize
    inline explicit TCoversRange(TCover* _cover_begin,TCover* _cover_end,TClipData& tempData)
     :cover_begin(_cover_begin),cover_end(_cover_end){  }
    inline size_t coverCount()const{ return cover_end-cover_begin; }
    bool clipRangeTo(TCoversRange& rangeRight,TClipData& tempData);
private:
};

}//namespace hdiff_private

#endif // covers_range_h