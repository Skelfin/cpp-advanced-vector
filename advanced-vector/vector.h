#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <algorithm>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory&) = delete;

    RawMemory(RawMemory&& other) noexcept
        : buffer_(other.buffer_)
        , capacity_(other.capacity_) {
        other.buffer_ = nullptr;
        other.capacity_ = 0;
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other) {
            Deallocate(buffer_);
            buffer_ = other.buffer_;
            capacity_ = other.capacity_;
            other.buffer_ = nullptr;
            other.capacity_ = 0;
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return buffer_ + offset;
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() noexcept = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_) {
        other.size_ = 0;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector tmp(rhs);
                Swap(tmp);
            }
            else {
                size_t i = 0;
                try {
                    for (; i < size_ && i < rhs.size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    for (; i < rhs.size_; ++i) {
                        new (data_ + i) T(rhs.data_[i]);
                    }
                    for (size_t j = rhs.size_; j < size_; ++j) {
                        data_[j].~T();
                    }
                    size_ = rhs.size_;
                }
                catch (...) {
                    for (size_t j = i; j < rhs.size_; ++j) {
                        data_[j].~T();
                    }
                    throw;
                }
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_.Swap(rhs.data_);
            std::swap(size_, rhs.size_);
            rhs.size_ = 0;
        }
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity > data_.Capacity()) {
            RawMemory<T> new_data(new_capacity);
            MoveOrCopyElements(data_.GetAddress(), size_, new_data.GetAddress());
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
            size_ = new_size;
        }
        else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
            size_ = new_size;
        }
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return data_[size_ - 1];
        }
        else {
            return *Emplace(end(), std::forward<Args>(args)...);
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        --size_;
        data_[size_].~T();
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < size_);
        return data_[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        size_t index = pos - cbegin();
        if (size_ < data_.Capacity()) {
            InsertWithoutReallocation(index, std::forward<Args>(args)...);
        }
        else {
            InsertWithReallocation(index, std::forward<Args>(args)...);
        }
        return data_.GetAddress() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        if (IsSelfReference(&value)) {
            T tmp = value;
            return Emplace(pos, std::move(tmp));
        }
        else {
            return Emplace(pos, value);
        }
    }

    iterator Insert(const_iterator pos, T&& value) {
        if (IsSelfReference(&value)) {
            T tmp = std::move(value);
            return Emplace(pos, std::move(tmp));
        }
        else {
            return Emplace(pos, std::move(value));
        }
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos >= cbegin() && pos < cend());
        size_t index = pos - cbegin();
        iterator non_const_pos = data_.GetAddress() + index;
        std::move(non_const_pos + 1, data_.GetAddress() + size_, non_const_pos);
        --size_;
        data_[size_].~T();
        return non_const_pos;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    bool IsSelfReference(const T* ptr) const noexcept {
        return ptr >= data_.GetAddress() && ptr < data_.GetAddress() + size_;
    }

    void MoveOrCopyElements(T* from, size_t count, T* to) {
        size_t i = 0;
        try {
            if constexpr (IsNothrowMoveConstructibleOrNoCopy<T>()) {
                for (; i < count; ++i) {
                    new (to + i) T(std::move(from[i]));
                }
            }
            else {
                for (; i < count; ++i) {
                    new (to + i) T(from[i]);
                }
            }
        }
        catch (...) {
            for (size_t j = 0; j < i; ++j) {
                (to + j)->~T();
            }
            throw;
        }
    }

    template <typename... Args>
    void InsertWithoutReallocation(size_t index, Args&&... args) {
        T* insert_pos = data_.GetAddress() + index;
        if (index == size_) {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        else {
            new (data_ + size_) T(std::move(data_[size_ - 1]));
            std::move_backward(insert_pos, data_.GetAddress() + size_ - 1, data_.GetAddress() + size_);
            data_[index] = T(std::forward<Args>(args)...);
        }
        ++size_;
    }

    template <typename... Args>
    void InsertWithReallocation(size_t index, Args&&... args) {
        size_t new_capacity = data_.Capacity() == 0 ? 1 : data_.Capacity() * 2;
        RawMemory<T> new_data(new_capacity);
        T* new_ptr = new_data.GetAddress();
        size_t new_size = 0;
        try {
            MoveOrCopyElements(data_.GetAddress(), index, new_ptr);
            new_size = index;
            new (new_ptr + new_size) T(std::forward<Args>(args)...);
            ++new_size;
            MoveOrCopyElements(data_.GetAddress() + index, size_ - index, new_ptr + new_size);
            new_size += size_ - index;
        }
        catch (...) {
            for (size_t i = 0; i < new_size; ++i) {
                (new_ptr + i)->~T();
            }
            throw;
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
        size_ = new_size;
    }

    template <typename U>
    static constexpr bool IsNothrowMoveConstructibleOrNoCopy() {
        return std::is_nothrow_move_constructible_v<U> || !std::is_copy_constructible_v<U>;
    }
};
