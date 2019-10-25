#include <io_context.h>
#include <stdio.h>
#include <timer.h>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

int main()
{
    io_context io;

    {
        timer t(&io);
        t.set(
            2, []() { std::cout << "hello" << std::endl; }, true);
    }

    timer t(&io);
    t.set( 3, []() { std::cout << "world" << std::endl; }, true);
    io.run();
    return 0;
}
