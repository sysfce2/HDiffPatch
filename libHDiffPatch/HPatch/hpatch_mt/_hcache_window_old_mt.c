//  _hcache_window_old_mt.c
//  hpatch
/*
 The MIT License (MIT)
 Copyright (c) 2026 HouSisong
*/
#include "_hcache_window_old_mt.h"
#include "_patch_private_mt.h"
#include "../patch_private.h"
#if (_IS_USED_MULTITHREAD)

    // Per-window tracking info for the prepared batch
    typedef struct {
        hpatch_StreamPos_t oldPos;
        size_t             len;
        size_t             bufPos;     // offset in ring buffer
    } _win_info_t;

#define _kWinMask  (hpatch_kMaxWindowMetaCount-1)

// Ring-buffer old data cache manager with async preload thread
typedef struct hcache_window_old_mt_t{
    unsigned char*         buf; // Ring buffer
    size_t                 bufSize;
    // Ring buffer write position (next window placed here, wraps at bufSize)
    size_t                 writePos;

    // Prepared window batch info (ring array)
    _win_info_t            winInfos[hpatch_kMaxWindowMetaCount];
    size_t                 winStart;          // ring start index (first unconsumed window)
    volatile size_t        winCount;          // number of unconsumed windows in ring
    volatile size_t        processedWinCount; // number of already-processed windows from winStart
    volatile size_t        readyWinCount;     // number of ready windows from winStart

    // Protected region: written-but-unconsumed data in ring buffer
    // Async thread checks overlap against this before each window write
    volatile size_t        protectedBufPos; // bufPos of first unconsumed window
    volatile size_t        protectedLen;    // total bytes of written-but-unconsumed data

    volatile hpatch_StreamPos_t leaveWindowCount;// Window count tracking
    hpatch_mt_base_t            mt_base;// Threading infrastructure
    const hpatch_TStreamInput*  old_stream;
} hcache_window_old_mt_t;

size_t hcache_window_old_mt_t_memSize(void) {
    return sizeof(hcache_window_old_mt_t);
}

// ---- ring buffer helpers ----

    static hpatch_inline hpatch_BOOL _buf_range_wraps(size_t pos, size_t len, size_t bufSize) {
        return (pos + len > bufSize);
    }

    static hpatch_inline void __swap(size_t* a,size_t* b) { size_t t=*a; *a=*b; *b=t; }
    
    // Check if two ring buffer ranges overlap
    static hpatch_inline hpatch_BOOL _buf_ranges_overlap(size_t pos1,size_t len1,size_t pos2,size_t len2,size_t bufSize) {
        size_t end1=pos1+len1;
        size_t end2=pos2+len2;
        if ((len1==0) || (len2==0)) return hpatch_FALSE;
        if (pos1>pos2) {  __swap(&pos1,&pos2); __swap(&end1,&end2);  }
        //  [             bufSize                   ]
        //       {pos1          end1}  (pos2           end2)   
        return (pos2<end1)||(pos1+bufSize<end2);
    }

    // Execute read commands for one window, computed on-the-fly from winInfo
    static hpatch_BOOL _exec_window_read(hcache_window_old_mt_t* self,
                                        hpatch_StreamPos_t oldPos, size_t bufPos, size_t len) {
        if (len == 0) return hpatch_TRUE;
        if (!_buf_range_wraps(bufPos, len, self->bufSize)) {
            return self->old_stream->read(self->old_stream, oldPos,
                    self->buf + bufPos, self->buf + bufPos + len);
        } else {
            size_t part1 = self->bufSize - bufPos;
            if (!self->old_stream->read(self->old_stream, oldPos,
                    self->buf + bufPos, self->buf + bufPos + part1))
                return hpatch_FALSE;
            return self->old_stream->read(self->old_stream, oldPos + part1,
                    self->buf, self->buf + (len - part1));
        }
    }

// ---- async thread ----

