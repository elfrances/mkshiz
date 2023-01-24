#pragma once

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern TrkPt *nxtTrkPt(TrkPt **p1, TrkPt *p2);
extern TrkPt *remTrkPt(GpsTrk *pTrk, TrkPt *p);
extern TrkPt *newTrkPt(int index, const char *inFile, int lineNum);
extern const char *fmtTrkPtIdx(const TrkPt *pTrkPt);
extern void printTrkPt(TrkPt *p);
extern void dumpTrkPts(GpsTrk *pTrk, TrkPt *p, int numPtsBefore, int numPtsAfter);

#ifdef __cplusplus
};
#endif
