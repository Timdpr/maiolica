#include <chrono>
#include "defs.h"

TimeMS getTimeMS() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
