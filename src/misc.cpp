#include <chrono>

int getTimeMS() {
    int now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    return now;
}