static void _hcache_window_old_mt_thread(int threadIndex, void* workData) {
    hcache_window_old_mt_t* self = (hcache_window_old_mt_t*)workData;

    while (!hpatch_mt_isOnFinish(self->mt_base.h_mt)){
        size_t localUnprocessedCount=0;
        size_t localUnprocessedWinStart;

        c_locker_enter(self->mt_base._locker);
        if ((!self->mt_base.isOnError) && (self->winCount <= self->processedWinCount)) 
            c_condvar_wait(self->mt_base._waitCondvar, self->mt_base._locker);
        if ((!self->mt_base.isOnError) && (self->winCount>self->processedWinCount)) {
            localUnprocessedCount = self->winCount - self->processedWinCount;
            localUnprocessedWinStart = self->winStart+self->processedWinCount;
            self->processedWinCount = self->winCount; // mark all current windows as processed
        }
        c_locker_leave(self->mt_base._locker);

        {// Execute reads for unprocessed windows, checking protected region overlap per window
            size_t winIdx;
            for (winIdx=0;winIdx<localUnprocessedCount;++winIdx) {
                const _win_info_t* winInfo=&self->winInfos[(localUnprocessedWinStart + winIdx) & _kWinMask];
                size_t winBufPos = winInfo->bufPos;
                size_t winLen = winInfo->len;

                // Wait until this window's write doesn't overlap with protected region
                c_locker_enter(self->mt_base._locker);
                while ((!self->mt_base.isOnError)
                      &&_buf_ranges_overlap(winBufPos, winLen,self->protectedBufPos,self->protectedLen,self->bufSize)) {
                    c_condvar_wait(self->mt_base._waitCondvar, self->mt_base._locker);
                }
                // Mark this window's data as protected (it will be written next)
                self->protectedLen += winLen;
                c_locker_leave(self->mt_base._locker);
                if (self->mt_base.isOnError) break;

                // Execute read commands for this window (outside lock, computed on-the-fly)
                if (!_exec_window_read(self, winInfo->oldPos,winBufPos, winLen)){
                    hpatch_mt_base_setOnError_(&self->mt_base);
                    break;
                }

                // Signal this window's completion
                c_locker_enter(self->mt_base._locker);
                ++self->readyWinCount;
                c_condvar_signal(self->mt_base._waitCondvar);
                c_locker_leave(self->mt_base._locker);
            }
        }
    }

    hpatch_mt_base_aThreadEnd_(&self->mt_base);
}

    static hpatch_inline void _hcache_window_old_free(hcache_window_old_mt_t* self){
        if (!self) return;
        _hpatch_mt_base_free(&self->mt_base);
    }

// ---- public API ----
struct hcache_window_old_mt_t* hcache_window_old_open(void* pmem,size_t memSize,struct hpatch_mt_t* h_mt,
                                                      unsigned char* buf, size_t bufSize,
                                                      const hpatch_TStreamInput* old_stream,hpatch_StreamPos_t windowCount){
    hcache_window_old_mt_t* self=pmem;
    if (memSize<hcache_window_old_mt_t_memSize()) return 0;
    memset(self,0,sizeof(*self));
    self->buf = buf;
    self->bufSize = bufSize;
    self->old_stream = old_stream;
    self->leaveWindowCount = windowCount;

    if (!_hpatch_mt_base_init(&self->mt_base,h_mt,0,0))
        goto _on_error;

    if (self->leaveWindowCount>0){
        if (!hpatch_mt_base_aThreadBegin_(&self->mt_base,_hcache_window_old_mt_thread,self))
            goto _on_error;
    }
    return self;

_on_error:
    _hcache_window_old_free(self);
    return 0;
}

