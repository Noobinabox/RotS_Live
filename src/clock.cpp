/* clock.cpp */

#include "clock.h"

//============================================================================
rots_clock::rots_clock()
{
    gettimeofday(&current_time, NULL);
}

//============================================================================
float rots_clock::get_elapsed_seconds()
{
    timeval last_time = current_time;
    gettimeofday(&current_time, NULL);

    return timediff_seconds(current_time, last_time);
}

//============================================================================
float rots_clock::timediff_seconds(const timeval& now, const timeval& then)
{
    timeval time_passed = timediff(now, then);
    float time_delta = time_passed.tv_sec + time_passed.tv_usec / 1000000.0f;
    return time_delta;
}

//============================================================================
timeval rots_clock::timediff(const timeval& now, const timeval& then)
{
    timeval time_passed;

    timeval temp = now;

    time_passed.tv_usec = temp.tv_usec - then.tv_usec;
    if (time_passed.tv_usec < 0) {
        time_passed.tv_usec += 1000000;
        --temp.tv_sec;
    }

    time_passed.tv_sec = temp.tv_sec - then.tv_sec;
    if (time_passed.tv_sec < 0) {
        time_passed.tv_usec = 0;
        time_passed.tv_sec = 0;
    }

    return time_passed;
}
