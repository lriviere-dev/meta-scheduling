#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "Instance.h"  // Include other necessary headers

class Schedule {
public:
    std::vector<int> startTimes;

    explicit Schedule(std::vector<int> startTimesArray);
    int evaluate(const DataInstance& instance);
    void print();
};

#endif // SCHEDULE_H