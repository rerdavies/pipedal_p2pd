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