//
// Created by 小叶 on 2026/2/14.
//

#ifndef VULKAN_PROJECT_UTILITY_H
#define VULKAN_PROJECT_UTILITY_H

#include <functional>
#include <stack>
#include <source_location>

[[noreturn]] void print_stacktrace_and_terminate(const std::source_location &location = std::source_location::current());


using dtor_signature = void();

template <typename derived, template<typename> typename dtor_container = std::function>
class enable_destruct_stack {
public:
    // 禁用拷贝
    enable_destruct_stack(const enable_destruct_stack&) = delete;
    enable_destruct_stack& operator=(const enable_destruct_stack&) = delete;
protected:
    using dtor_type = dtor_container<dtor_signature>;
    enable_destruct_stack() = default;

    // 正确的移动构造
    enable_destruct_stack(enable_destruct_stack&& other) noexcept
        : dtor_stack(std::move(other.dtor_stack)) {
    }

    // 移动赋值
    enable_destruct_stack& operator=(enable_destruct_stack&& other) noexcept {
        if (this != &other) {
            do_cleanup();  // 清理当前资源
            dtor_stack = std::move(other.dtor_stack);
        }
        return *this;
    }

    ~enable_destruct_stack() noexcept {
        do_cleanup();
    }

    // 提供安全的注册接口
    template<typename F>
    void register_cleanup(F&& f) noexcept {
        dtor_stack.push(std::forward<F>(f));
    }

    void set_entry_callback(std::function<dtor_signature>&& f) noexcept {
        entry_stack_callback = std::make_unique<std::function<dtor_signature>>(std::move(f));
    }

    void set_end_callback(std::function<dtor_signature>&& f) noexcept {
        end_stack_callback = std::make_unique<std::function<dtor_signature>>(std::move(f));
    }

    // 允许手动触发清理
    void cleanup() noexcept {
        do_cleanup();
    }
    std::stack<std::function<dtor_signature>> dtor_stack;
    std::unique_ptr<std::function<dtor_signature>> entry_stack_callback;
    std::unique_ptr<std::function<dtor_signature>> end_stack_callback;

    void do_cleanup() noexcept {

        if (entry_stack_callback)
            (*entry_stack_callback)();

        while (!dtor_stack.empty()) {
            dtor_stack.top()();
            dtor_stack.pop();
        }

        if (end_stack_callback)
            (*end_stack_callback)();
    }
};


#endif //VULKAN_PROJECT_UTILITY_H`