//
// Created by 小叶 on 2026/2/14.
//

#include <print>
#define BOOST_STACKTRACE_USE_WINDBUG
#include <boost/stacktrace/stacktrace.hpp>
#include "utility.h"

#include <exception>

[[noreturn]] void print_stacktrace_and_terminate(const std::source_location &location) {

    std::println(
        stderr,
        "terminate occurred in \nfile [{}] \nline [{}] \nfuncton [{}]",
        location.file_name(), location.line(), location.function_name()
        );

#ifdef _DEBUG
    std::print(stderr, "this is the stacktrace \n{}",
        boost::stacktrace::to_string(boost::stacktrace::stacktrace())
        );
#endif

    std::terminate();
}
