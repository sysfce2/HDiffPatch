//window_matcher.h
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
#ifndef window_matcher_h
#define window_matcher_h
#include <vector>
#include "../../diff_types.h"
#include "../mem_buf.h"
namespace hdiff_private{

class TWindowMatcher{
public:
    TWindowMatcher(hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize,size_t newWindowSize,
                   size_t oldWindowSize,size_t kBigCoverSize,std::vector<TCover>& covers,size_t threadNum);
    void search_windows(std::vector<hpatch_TWindow>& out_windows);
    ~TWindowMatcher();
private:
    TWindowMatcher(const TWindowMatcher &); //empty
    TWindowMatcher &operator=(const TWindowMatcher &); //empty
private:
    const hpatch_StreamPos_t    m_newSize;
    const hpatch_StreamPos_t    m_oldSize;
    const size_t                m_newWindowSize;
    const size_t                m_oldWindowSize;
    const size_t                m_kBigCoverSize;
    std::vector<TCover>&        m_covers;
    const size_t                m_threadNum;
    TAutoMem                    m_mem;
};

void getBigCoversInWindows(std::vector<std::vector<TCover> >& out_bigCoverss,const std::vector<TCover>& covers,
                           const std::vector<hpatch_TWindow>& windows,size_t kBigCoverSize);

void updateCoversIntoWindows(std::vector<TCover>& covers,hpatch_TWindow& window);

}//namespace hdiff_private
#endif
