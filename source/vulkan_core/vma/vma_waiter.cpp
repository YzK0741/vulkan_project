//
// Created by 小叶 on 2026/5/18.
//

#include <print>
#include "vma_waiter.h"

vma_waiter::vma_waiter(std::future<void> &&wait, std::function<void()> &&wait_fn) {
    this->wait_fn = std::move(wait_fn);
    this->wait_future = std::move(wait);
}

vma_waiter::vma_waiter(vma_waiter &&other)  noexcept {
    this->wait_future = std::move(other.wait_future);
    this->wait_fn = std::move(other.wait_fn);
    this->location = other.location;
    this->waited = false;
    other.waited = true;
}

vma_waiter &vma_waiter::operator=(vma_waiter &&other) noexcept {
    if (this != &other) {
        std::call_once(once, [this] { do_destroy(); });

        if (other.wait_future.valid() && !other.waited) {
            this->wait_future = std::move(other.wait_future);
            this->wait_fn = std::move(other.wait_fn);
            this->location = other.location;
            this->waited = false;
            other.waited = true;
        }
    }
    return *this;
}

vma_waiter::~vma_waiter() {
    std::call_once(this->once, [this]{this->do_destroy();});
}

void vma_waiter::wait() {
    std::call_once(this->once, [this]{this->do_wait();});
}

void vma_waiter::do_wait() noexcept {
    if (!this->waited && this->wait_future.valid()) {
        this->wait_future.wait();
        if (this->wait_fn)
            this->wait_fn();
        this->waited = true;
    }
}

void vma_waiter::do_destroy() const noexcept{
    if (!this->waited && this->wait_future.valid()) {
        std::println(stderr, "unwaited until destroy");
        std::println(stderr,
            "this waiter is created in file[{}] line [{}] functon [{}]",
            this->location.file_name(),
            this->location.line(),
            this->location.function_name()
            );
        this->wait_future.wait();
        if (this->wait_fn)
            this->wait_fn();
    }
}
