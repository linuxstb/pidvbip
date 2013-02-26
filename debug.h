#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef NDEBUG
#define DEBUGF(...)
#else
#define DEBUGF(...) fprintf(stderr,__VA_ARGS__)
#endif

#endif
