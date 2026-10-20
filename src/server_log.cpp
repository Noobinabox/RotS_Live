#include "server_log.h"

#include <cstdio>
#include <cstring>
#include <ctime>

void log(const char* str)
{
    const std::time_t now = std::time(nullptr);
    char* time_text = std::asctime(std::localtime(&now));
    // asctime ends its text with a newline; the log line supplies its own.
    time_text[std::strlen(time_text) - 1] = '\0';
    std::fprintf(stderr, "%-19.19s :: %s\n", time_text, str);
}
