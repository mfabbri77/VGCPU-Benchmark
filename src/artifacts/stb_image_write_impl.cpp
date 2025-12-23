#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996) // CRT_SECURE_NO_WARNINGS
#endif

// Include path assumes "third_party" is in the include path or we point to it relative?
// The blueprint says "check into third_party/stb/stb_image_write.h". 
// Usually we add third_party/ to include path.
#include "stb/stb_image_write.h"

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
