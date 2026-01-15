#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include "Instance.h"
#include "Schedule.h"

//when handling other problem, might have to generalize to "Solutions" of which schedule is a type that is composed of start times.
//All solutions can be evaluated though

Schedule::Schedule(std::vector<int> startTimesArray) : startTimes(startTimesArray) {}

// Maybe there should be an input for evaluation type gamma. 
// For now implemented type is sumci.
// note that feasibility is not checked.
int Schedule::evaluate(const DataInstance& instance) {
    // A schedule is evaluated for an instance (scenario is irrelevant, feasibility isn't checked)
    // feasibility (precedences, overlap, release dates) is assumed
    if (instance.type == InstanceType::SINGLE_MACHINE) {
        const SingleMachineInstance& sm_instance = static_cast<const SingleMachineInstance&>(instance); //static cast (fast)
        // for each task, add up end time 
        int sumci = 0;
        for (int i = 0; i < sm_instance.N; ++i) {
            //int lateness = startTimes[i]+instance.durations[i]-instance.dueDates[i];
            sumci +=  startTimes[i]+sm_instance.durations[i]; //adding end time of task       
        }
        return sumci;
    }
    else if (instance.type == InstanceType::RCPSP)
    {
        const RCPSPInstance& rcpsp_instance = static_cast<const RCPSPInstance&>(instance);
        // for each task, add up end time 
        int sumci = 0;
        for (int i = 0; i < rcpsp_instance.N; ++i) {
            //int lateness = startTimes[i]+instance.durations[i]-instance.dueDates[i];
            //int makespan = std::max(makespan, startTimes[i]+instance.durations[i]);
            sumci +=  startTimes[i]+rcpsp_instance.durations[i]; //adding end time of task       
        }
        return sumci;
    }
    
    else {
        throw std::runtime_error("Schedule evaluation not implemented for this instance type.");
    }
}

void Schedule::print() {
    std::cout << "Schedule:" << std::endl;
    for (size_t i = 0; i < startTimes.size(); ++i) {
        std::cout << "Task " << i << " : " << startTimes[i] << std::endl;
    }
}

