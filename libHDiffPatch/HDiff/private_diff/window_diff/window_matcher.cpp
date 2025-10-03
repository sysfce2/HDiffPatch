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

void updateCoversIntoWindows(std::vector<TCover>& covers,hpatch_TWindow& window){
    hpatch_StreamPos_t _oldPos   =window.oldPos+window.oldLength;
    hpatch_StreamPos_t _oldPosEnd=window.oldPos;
    for (size_t i=0;i<covers.size();++i){
        TCover& cover=covers[i];
        assert(cover.oldPos+cover.length<=window.oldLength);
        assert(cover.newPos+cover.length<=window.newLength);
        cover.oldPos+=window.oldPos;
        cover.newPos+=window.newPos;
        
        _oldPos=(cover.oldPos<_oldPos)?cover.oldPos:_oldPos;
        _oldPosEnd=(cover.oldPos+cover.length>_oldPosEnd)?cover.oldPos+cover.length:_oldPosEnd;
    }

    if (_oldPos<_oldPosEnd){
        window.oldPos=_oldPos;
        window.oldLength=_oldPosEnd-_oldPos;
    }else{ //empty window
        window.oldPos=0;
        window.oldLength=0;
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
    


}


}//namespace hdiff_private
