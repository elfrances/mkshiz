#pragma once

#include "defs.h"

// Check the TrkPt's for duplicates and bogus values
extern int checkTrkPts(GpsTrk *pTrk, const CmdArgs *pArgs);

// Compute the basic metrics
extern int compMetrics(GpsTrk *pTrk, const CmdArgs *pArgs);
