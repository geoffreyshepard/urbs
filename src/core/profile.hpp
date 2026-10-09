#pragma once
// Tracy zones. Compile to nothing unless the `profile` preset (TRACY_ENABLE) is used.
#include <tracy/Tracy.hpp>

#define URBS_ZONE(name) ZoneScopedN(name)
#define URBS_FRAME_MARK FrameMark
