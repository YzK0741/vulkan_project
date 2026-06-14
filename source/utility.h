//
// Created by 小叶 on 2026/2/14.
//

#ifndef VULKAN_PROJECT_UTILITY_H
#define VULKAN_PROJECT_UTILITY_H

#include <algorithm>
#include <expected>
#include <functional>
#include <stack>
#include <source_location>
#include <memory>
#include <string_view>
#include <mutex>

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
            this->do_cleanup();  // 清理当前资源
            this->dtor_stack = std::move(other.dtor_stack);
        }
        return *this;
    }

    ~enable_destruct_stack() noexcept {
        this->do_cleanup();
    }

    // 提供安全的注册接口
    template<typename F>
    void register_cleanup(F&& f) noexcept {
        this->dtor_stack.push(std::forward<F>(f));
    }

    void set_entry_callback(std::function<dtor_signature>&& f) noexcept {
        this->entry_stack_callback = std::make_unique<std::function<dtor_signature>>(std::move(f));
    }

    void set_end_callback(std::function<dtor_signature>&& f) noexcept {
        this->end_stack_callback = std::make_unique<std::function<dtor_signature>>(std::move(f));
    }

    // 允许手动触发清理
    void cleanup() noexcept {
        static_cast<derived*>(this)->do_cleanup();
    }
    std::stack<std::function<dtor_signature>> dtor_stack;
    std::unique_ptr<std::function<dtor_signature>> entry_stack_callback;
    std::unique_ptr<std::function<dtor_signature>> end_stack_callback;

    void do_cleanup() noexcept {

        if (this->entry_stack_callback)
            (*this->entry_stack_callback)();

        while (!dtor_stack.empty()) {
            this->dtor_stack.top()();
            dtor_stack.pop();
        }

        if (this->end_stack_callback)
           (*this->end_stack_callback)();
    }
};

template<typename T>
concept handler_type = std::integral<T> &&
    requires {
    { std::numeric_limits<T>::min() } -> std::convertible_to<long long>;
    { std::numeric_limits<T>::max() } -> std::convertible_to<long long>;
    requires std::numeric_limits<T>::min() < 1;
    requires std::numeric_limits<T>::max() >= INT32_MAX;
    };

template <typename derived, handler_type Ht = long, Ht lowest = 1000, Ht biggest = INT32_MAX>
class enable_handler_distribute {
private:
    std::vector<Ht> recycled_handlers;
    Ht handler_up_bound = lowest - 1;
    mutable std::mutex distribute_mutex;

    bool is_valid_handler_no_lock(const Ht& handler) const noexcept  {
        if (handler <= this->handler_up_bound &&
            lowest <= handler &&
            std::ranges::find(this->recycled_handlers, handler) == this->recycled_handlers.end()) {
            return true;
            }
        return false;
    }

protected:

    using handler = Ht;

    enable_handler_distribute() {
        static_assert(lowest > std::numeric_limits<Ht>::min());
        static_assert(lowest <= biggest, "lowest must not exceed biggest");
        recycled_handlers.reserve(1024);
    }

    std::expected<Ht, std::string_view> distribute_handler() noexcept {
        std::unique_lock lock(this->distribute_mutex);
        if (!this->recycled_handlers.empty()) {
            Ht handler = this->recycled_handlers.back();
            this->recycled_handlers.pop_back();
            return handler;
        }
        if (this->handler_up_bound < biggest) {
            ++this->handler_up_bound;
            return this->handler_up_bound;
        }
        return std::unexpected("all handlers had distributed");
    }

    std::expected<void, std::string_view> recycle_handler(const Ht& handler) noexcept {
        std::unique_lock lock(this->distribute_mutex);
        if (this->is_valid_handler_no_lock(handler))  {
            this->recycled_handlers.push_back(handler);
            return {};
        }

        return std::unexpected("invalid handler");
    }

    bool is_valid_handler(const Ht& handler) const noexcept {
        std::unique_lock lock(this->distribute_mutex);
        return this->is_valid_handler_no_lock(handler);
    }

    void reset() noexcept {
        std::unique_lock lock(this->distribute_mutex);
        this->recycled_handlers.clear();
        this->handler_up_bound = lowest - 1;
    }
};

#endif //VULKAN_PROJECT_UTILITY_H`