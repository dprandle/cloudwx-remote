// #define MA_NO_DEVICE_IO
// #define MA_NO_THREADING
#include "basic_types.h"
#include "logging.h"
#define MA_ASSERT asrt
#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_ALSA
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
