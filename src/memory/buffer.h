// NOVA GPU - Software GPU
// src/memory/buffer.h - GPU-style typed buffer.
//
// A deliberately thin wrapper over std::vector so that vertex/index/constant
// storage has one named type the pipeline can talk about. Phase 5 can swap the
// backing store for an aligned/pooled allocator without touching call sites.
#pragma once

#include <cstddef>
#include <initializer_list>
#include <vector>

namespace nova::memory {

template <typename T>
class Buffer {
public:
    Buffer() = default;
    explicit Buffer(size_t count) : data_(count) {}
    Buffer(std::initializer_list<T> init) : data_(init) {}
    explicit Buffer(std::vector<T> data) : data_(std::move(data)) {}

    size_t size() const { return data_.size(); }
    bool empty() const { return data_.empty(); }
    size_t bytes() const { return data_.size() * sizeof(T); }

    T* data() { return data_.data(); }
    const T* data() const { return data_.data(); }

    T& operator[](size_t i) { return data_[i]; }
    const T& operator[](size_t i) const { return data_[i]; }

    T& back() { return data_.back(); }
    const T& back() const { return data_.back(); }

    void push_back(const T& value) { data_.push_back(value); }
    void resize(size_t count) { data_.resize(count); }
    void reserve(size_t count) { data_.reserve(count); }
    void clear() { data_.clear(); }

    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

private:
    std::vector<T> data_;
};

}  // namespace nova::memory
