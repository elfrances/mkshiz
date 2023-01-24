#pragma once

#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int parseCsvFile(CmdArgs *pArgs, GpsTrk *pTrk, const char *inFile);
extern int parseFitFile(CmdArgs *pArgs, GpsTrk *pTrk, const char *inFile);
extern int parseGpxFile(CmdArgs *pArgs, GpsTrk *pTrk, const char *inFile);
extern int parseTcxFile(CmdArgs *pArgs, GpsTrk *pTrk, const char *inFile);

#ifdef __cplusplus
};
#endif
