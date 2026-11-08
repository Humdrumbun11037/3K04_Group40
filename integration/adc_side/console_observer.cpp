//console observer

#include "console_observer.hpp"

ConsoleObserver::ConsoleObserver(std::streambuf* console, LineHandler onLine, std::function<void()> beforeFirst)
    : console(console), onLine(std::move(onLine)), beforeFirst(std::move(beforeFirst)) {}

void ConsoleObserver::first() {
    if (!started.exchange(true) && beforeFirst) beforeFirst();
}

void ConsoleObserver::take(char c) {
    if (c != '\n') {
        line += c;
        return;
    }
    std::string done;
    done.swap(line);
    onLine(done);
    // Each line reaches the console (or a log file) at once, even when no
    // keyboard read follows to flush it, e.g. from BH3's drawer-safety thread.
    console->pubsync();
}

int ConsoleObserver::overflow(int c) {
    if (c == traits_type::eof()) return traits_type::not_eof(c);
    first();
    std::lock_guard<std::mutex> guard(lock);
    console->sputc(static_cast<char>(c));
    take(static_cast<char>(c));
    return c;
}

std::streamsize ConsoleObserver::xsputn(const char* s, std::streamsize n) {
    first();
    std::lock_guard<std::mutex> guard(lock);
    std::streamsize written = console->sputn(s, n);
    for (std::streamsize i = 0; i < n; i++) take(s[i]);
    return written;
}

int ConsoleObserver::sync() {
    return console->pubsync();
}