void hcache_window_old_prepareBatch(struct hcache_window_old_mt_t* self,hpatch_size_t batchCount,
                                    const hpatch_StreamPos_t* windowOldPos,const hpatch_StreamPos_t* windowOldLength,
                                    hpatch_size_t posStart,hpatch_size_t posMask){
    c_locker_enter(self->mt_base._locker);
    {
        hpatch_size_t i;
        assert(batchCount+self->winCount<=hpatch_kMaxWindowMetaCount);
        for (i = 0; i < batchCount; ++i) {
            hpatch_size_t idx = (posStart + i) & posMask;
            hpatch_StreamPos_t oldPos = windowOldPos[idx];
            size_t oldLen = (size_t)windowOldLength[idx];
            size_t wrBufPos=self->writePos;
            size_t wiIdx = (self->winStart + self->winCount) & _kWinMask;
            assert(oldLen <= self->bufSize);

            if (self->winCount==0)// Set protectedBufPos for the first window
                self->protectedBufPos = wrBufPos;
            // Record window info
            self->winInfos[wiIdx].oldPos   = oldPos;
            self->winInfos[wiIdx].len      = oldLen;
            self->winInfos[wiIdx].bufPos   = wrBufPos;
            // Advance writePos
            self->writePos = (wrBufPos + oldLen) % self->bufSize;
            ++self->winCount;
        }
    }
    c_condvar_signal(self->mt_base._waitCondvar);//Signal async thread: new windows available for processing
    c_locker_leave(self->mt_base._locker);
}

hpatch_BOOL hcache_window_old_getWindow(hcache_window_old_mt_t* self,
                                        hpatch_StreamPos_t windowOldPos,hpatch_StreamPos_t windowOldLength,
                                        unsigned char** out_seg1_begin, unsigned char** out_seg1_end,
                                        unsigned char** out_seg2_begin, unsigned char** out_seg2_end){
    _win_info_t* wi = &self->winInfos[self->winStart & _kWinMask];
    assert(self->winCount>0);// A batch must be prepared and not exhausted
    assert(wi->oldPos==windowOldPos);
    assert(wi->len==windowOldLength);
    if (self->readyWinCount<=0){// Wait until this window's data is ready
        c_locker_enter(self->mt_base._locker);
        while ((self->readyWinCount<=0) && (!self->mt_base.isOnError))
            c_condvar_wait(self->mt_base._waitCondvar, self->mt_base._locker);
        c_locker_leave(self->mt_base._locker);
        if (self->mt_base.isOnError) return hpatch_FALSE;
    }
    {// Return pointers into the ring buffer
        size_t       oldLen = wi->len;
        size_t       bufPos = wi->bufPos;
        *out_seg1_begin = self->buf + bufPos;
        if (!_buf_range_wraps(bufPos,oldLen,self->bufSize)) {
            *out_seg1_end   = self->buf + bufPos + oldLen;
            *out_seg2_begin = 0;
            *out_seg2_end   = 0;
        } else {
            *out_seg1_end   = self->buf + self->bufSize;
            *out_seg2_begin = self->buf;
            *out_seg2_end   = self->buf + ((bufPos + oldLen) - self->bufSize);
        }
    }
    return hpatch_TRUE;
}

void hcache_window_old_finishWindow(hcache_window_old_mt_t* self){
    c_locker_enter(self->mt_base._locker);
    {
        size_t ri = self->winStart & _kWinMask;
        size_t finishedLen = self->winInfos[ri].len;
        // Advance ring: free the consumed window's slot
        self->winStart = (self->winStart + 1) & _kWinMask;
        --self->winCount;
        --self->processedWinCount;
        --self->readyWinCount;
        if (self->winCount>0)// Update protected region: point to next unconsumed window
            self->protectedBufPos = self->winInfos[self->winStart & _kWinMask].bufPos;
        self->protectedLen -= finishedLen;
        --self->leaveWindowCount;
    }
    c_condvar_signal(self->mt_base._waitCondvar); // wake async thread waiting on protected region
    c_locker_leave(self->mt_base._locker);
    if (self->leaveWindowCount==0)
        hpatch_mt_isOnFinish(self->mt_base.h_mt);
}

hpatch_BOOL hcache_window_old_close(hcache_window_old_mt_t* self){
    _hcache_window_old_free(self);
    return (self->leaveWindowCount==0)&&(!self->mt_base.isOnError);
}

#endif //_IS_USED_MULTITHREAD
