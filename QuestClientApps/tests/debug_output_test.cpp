#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include <Utils/DebugOutput.h>

struct LogEntry {
    int priority;
    std::string message;
};

static std::vector<LogEntry> entries;

extern "C" int __android_log_write(int priority, const char*, const char* message) {
    entries.push_back({priority, message});
    return 1;
}

static void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

static void testLongLine() {
    AndroidStreambuf buffer;
    std::ostream output(&buffer);
    const std::string message(400, 'x');
    output << message << '\n' << std::flush;
    expect(output.good(), "logging must not set the output stream's failure state");
    expect(entries.size() == 1 && entries[0].message == message,
           "long messages must reach logcat intact");
}

static void testMultiline() {
    AndroidStreambuf buffer;
    std::ostream output(&buffer);
    output << "first\nsecond\nthird\n" << std::flush;
    expect(entries.size() == 3, "each complete line must reach logcat");
    expect(entries[0].message == "first" && entries[1].message == "second" &&
           entries[2].message == "third", "line boundaries must be preserved");
}

static void testFlush() {
    AndroidStreambuf buffer;
    std::ostream output(&buffer);
    output << "partial" << std::flush;
    expect(entries.size() == 1 && entries[0].message == "partial",
           "explicit flush must emit a line without a trailing newline");
    expect(output.good(), "a successful flush must keep the stream usable");
}

static void testTeardown() {
    auto* previousCout = std::cout.rdbuf();
    auto* previousCerr = std::cerr.rdbuf();
    {
        DebugOutput output;
        entries.clear();
        std::cerr << "error" << std::endl;
        std::cout << "unfinished";
    }
    const bool restored = std::cout.rdbuf() == previousCout && std::cerr.rdbuf() == previousCerr;
    // Restore explicitly so a failing test can exit without dangling global buffers.
    std::cout.rdbuf(previousCout);
    std::cerr.rdbuf(previousCerr);
    expect(restored, "logger teardown must restore both original stream buffers");
    expect(entries.size() == 2, "teardown must flush the unfinished output");
    expect(entries[0].priority == ANDROID_LOG_ERROR && entries[0].message == "error",
           "stderr must retain error priority");
    expect(entries[1].priority == ANDROID_LOG_DEBUG && entries[1].message == "unfinished",
           "stdout must retain debug priority");
}

int main(int argc, char** argv) {
    try {
        expect(argc == 2, "expected a test name");
        const std::string test = argv[1];
        if (test == "long_line") testLongLine();
        else if (test == "multiline") testMultiline();
        else if (test == "flush") testFlush();
        else if (test == "teardown") testTeardown();
        else throw std::runtime_error("unknown test");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
