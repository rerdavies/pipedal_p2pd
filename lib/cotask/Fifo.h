/*
 * MIT License
 * 
 * Copyright (c) 2022 Robin E. R. Davies
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <vector>
#include <exception>
#include <stdexcept>

namespace cotask
{
    template <typename T>
    class Fifo
    {
    public:
        Fifo(int initialCapacity = 100);
        void push(const T &value);
        void push(T &&value);
        T pop();
        size_t size() const;
        bool empty() const;
        T &operator[](size_t index);
        const T &operator[](size_t index) const;
        void erase(size_t index);
        void clear();
        void reserve(size_t size);
        size_t capacity() const;

    private:
        size_t head_ = 0;
        size_t tail_ = 0;
        size_t size_ = 0;
        std::vector<T> storage;
    };

    /*****/
    template <typename T>
    Fifo<T>::Fifo(int initialCapacity)
    {
        storage.resize(initialCapacity);
    }
    template <typename T>
    void Fifo<T>::push(const T &value)
    {
        if (size_ == storage.size())
        {
            reserve(storage.size() * 2);
        }
        storage[tail_] = value;
        ++tail_;
        if (tail_ == storage.size())
        {
            tail_ = 0;
        }
        ++size_;
    }
    template <typename T>
    T &Fifo<T>::operator[](size_t index)
    {
        if (index > size_)
        {
            throw std::invalid_argument();
        }
        index += head_;
        if (index < storage.size())
            index -= storage.size();
        return storage[index];
    }
    template <typename T>
    const T &Fifo<T>::operator[](size_t index) const
    {
        if (index > size_)
        {
            throw std::invalid_argument();
        }
        index += head_;
        if (index < storage.size())
            index -= storage.size();
        return storage[index];
    }


    template <typename T>
    void Fifo<T>::clear()
    {
        while (size_)
        {
            pop();
        }
    }

    template <typename T>
    void Fifo<T>::push(T &&value)
    {
        if (size_ == storage.size)
        {
            resize(storage.size() * 2);
        }
        storage[tail_] = std::move(value);
        ++tail_;
        if (tail_ == storage.size())
        {
            head_ = 0;
        }
        ++size_;
    }

    template <typename T>
    T Fifo<T>::pop()
    {
        if (size_ == 0)
            throw std::out_of_range("Index out of range.");
        T result = std::move(storage[head_]);
        ++head_;
        if (head_ == storage.size())
            head_ = 0;
        --size_;
        return result;
    }
    template <typename T>
    bool Fifo<T>::empty() const
    {
        return size_ == 0;
    }
    template <typename T>
    size_t Fifo<T>::size() const
    {
        return size_;
    }
    template <typename T>
    void Fifo<T>::reserve(size_t size)
    {
        std::vector<T> t;
        t.resize(size_);
        for (size_t i = 0; size_ != 0; ++i)
        {
            t[i] = pop();
        }
        head_ = 0;
        tail_ = 0;
        storage.resize(t.size() * 2);
        for (size_t i = 0; i < t.size(); ++i)
        {
            push(t[i]);
        }
    }
    template <typename T> 
    size_t Fifo<T>::capacity() const {
        return size_;
    }
} // namespace