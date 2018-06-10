/* clock.h */
// Basic clock implementation to simplify time telling.

#ifndef CLOCK_H
#define CLOCK_H
#pragma once

#include <ctime>
#include <sys/time.h>

class rots_clock {
public:
    rots_clock();

    // Returns the number of seconds that have passed since the last time this function
    // was called.
    float get_elapsed_seconds();

private:
    // Returns the amount of time that has passed between "now" and "then".
    timeval timediff(const timeval& now, const timeval& then);
    float timediff_seconds(const timeval& now, const timeval& then);

    timeval current_time;
};

#endif /* CLOCK_H */
