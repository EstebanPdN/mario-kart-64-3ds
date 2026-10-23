#pragma once
#include <cstdlib>
inline void* linearMemAlign(std::size_t size,std::size_t alignment){void* pointer=nullptr;return posix_memalign(&pointer,alignment,size)?nullptr:pointer;}
inline void linearFree(void* pointer){std::free(pointer);}
inline int GSPGPU_FlushDataCache(const void*,std::size_t){return 0;}
#define R_FAILED(result) ((result)<0)
