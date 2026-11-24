// The miniaudio configuration of upstream's AudioDevice.cpp (which holds the
// implementation), for the host files that need its declarations.
#pragma once
#define MA_NO_DEVICE_IO
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_MP3
#define MA_NO_FLAC
#define MA_NO_ENCODING
#define MA_NO_THREADING
#include "third_party/miniaudio.h"
