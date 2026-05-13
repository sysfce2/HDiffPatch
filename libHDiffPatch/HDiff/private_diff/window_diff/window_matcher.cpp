//window_matcher.cpp
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

#include "window_matcher.h"
#include "covers_range.h"
#include <algorithm> //std::sort
#include <stdexcept>  //std::runtime_error
#define checki(value,info) { if (!(value)) { \
    assert(0);\
    throw std::runtime_error(info); } \
}
#define check(value) checki(value,"check "#value" error!")

namespace hdiff_private{

void getBigCoversInWindows(std::vector<std::vector<TCover> >& out_bigCoverss,const std::vector<TCover>& covers,
                           const std::vector<hpatch_TWindow>& windows,size_t kBigCoverSize){
    out_bigCoverss.clear();
    out_bigCoverss.resize(windows.size());
    size_t wi=0;
    for (size_t ci=0;ci<covers.size();++ci){
        TCover cover=covers[ci];
        while (cover.length>=kBigCoverSize){
            if (wi>=windows.size()) return; //finish
            if (cover.newPos+cover.length<=windows[wi].newPos) break; //next ci
            if (cover.newPos>=windows[wi].newPos+windows[wi].newLength) { ++wi; continue; } //next wi
            if (cover.newPos<windows[wi].newPos){
                hpatch_StreamPos_t cutLen=windows[wi].newPos-cover.newPos;
                setCover(cover,cover.oldPos+cutLen,cover.newPos+cutLen,cover.length-cutLen);
            }
            if (cover.oldPos+cover.length<=windows[wi].oldPos) break; //next ci
            if (cover.oldPos>=windows[wi].oldPos+windows[wi].oldLength) break; //next ci
            if (cover.oldPos<windows[wi].oldPos){
                hpatch_StreamPos_t cutLen=windows[wi].oldPos-cover.oldPos;
                setCover(cover,cover.oldPos+cutLen,cover.newPos+cutLen,cover.length-cutLen);
            }
            hpatch_StreamPos_t hitNewLen=(cover.newPos+cover.length<=windows[wi].newPos+windows[wi].newLength)?
                                          cover.length:windows[wi].newPos+windows[wi].newLength-cover.newPos;
            hpatch_StreamPos_t hitOldLen=(cover.oldPos+cover.length<=windows[wi].oldPos+windows[wi].oldLength)?
                                          cover.length:windows[wi].oldPos+windows[wi].oldLength-cover.oldPos;
            hpatch_StreamPos_t hitLen=(hitNewLen<hitOldLen)?hitNewLen:hitOldLen;
            if (hitLen>=kBigCoverSize){
                TCover hitCover={cover.oldPos,cover.newPos,hitLen};
                out_bigCoverss[wi].push_back(hitCover);
            }
            setCover(cover,cover.oldPos+hitLen,cover.newPos+hitLen,cover.length-hitLen);
        }
    }
}

void updateCoversPosIntoWindows(std::vector<TCover>& covers,const hpatch_TWindow& window){
    for (size_t i=0;i<covers.size();++i){
        TCover& cover=covers[i];
        check(cover.oldPos+cover.length<=window.oldLength);
        check(cover.newPos+cover.length<=window.newLength);
        cover.oldPos+=window.oldPos;
        cover.newPos+=window.newPos;
    }
}

void getWindowBoxByCovers(hpatch_TWindow& out_window,const std::vector<TCover>& covers){
    const TCover* cover_begin=covers.data();
    const TCover* cover_end=covers.data()+covers.size();
    hpatch_StreamPos_t oldMin= ~(hpatch_StreamPos_t)0;
    hpatch_StreamPos_t oldMax= 0;
    hpatch_StreamPos_t newMin= ~(hpatch_StreamPos_t)0;
    hpatch_StreamPos_t newMax= 0;
    for (;cover_begin!=cover_end;){
        const TCover& cover = *cover_begin++;
        if (cover.length==0) continue;
        oldMin=(oldMin<cover.oldPos)?oldMin:cover.oldPos;
        oldMax=(oldMax>cover.oldPos+cover.length)?oldMax:cover.oldPos+cover.length;
        newMin=(newMin<cover.newPos)?newMin:cover.newPos;
        newMax=(newMax>cover.newPos+cover.length)?newMax:cover.newPos+cover.length;
    }
    
    if ((oldMin<oldMax)&(newMin<newMax)){
        out_window.oldPos=oldMin;
        out_window.oldLength=oldMax-oldMin;
        out_window.newPos=newMin;
        out_window.newLength=newMax-newMin;
    }else{ //empty window
        out_window.oldPos=0;
        out_window.oldLength=0;
        out_window.newPos=0;
        out_window.newLength=0;
    }
}

