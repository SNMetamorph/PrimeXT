#ifndef TIMECOUNTER_H__
#define TIMECOUNTER_H__

#if _MSC_VER >= 1000
#pragma once
#endif

#include "cmdlib.h"

class TimeCounter
{
public:
    void start()
    {
        start = I_FloatTime();
    }

    void stop()
    {
        double stop = I_FloatTime();
        accum += stop - start;
    }

    double getTotal() const
    {
        return accum;
    }

    void reset()
    {
        memset(this, 0, sizeof(*this));
    }

// Construction
public:
    TimeCounter()
    {
        reset();
    }
    // Default Destructor ok
    // Default Copy Constructor ok
    // Default Copy Operator ok

protected:
    double start;
    double accum;
};

#endif//TIMECOUNTER_H__
