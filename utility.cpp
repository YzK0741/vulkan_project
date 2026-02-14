//
// Created by 小叶 on 2026/2/14.
//

#include <print>
#include <sstream>
#include <boost/stacktrace/stacktrace.hpp>
#include "utility.h"

#include <exception>

[[noreturn]] void print_stacktrace_and_terminate() {
    std::stringstream ss;
    ss << "this is the stacktrace \n" << boost::stacktrace::stacktrace();
    std::print("{}", ss.str());
    std::terminate();
}
