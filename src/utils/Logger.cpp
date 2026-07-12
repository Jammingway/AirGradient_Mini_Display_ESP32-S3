#include "Logger.h"
#include <stdarg.h>

namespace Logger {

static Sink s_sink = nullptr;

void setSink(Sink sink) { s_sink = sink; }

void log(Level level, const char* tag, const char* fmt, ...) {
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    static const char* names[] = {"D", "I", "W", "E"};
    Serial.printf("[%8lu][%s][%s] %s\n", millis(), names[(int)level], tag, buf);

    if (s_sink) s_sink(level, tag, buf);
}

}  // namespace Logger
