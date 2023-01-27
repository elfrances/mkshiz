#pragma once

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Given the current TrkPt 'p2' return the next one and sore the previous one in 'p1'
extern TrkPt *nxtTrkPt(TrkPt **p1, TrkPt *p2);

// Delete the specified TrkPt from the list and return the next one
extern TrkPt *remTrkPt(GpsTrk *pTrk, TrkPt *p);

// Create a skeletal TrkPt
extern TrkPt *newTrkPt(int index, const char *inFile, int lineNum);

// Duplicate (clone) a TrkPt
extern TrkPt *dupTrkPt(const TrkPt *pTrkPt);

extern const char *fmtTrkPtIdx(const TrkPt *pTrkPt);
extern void printTrkPt(TrkPt *p);
extern void dumpTrkPts(GpsTrk *pTrk, TrkPt *p, int numPtsBefore, int numPtsAfter);

#ifdef __cplusplus
};
#endif
