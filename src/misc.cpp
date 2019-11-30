#include <chrono>
#include <cstdio>
#include "defs.h"

#ifdef WIN32
#include "windows.h"
#elseif MINGW
#include <io.h>
#else
#include "sys/select.h"
#include "unistd.h"
#include <cstring>
#endif

TimeMS getTimeMS() {
//    return std::chrono::system_clock::now().time_since_epoch().count();
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// From: http://home.arcor.de/dreamlike/chess/
/// Is there input waiting?
int InputWaiting()
{
#if !defined(WIN32) && !defined(WIN64)
    fd_set readfds;
    struct timeval tv;
    FD_ZERO (&readfds);
    FD_SET (fileno(stdin), &readfds);
    tv.tv_sec=0; tv.tv_usec=0;
    select(16, &readfds, 0, 0, &tv);

    return (FD_ISSET(fileno(stdin), &readfds));
#else
    static int init = 0, pipe;
    static HANDLE inh;
    DWORD dw;

    if (!init) {
      init = 1;
      inh = GetStdHandle(STD_INPUT_HANDLE);
      pipe = !GetConsoleMode(inh, &dw);
      if (!pipe) {
         SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
         FlushConsoleInputBuffer(inh);
       }
     }
     if (pipe) {
       if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
       return dw;
     } else {
       GetNumberOfConsoleInputEvents(inh, &dw);
       return dw <= 1 ? 0 : dw;
	 }
#endif
}

void ReadInput(SearchInfo *info) {
    int             bytes;
    char            input[256] = "", *endc;

    if (InputWaiting()) {
        info->stopped = TRUE;
        do {
            bytes=read(fileno(stdin),input,256);
        } while (bytes<0);
        endc = strchr(input,'\n');
        if (endc) *endc=0;

        if (strlen(input) > 0) {
            if (!strncmp(input, "quit", 4))    {
                info->quit = TRUE;
            }
        }
        return;
    }
}
