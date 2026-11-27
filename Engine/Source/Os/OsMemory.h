#ifndef ENGINE_SOURCE_OS_OSMEMORY_H
#define ENGINE_SOURCE_OS_OSMEMORY_H

#include <cstddef>

#if defined(WOWDECOMP_ARCH_64)
void* OsAllocNear(const void* anchor, size_t size);
void OsFreeNear(void* ptr);
#endif

// CoA diagnostics (set COA_HEAPCHECK=1): validate the process heaps at labelled points and log the first corruption with the last events.
void CoaHeapNote(const char* what, const char* detail);
void CoaHeapCheck(const char* label);

#endif
