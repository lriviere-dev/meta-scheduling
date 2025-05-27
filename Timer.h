#ifndef TIMER_H
#define TIMER_H


#include <iostream>
#include <chrono>

//helper timer class. automatically displays time when timer goes out of scope
class Timer {
    std::string label;
    std::chrono::high_resolution_clock::time_point start;
public:
    Timer(std::string lbl) : label(std::move(lbl)), start(std::chrono::high_resolution_clock::now()) {}
    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        std::cout << label << ": " << duration.count() << " seconds\n";
    }
};

int timer_test() {
    {
        Timer t("Loop 1");
        for (volatile int i = 0; i < 1000000; ++i);
    }

    {
        Timer t("Loop 2");
        for (volatile int i = 0; i < 5000000; ++i);
    }

    return 0;
}

#endif // TIMER_H
