//
// Created by 小叶 on 2026/5/18.
//

#ifndef VULKAN_PROJECT_VMA_WAITER_H
#define VULKAN_PROJECT_VMA_WAITER_H

#include <functional>
#include <future>
#include <source_location>

class vma_waiter {
    std::function<void()> wait_fn;
    mutable std::future<void> wait_future;
    std::source_location location = std::source_location::current();
    bool waited = false;
    mutable std::once_flag once;

    void do_wait() noexcept;
    void do_destroy() const noexcept;

public:
    vma_waiter(std::future<void>&& wait, std::function<void()>&& wait_fn);

    vma_waiter(const vma_waiter &) = delete;

    vma_waiter &operator=(const vma_waiter &) = delete;

    vma_waiter(vma_waiter &&other) noexcept;

    vma_waiter &operator=(vma_waiter &&other) noexcept;

    ~vma_waiter();

    void wait();

};


#endif //VULKAN_PROJECT_VMA_WAITER_H
