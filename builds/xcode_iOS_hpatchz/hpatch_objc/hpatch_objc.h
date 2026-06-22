//
//  hpatch_objc.h
//  hpatchz
//
//  Created by hss on 2023-08-25.
//

#import <Foundation/Foundation.h>

@interface hpatcher : NSObject

    //  return THPatchResult, 0 is ok, other is error;
    //  cacheMemory:
    //    cacheMemory is used for file IO, different cacheMemory only affects patch speed;
    //    recommended 1024*1024*1,1024*1024*8,...
    //    NOTE: if diffFile created by $hdiffz -w[-*] -VCD[-*], $xdelta3(-S,-S lzma),$open-vcdiff, alloc memory=cacheMemory+sourceWindowSize+targetWindowSize;
    //        ( sourceWindowSize & targetWindowSize is set when diff, C API getVcDiffInfo() can got values from diffFile. )
    //  if need VCDIFF ($hdiffz -VCD or xdelta3 or open-vcdiff ) format support, build .so with VCD=1 can opened;
    //  if need bsdiff4 & endsley/bsdiff format support, build .so with BSD=1 BZIP2=1 can opened;
    //  if need supported oldFileName or outNewFileName is directory, build .so with DIR=1 can opened;
    //  threadNum: 1..5, default 1; >1 opened multi-thread patching;
    //      now support single compressed diffData(created by hdiffz -SD) & window diffData(created by hdiffz -WD, recommended)
    //  isChecksumNewData: default true; checsum newData when patching VCDIFF and window diffData;

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName;

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName
           byMemory:(int64_t)cacheMemory;

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName
           byMemory:(int64_t)cacheMemory
        byThreadNum:(size_t)threadNum;

+ (int)patchWithOld:(NSString *)oldFileName
           withDiff:(NSString *)diffFileName
              toNew:(NSString *)outNewFileName
           byMemory:(int64_t)cacheMemory
        byThreadNum:(size_t)threadNum
         byChecksum:(BOOL)isChecksumNewData;

@end
