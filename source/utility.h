//
// Created by 小叶 on 2026/2/14.
//

#ifndef VULKAN_PROJECT_UTILITY_H
#define VULKAN_PROJECT_UTILITY_H

#include <functional>
#include <stack>
#include <source_location>
#include <memory>

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
            static_cast<derived*>(this)->do_cleanup();  // 清理当前资源
            static_cast<derived*>(this)->dtor_stack = std::move(other.dtor_stack);
        }
        return *this;
    }

    ~enable_destruct_stack() noexcept {
        static_cast<derived*>(this)->do_cleanup();
    }

    // 提供安全的注册接口
    template<typename F>
    void register_cleanup(F&& f) noexcept {
        static_cast<derived*>(this)->dtor_stack.push(std::forward<F>(f));
    }

    void set_entry_callback(std::function<dtor_signature>&& f) noexcept {
        static_cast<derived*>(this)->entry_stack_callback = std::make_unique<std::function<dtor_signature>>(std::move(f));
    }

    void set_end_callback(std::function<dtor_signature>&& f) noexcept {
        static_cast<derived*>(this)->end_stack_callback = std::make_unique<std::function<dtor_signature>>(std::move(f));
    }

    // 允许手动触发清理
    void cleanup() noexcept {
        static_cast<derived*>(this)->do_cleanup();
    }
    std::stack<std::function<dtor_signature>> dtor_stack;
    std::unique_ptr<std::function<dtor_signature>> entry_stack_callback;
    std::unique_ptr<std::function<dtor_signature>> end_stack_callback;

    void do_cleanup() noexcept {

        if (static_cast<derived*>(this)->entry_stack_callback)
            (*static_cast<derived*>(this)->entry_stack_callback)();

        while (!dtor_stack.empty()) {
            static_cast<derived*>(this)->dtor_stack.top()();
            static_cast<derived*>(this)->dtor_stack.pop();
        }

        if (static_cast<derived*>(this)->end_stack_callback)
            (*static_cast<derived*>(this)->end_stack_callback)();
    }
};


#endif //VULKAN_PROJECT_UTILITY_H`