    static void _extenPos(hpatch_StreamPos_t& pos,hpatch_StreamPos_t& length,size_t windowSize,
                          hpatch_StreamPos_t limitPos,hpatch_StreamPos_t limitPosEnd){
        static size_t kExtenPosSize = 1024*4;
        hpatch_StreamPos_t posEnd=pos+length;
        assert((windowSize>=length)&&(pos>=limitPos)&&(posEnd<=limitPosEnd));
        size_t extenLen=(windowSize-(size_t)length)>>1;
        if (extenLen){
            extenLen=(extenLen<kExtenPosSize)?extenLen:kExtenPosSize;
            pos=(pos<extenLen)?0:pos-extenLen;
            pos=(pos>limitPos)?pos:limitPos;
            posEnd+=extenLen;
            posEnd=(posEnd<limitPosEnd)?posEnd:limitPosEnd;
            length=posEnd-pos;
        }
    }
void extenWindowsForMatch(std::vector<hpatch_TWindow>& windows,hpatch_StreamPos_t newSize,
                          hpatch_StreamPos_t oldSize,size_t newWindowSize,size_t oldWindowSize){
    hpatch_StreamPos_t newPosEndBck=0;
    for (size_t i=0;i<windows.size();++i){
        hpatch_TWindow& window=windows[i];
        assert(window.newLength>0);
        if (window.oldLength==0) continue;
        _extenPos(window.oldPos,window.oldLength,oldWindowSize,0,oldSize);
        hpatch_StreamPos_t newLimitPosEnd=(i+1<windows.size())?
                                (window.newPos+window.newLength+windows[i+1].newPos)/2:newSize;
        _extenPos(window.newPos,window.newLength,newWindowSize,newPosEndBck,newLimitPosEnd);
        newPosEndBck=window.newPos+window.newLength;
    }
}

    struct TWindowCmpByNewPos{
        inline bool operator()(const hpatch_TWindow& a,const hpatch_TWindow& b)const{
                                return a.newPos < b.newPos; }
    };
void rangesToWindows(std::vector<hpatch_TWindow>& out_windows,const std::vector<TCoversRange>& cvRanges){
    out_windows.reserve(cvRanges.size());
    for (size_t i=0;i<cvRanges.size();i++){
        const hpatch_TWindow& window=cvRanges[i].window;
        if (window.oldLength>0)
            out_windows.push_back(window);
    }
    std::sort(out_windows.begin(),out_windows.end(),TWindowCmpByNewPos());
}

static void _check_windows_safe(const std::vector<hpatch_TWindow>& windows,hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize){
    hpatch_StreamPos_t newPosEndBck=0;
    for (size_t i=0;i<windows.size();++i){
        const hpatch_TWindow& window=windows[i];
        check((window.newLength>0)&&(window.oldLength>0));
        check(window.newPos+window.newLength<=newSize);
        check(window.oldPos+window.oldLength<=oldSize);
        check(window.newPos>=newPosEndBck);
        newPosEndBck=window.newPos+window.newLength;
    }
}

TWindowMatcher::~TWindowMatcher(){
}

TWindowMatcher::TWindowMatcher(hpatch_StreamPos_t newSize,hpatch_StreamPos_t oldSize,size_t newWindowSize,
                               size_t oldWindowSize,size_t kBigCoverSize,std::vector<TCover>& covers,size_t threadNum)
:m_newSize(newSize),m_oldSize(oldSize),m_newWindowSize(newWindowSize),m_oldWindowSize(oldWindowSize),
 m_kBigCoverSize(kBigCoverSize),m_covers(covers),m_threadNum((threadNum>0)?threadNum:1){
}

void TWindowMatcher::search_windows(std::vector<hpatch_TWindow>& out_windows){
    _out_diff_info("    search windows by covers ...\n");
    out_windows.clear();
    if (m_covers.empty()) return;

    TCoversRange::TClipData tempData(m_newWindowSize,m_oldWindowSize,m_kBigCoverSize);
    std::vector<TCoversRange> cvRanges;
    cvRanges.reserve(1+(m_newSize/m_newWindowSize));
    cvRanges.push_back(TCoversRange(m_covers.data(),m_covers.data()+m_covers.size(),tempData));

    size_t curRangei=0;
    while (curRangei<cvRanges.size()){
        TCoversRange& cvRange=cvRanges[curRangei];
        TCoversRange cvRangeRight=cvRange;
        if (cvRange.clipRangeTo(cvRangeRight,tempData)){
            if (cvRangeRight.window.oldLength>0)
                cvRanges.push_back(cvRangeRight);
        }else{
            ++curRangei;
        }
    }

    rangesToWindows(out_windows,cvRanges);
    _check_windows_safe(out_windows,m_newSize,m_oldSize);
}


}//namespace hdiff_private
