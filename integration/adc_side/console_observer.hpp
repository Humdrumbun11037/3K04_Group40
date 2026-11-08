//console observer

/*
Hides: how the ADC's console output is read without changing the ADC: a
       stream buffer put in front of std::cout that passes every character
       to the real console unchanged and hands each complete line on

Interface
takes: everything Subgroup A's code writes to std::cout (any thread)
sends: the same text to the console; each complete line (no newline) to the
       callback, one line at a time
uses: nothing
*/
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <streambuf>
#include <string>

class ConsoleObserver : public std::streambuf {
public:
    using LineHandler = std::function<void(const std::string&)>;

    // beforeFirst (optional) runs once, just before the first character is
    // passed on: by then the program's main() is running.
    ConsoleObserver(std::streambuf* console, LineHandler onLine, std::function<void()> beforeFirst = {});

protected:
    int overflow(int c) override;
    std::streamsize xsputn(const char* s, std::streamsize n) override;
    int sync() override;

private:
    void take(char c);
    void first();

    std::streambuf* console;
    LineHandler onLine;
    std::function<void()> beforeFirst;
    std::atomic<bool> started{false};
    std::string line;
    std::mutex lock;   // BH3's drawer-safety thread prints too
};
