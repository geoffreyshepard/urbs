#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
#if defined(__clang__)
#pragma clang diagnostic ignored "-Weverything"
#endif
#include <stb_image.h>
#include <stb_image_write.h>
