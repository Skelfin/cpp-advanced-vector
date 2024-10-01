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
        return buffer_[index];
    }

    T& operator[](size_t index) noexcept {
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
            size_t i = 0;
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    for (; i < size_; ++i) {
                        new (new_data + i) T(std::move(data_[i]));
                    }
                }
                else {
                    for (; i < size_; ++i) {
                        new (new_data + i) T(data_[i]);
                    }
                }
            }
            catch (...) {
                for (size_t j = 0; j < i; ++j) {
                    (new_data + j)->~T();
                }
                throw;
            }
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
            if (new_size > data_.Capacity()) {
                Reserve(new_size);
            }
            size_t count = new_size - size_;
            try {
                std::uninitialized_value_construct_n(data_ + size_, count);
                size_ = new_size;
            }
            catch (...) {
                std::destroy_n(data_ + size_, count);
                throw;
            }
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
            size_t new_capacity = data_.Capacity() == 0 ? 1 : data_.Capacity() * 2;
            RawMemory<T> new_data(new_capacity);
            size_t new_size = 0;
            try {
                new (new_data + size_) T(std::forward<Args>(args)...);
                ++new_size;
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    for (size_t i = 0; i < size_; ++i, ++new_size) {
                        new (new_data + i) T(std::move(data_[i]));
                    }
                }
                else {
                    for (size_t i = 0; i < size_; ++i, ++new_size) {
                        new (new_data + i) T(data_[i]);
                    }
                }
            }
            catch (...) {
                for (size_t i = 0; i < new_size; ++i) {
                    (new_data + i)->~T();
                }
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
            return data_[size_ - 1];
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(value);
    }

    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }

    void PopBack() noexcept {
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
        return data_[index];
    }

    T& operator[](size_t index) noexcept {
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
        iterator insert_pos = data_.GetAddress() + index;
        if (size_ < data_.Capacity()) {
            if (pos == cend()) {
                new (data_ + size_) T(std::forward<Args>(args)...);
                ++size_;
                return data_.GetAddress() + index;
            }
            else {
                new (data_ + size_) T(std::move(*(data_ + size_ - 1)));
                ++size_;
                std::move_backward(insert_pos, data_.GetAddress() + size_ - 2, data_.GetAddress() + size_ - 1);
                try {
                    *insert_pos = T(std::forward<Args>(args)...);
                }
                catch (...) {
                    std::move(insert_pos + 1, data_.GetAddress() + size_, insert_pos);
                    (data_ + size_ - 1)->~T();
                    --size_;
                    throw;
                }
                return insert_pos;
            }
        }
        else {
            size_t new_capacity = data_.Capacity() == 0 ? 1 : data_.Capacity() * 2;
            RawMemory<T> new_data(new_capacity);
            size_t new_size = 0;
            try {
                for (; new_size < index; ++new_size) {
                    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                        new (new_data + new_size) T(std::move(data_[new_size]));
                    }
                    else {
                        new (new_data + new_size) T(data_[new_size]);
                    }
                }
                new (new_data + new_size) T(std::forward<Args>(args)...);
                ++new_size;
                for (size_t i = index; i < size_; ++i, ++new_size) {
                    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                        new (new_data + new_size) T(std::move(data_[i]));
                    }
                    else {
                        new (new_data + new_size) T(data_[i]);
                    }
                }
            }
            catch (...) {
                for (size_t i = 0; i < new_size; ++i) {
                    (new_data + i)->~T();
                }
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            ++size_;
            return data_.GetAddress() + index;
        }
    }

    iterator Insert(const_iterator pos, const T& value) {
        if (std::addressof(value) >= data_.GetAddress() && std::addressof(value) < data_.GetAddress() + size_) {
            T tmp = value;
            return Emplace(pos, std::move(tmp));
        }
        else {
            return Emplace(pos, value);
        }
    }

    iterator Insert(const_iterator pos, T&& value) {
        if (std::addressof(value) >= data_.GetAddress() && std::addressof(value) < data_.GetAddress() + size_) {
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
};
