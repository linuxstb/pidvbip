#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef NDEBUG
#define DEBUGF(...)
#else
#define DEBUGF(...) fprintf(stderr,...)
#endif

#endif
