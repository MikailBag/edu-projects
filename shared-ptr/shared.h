#pragma once

#include "sw_fwd.h"  // Forward declaration

#include <cstddef>  // std::nullptr_t
#include <cassert>
#include <utility>

class EnableSharedFromThisBase {
protected:
    template <class U>
    friend class SharedPtr;

    virtual void Initialize(details::Handle&& handle, void* ptr) const = 0;

public:
    virtual ~EnableSharedFromThisBase() = default;
};

// Look for usage examples in tests
template <typename T>
class EnableSharedFromThis : public EnableSharedFromThisBase {
    mutable WeakPtr<T> me_;

protected:
    void Initialize(details::Handle&& handle, void* ptr) const override {
        me_.handle_ = std::move(handle);
        me_.ptr_ = static_cast<T*>(ptr);
    }

public:
    SharedPtr<T> SharedFromThis() {
        return SharedPtr(me_);
    }
    SharedPtr<T const> SharedFromThis() const {
        return SharedPtr(me_);
    }

    WeakPtr<T> WeakFromThis() noexcept {
        return me_;
    }
    WeakPtr<T const> WeakFromThis() const noexcept {
        return me_;
    }
};

// https://en.cppreference.com/w/cpp/memory/shared_ptr
template <class T>
class SharedPtr {
    template <class U>
    friend class SharedPtr;
    template <class U>
    friend class WeakPtr;

    details::Handle handle_;
    T* ptr_ = nullptr;

    void Adopt(details::Handle&& handle, T* ptr) {
        handle_ = std::move(handle);
        if (handle_) {
            handle_.Get()->BeginStrong();
        }
        ptr_ = ptr;
    }

    void Steal(details::Handle&& handle, T* ptr) {
        handle_ = std::move(handle);
        ptr_ = ptr;
    }

    void SharedFromThisSetupHook() {
        if constexpr (std::is_convertible_v<T*, EnableSharedFromThisBase const*>) {
            auto base = static_cast<EnableSharedFromThisBase const*>(ptr_);
            base->Initialize(details::Handle(handle_), ptr_);
        }
    }

public:
    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Constructors

    SharedPtr() {
    }
    SharedPtr(std::nullptr_t) {
    }
    explicit SharedPtr(T* ptr) {
        Steal(details::Handle(new details::IndirectStorage<T>(ptr)), ptr);
        SharedFromThisSetupHook();
    }

    template <typename... Args>
    static SharedPtr MakeShared(Args&&... args) {
        auto s = new details::InlineStorage<T>();
        T* ptr = s->Init(std::forward<Args>(args)...);
        SharedPtr sp;
        sp.Steal(details::Handle(s), ptr);
        sp.SharedFromThisSetupHook();
        return sp;
    }

    SharedPtr(SharedPtr const& other) {
        Adopt(details::Handle(other.handle_), other.ptr_);
    }
    SharedPtr(SharedPtr&& other) {
        Steal(std::move(other.handle_), other.ptr_);
    }

    // Aliasing constructor
    // #8 from https://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    template <class U>
    SharedPtr(SharedPtr<U>&& other, T* ptr) {
        Steal(std::move(other.handle_), ptr);
    }

    template <class U>
    SharedPtr(SharedPtr<U> const& other, T* ptr) {
        Adopt(details::Handle(other.handle_), ptr);
    }

    template <class U>
    SharedPtr(SharedPtr<U> const& other) requires(std::is_convertible_v<U*, T*>)
        : SharedPtr(other, other.Get()) {
    }

    template <class U>
    SharedPtr(SharedPtr<U>&& other) requires(std::is_convertible_v<U*, T*>)
        : SharedPtr(std::move(other), other.Get()) {
    }

    template <class U>
    explicit SharedPtr(U* ptr) : SharedPtr(SharedPtr<U>(ptr)) {
    }

    // Promote `WeakPtr`
    // #11 from https://en.cppreference.com/w/cpp/memory/shared_ptr/shared_ptr
    explicit SharedPtr(const WeakPtr<T>& weak) : SharedPtr() {
        if (!weak.TryLockInto(this)) {
            throw BadWeakPtr();
        }
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // `operator=`-s

    SharedPtr& operator=(const SharedPtr& other) {
        Reset();
        Adopt(details::Handle(other.handle_), other.ptr_);
        return *this;
    }
    SharedPtr& operator=(SharedPtr&& other) {
        Reset();
        Steal(std::move(other.handle_), other.ptr_);
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Destructor

    ~SharedPtr() {
        Reset();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Modifiers

    void Reset() {
        if (!handle_) {
            return;
        }
        handle_.Get()->EndStrong();
        handle_ = details::Handle();
    }
    /*void Reset(T* ptr) {
        Reset();
        if (ptr) {
            Steal(details::Handle(new details::IndirectStorage<T>(ptr)));
        }
    }*/
    template <class U>
    void Reset(U* ptr) requires(std::is_convertible_v<U*, T*>) {
        Reset();
        if (ptr) {
            Steal(details::Handle(new details::IndirectStorage<U>(ptr)), ptr);
            SharedFromThisSetupHook();
        }
    }
    void Swap(SharedPtr& other) {
        std::swap(handle_, other.handle_);
        std::swap(ptr_, other.ptr_);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    // Observers

    T* Get() const {
        if (!handle_) {
            return nullptr;
        }
        return ptr_;
    }
    T& operator*() const {
        return *ptr_;
    }
    T* operator->() const {
        return ptr_;
    }
    size_t UseCount() const {
        if (!handle_) {
            return 0;
        }
        return handle_.Get()->StrongCount();
    }
    explicit operator bool() const {
        return handle_;
    }
};

template <typename T, typename U>
inline bool operator==(const SharedPtr<T>& left, const SharedPtr<U>& right) {
    return left.Get() == right.Get();
}

// Allocate memory only once
template <typename T, typename... Args>
SharedPtr<T> MakeShared(Args&&... args) {
    return SharedPtr<T>::MakeShared(std::forward<Args>(args)...);
}
