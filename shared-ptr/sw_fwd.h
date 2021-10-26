#pragma once

#include <exception>
#include <cassert>
#include <utility>

class BadWeakPtr : public std::exception {};

template <typename T>
class SharedPtr;

template <typename T>
class WeakPtr;

template <typename T>
class EnableSharedFromThis;

namespace details {
class Handle;

class Storage {
    friend class Handle;

    size_t handles_ = 1;

    void HandleBegin() {
        assert(handles_ > 0);
        ++handles_;
    }
    bool HandleEnd() {
        assert(handles_ > 0);
        return --handles_ == 0;
    }

public:
    virtual void BeginStrong() = 0;
    virtual void EndStrong() = 0;

    virtual size_t StrongCount() = 0;

    virtual bool Alive() = 0;

    virtual void Destroy() = 0;
    virtual ~Storage() = default;
};

class Handle {
    Storage* storage_;

    Storage* Take() {
        return std::exchange(storage_, nullptr);
    }

    void Reset() {
        Storage* s = Take();
        if (!s) {
            return;
        }
        if (s->HandleEnd()) {
            delete s;
        }
    }

public:
    Handle() : storage_(nullptr) {
    }
    explicit Handle(Storage* storage) : storage_(storage) {
    }

    Handle(Handle&& other) : storage_(other.Take()) {
    }

    Handle(Handle const& other) : storage_(other.storage_) {
        if (storage_) {
            storage_->HandleBegin();
        }
    }

    void operator=(Handle const& other) = delete;

    void operator=(Handle&& other) {
        Reset();
        storage_ = other.Take();
    }

    Storage* Get() const {
        return storage_;
    }

    operator bool() const {
        return storage_ != nullptr;
    }

    ~Handle() {
        Reset();
    }
};

class RealStorage : public Storage {
    friend class Handle;
    // the first handle is always owned by shared ptr
    size_t strong_ = 1;

public:
    virtual void BeginStrong() override {
        assert(strong_ > 0);
        ++strong_;
    }

    virtual void EndStrong() override {
        assert(strong_ > 0);
        --strong_;
        if (strong_ == 0) {
            Storage* self = this;
            self->Destroy();
        }
    }

    virtual size_t StrongCount() override {
        return strong_;
    }

    virtual bool Alive() override {
        return strong_ > 0;
    }
};

template <class T>
class IndirectStorage : public RealStorage {
    T* data_;

public:
    IndirectStorage(T* data) : data_(data) {
    }

    void Destroy() override {
        delete data_;
        data_ = nullptr;
    }

    ~IndirectStorage() {
        delete data_;
    }
};

template <class T>
class InlineStorage : public RealStorage {
    std::aligned_storage_t<sizeof(T), alignof(T)> storage_;

public:
    InlineStorage() {
    }

    template <typename... Args>
    T* Init(Args&&... args) {
        auto place = reinterpret_cast<char*>(&storage_);
        return new (place) T(std::forward<Args>(args)...);
    }

    void Destroy() override {
        reinterpret_cast<T*>(&storage_)->~T();
    }
};
}  // namespace details
