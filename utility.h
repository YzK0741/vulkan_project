//
// Created by 小叶 on 2026/2/14.
//

#ifndef VULKAN_PROJECT_UTILITY_H
#define VULKAN_PROJECT_UTILITY_H

#include <functional>
#include <stack>

[[noreturn]] void print_stacktrace_and_terminate();

template <typename derived, typename dtor_signature = void()>
class enabled_destruct_stack {
public:
    // 禁用拷贝
    enabled_destruct_stack(const enabled_destruct_stack&) = delete;
    enabled_destruct_stack& operator=(const enabled_destruct_stack&) = delete;
protected:
    enabled_destruct_stack() = default;

    // 正确的移动构造
    enabled_destruct_stack(enabled_destruct_stack&& other) noexcept
        : dtor_stack(std::move(other.dtor_stack)) {
    }

    // 移动赋值
    enabled_destruct_stack& operator=(enabled_destruct_stack&& other) noexcept {
        if (this != &other) {
            do_cleanup();  // 清理当前资源
            dtor_stack = std::move(other.dtor_stack);
        }
        return *this;
    }

    ~enabled_destruct_stack() noexcept {
        do_cleanup();
    }

    // 提供安全的注册接口
    template<typename F>
    void register_cleanup(F&& f) noexcept {
        dtor_stack.push(std::forward<F>(f));
    }

    // 允许手动触发清理
    void cleanup() noexcept {
        do_cleanup();
    }

private:
    std::stack<std::function<dtor_signature>> dtor_stack;

    void do_cleanup() noexcept {
        while (!dtor_stack.empty()) {
            dtor_stack.top()();
            dtor_stack.pop();
        }
    }
};


#endif //VULKAN_PROJECT_UTILITY_H