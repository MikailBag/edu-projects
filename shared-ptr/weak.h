#pragma once

#include "sw_fwd.h"  // Forward declaration
// https://en.cppreference.com/w/cpp/memory/weak_ptr
template <typename T>
class WeakPtr {
    template <class U>
    friend class WeakPtr;

    // template <>
    friend class EnableSharedFromThis<T>;

    details::Handle handle_;
    T* ptr_ = nullptr;

public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Constructors
    WeakPtr() {
    }

    WeakPtr(WeakPtr const& other) : handle_(other.handle_), ptr_(other.ptr_) {
    }
    template <class U>
    WeakPtr(WeakPtr<U> const& other) requires(std::is_convertible_v<U*, T*>)
        : handle_(other.handle_), ptr_(other.ptr_) {
    }
    WeakPtr(WeakPtr&& other) : handle_(std::move(other.handle_)), ptr_(other.ptr_) {
    }

    // Demote `SharedPtr`
    // #2 from https://en.cppreference.com/w/cpp/memory/weak_ptr/weak_ptr
    WeakPtr(const SharedPtr<T>& other) : handle_(other.handle_), ptr_(other.ptr_) {
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // `operator=`-s

    WeakPtr& operator=(const WeakPtr& other) {
        handle_ = details::Handle(other.handle_);
        ptr_ = other.ptr_;
        return *this;
    }
    WeakPtr& operator=(WeakPtr&& other) {
        handle_ = std::move(other.handle_);
        ptr_ = other.ptr_;
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Destructor

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Modifiers

    void Reset() {
        handle_ = details::Handle();
        //ptr_ = nullptr;
    }

    void Swap(WeakPtr& other) {
        std::swap(handle_, other.handle_);
        std::swap(ptr_, other.ptr_);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Observers

    size_t UseCount() const {
        if (!handle_) {
            return 0;
        }
        return handle_.Get()->StrongCount();
    }
    bool Expired() const {
        if (!handle_) {
            return true;
        }
        return !handle_.Get()->Alive();
    }

    bool TryLockInto(SharedPtr<T>* target) const {
        if (Expired()) {
            return false;
        }
        target->Adopt(details::Handle(handle_), ptr_);
        return true;
    }

    SharedPtr<T> Lock() const {
        SharedPtr<T> sp;
        TryLockInto(&sp);
        return sp;
    }
};
