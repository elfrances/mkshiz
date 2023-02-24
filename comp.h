#pragma once

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// Check the TrkPt's for duplicates and bogus values
extern int checkTrkPts(GpsTrk *pTrk, const CmdArgs *pArgs);

// Compute the Centered Moving Average of the specified metric
extern int compCMA(GpsTrk *pTrk, const CmdArgs *pArgs);

// Compute the basic metrics
extern int compMetrics(GpsTrk *pTrk, const CmdArgs *pArgs);

// Compute the Savitzky—Golay of the specified metric
extern int compSGF(GpsTrk *pTrk, const CmdArgs *pArgs);

// Compute the Simple Moving Average of the specified metric
extern int compSMA(GpsTrk *pTrk, const CmdArgs *pArgs);

// Compute the min/avg/max values
extern int computeMinMaxValues(GpsTrk *pTrk);

// Save the TrkPt's so that they can be restored if
// needed.
extern int saveTrkPts(GpsTrk *pTrk);

// Restore the TrkPt's from the last set saved
extern int restoreTrkPts(GpsTrk *pTrk);

#ifdef __cplusplus
};
#endif
