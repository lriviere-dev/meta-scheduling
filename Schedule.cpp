#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "Instance.h"
#include "Schedule.h"


Schedule::Schedule(std::vector<int> startTimesArray) : startTimes(startTimesArray) {}

// Maybe there should be an input for evaluation type sigma gamma. 
// For now implemented type is max sumci.
int Schedule::evaluate(DataInstance& instance) {
    // A schedule is evaluated for an instance with a single scenario
    // feasibility (precedences, overlap, release dates) is assumed
    
    if (instance.S != 1) {
        throw std::invalid_argument("The number of Scenario should be equal to 1 (S=1)");
    }
    if (instance.N != startTimes.size()) {
        std::cout << "N = :" << instance.N << "nb tasks = :" << startTimes.size() << std::endl;
        throw std::invalid_argument("The number of start times should equal the number of tasks (N)");
    }

    // for each task, add up end time 
    int sumci = 0;
    for (int i = 0; i < instance.N; ++i) {
        //int lateness = startTimes[i]+instance.durations[i]-instance.dueDates[i];
        int end_time = startTimes[i]+instance.durations[i];
        sumci += end_time;
        
    }
    return sumci;
}

void Schedule::print() {
    std::cout << "Schedule:" << std::endl;
    for (int i = 0; i < startTimes.size(); ++i) {
        std::cout << "Task " << i << " : " << startTimes[i] << std::endl;
    }
}

