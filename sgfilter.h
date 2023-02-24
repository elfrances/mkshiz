#pragma once

#define DEFAULT_M (4)
#define DEFAULT_LD (0)
#define CONVOLVE_WITH_NR_CONVLV (0)

#ifdef __cplusplus
extern "C" {
#endif

extern double* dvector(long nl, long nh);
extern void free_dvector(double *v, long nl, long nh);
extern char sgfilter(double yr[], double yf[], int mm, int nl, int nr, int ld, int m);

#ifdef __cplusplus
};
#endif
