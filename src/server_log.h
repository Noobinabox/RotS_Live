#ifndef SERVER_LOG_H
#define SERVER_LOG_H

// Writes str to stderr as one line of the server log, after the local time.
void log(const char* str);

#endif // SERVER_LOG_H
