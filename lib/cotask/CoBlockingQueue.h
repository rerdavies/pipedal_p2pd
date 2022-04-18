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
#include "CoEvent.h"

namespace cotask {

    /**
     * @brief An thread-safe coroutine queue containing elements of type T.
     * 
     * @tparam T Type of objects contained in the quueue.
     * 
     * The maximum length of a CoBlockingQueue is unbounded. For a queue that has a maximum number 
     * of entries, see CoBlockingQueue.
     */
    template <class T>
    class CoBlockingQueue
    {
    public:
        using item_type = T;

        CoBlockingQueue(size_t maxLength);
        ~CoBlockingQueue();

        /**
         * @brief Copy a value into the queue.
         * 
         * @param value A point to an object to be transferred through the queue.
         * @param timeout (optional) Time to wait for space in the queue.
         * @return Task<> 
         * @throws CoTimeoutException on timeout.
         * 
         * The current coroutine is suspended if there is no space in the queue. 
         * 
         * Ownmership of the value is transferred to the queue. In the event of a timeout,
         * the caller retains ownership. If CoBlockingQueue is destroyed with items in the 
         * queue, they will be deleted. 
         * 
         * Pushing a value into the queue is (at a minimum) a std::memory_order_release operation.
         * Since taking a value from the queue is a std::memory_order_aquire operation, users of 
         * the queue can rely on all pending write operations to members of the T* to have completed
         * by the time the object has passed through the queue should the object pass across a
         * thread boundary.
         */
        CoTask<> Push(T *value, std::chrono::milliseconds timeout = NO_TIMEOUT);

        
        /**
         * @brief Take a value from the queue.
         * 
         * @param timeout How long to wait for a value to become available.
         * @return Task<T*> A pointer to an object.
         * @throws CoTimeoutException on timeout
         * @throws CoIoClosedException if Close() has been called.
         * 
         * A CoTimeoutException is thrown if the timeout expires. 
         * 
         * If the Take() succeeds, the caller assumes ownership of the returned object.
         * 
         * Taking an object from the queue is (at a minimum) a std::memory_order_aquire 
         * operation. Since Pushing an object into the queue is a std::memory_order_release
         * operation, users of the queue can rely on all pending write operations to members 
         * of T*that occurred before the Push being visible after Taking the object if 
         * the object travels across a thread boundary.
         */
        CoTask<T*> Take(std::chrono::milliseconds timeout = NO_TIMEOUT);

        /**
         * @brief Close the queue.
         * 
         * Subsequent attempts to Push() into the queue will throw a CoIoClosedException.
         * Readers may Take() any values that are in the queue already, and will receive 
         * a CoIoClosedException once the queue has emptied. 
         */
        void Close();

        /**
         * @brief Has the queue been closed?
         * 
         * The result std::memory_order_seq_cst ordering with Close().
         * 
         * @return true if the queue has been closed.
         * @return false if the queue has not been closed.
         */
        bool IsClosed();
        /**
         * @brief Is the queue empty?
         * 
         * @return true if the queue is empty.
         * @return false if the queue is not empty.
         */
        bool IsEmpty();

    private:
        size_t maxLength;
        CoConditionVariable pushCv;
        CoConditionVariable takeCv;
        using queue_type = std::list<T*>;
        queue_type queue;
        bool closed = false;
        std::mutex queueMutex;
    };

    /*****  CoBlockingQueue inlines ****************************/
    template <typename T>
    CoTask<> CoBlockingQueue<T>::Push(T *value, std::chrono::milliseconds timeout)
    {

        co_await pushCv.Wait(
            timeout,
            [this, value]() {
                std::lock_guard lock {queueMutex};
                if (this->closed)
                {
                    throw CoIoClosedException();
                }
                if (queue.size() == this->maxLength)
                {
                    return false;
                }
                queue.push_back(value);
                return true;
            });
        takeCv.Notify([this]() {
            
        });

        co_return;
    }
    template <typename T>
    CoTask<T*> CoBlockingQueue<T>::Take(std::chrono::milliseconds timeout)
    {
        T* value;
        co_await takeCv.Wait(
            timeout,
            [this, &value]() {
                std::lock_guard lock {queueMutex};
                if (queue.empty())
                {
                    if (closed)
                    {
                        throw CoIoClosedException();
                    }
                    return false; // suspend.
                }
                value = queue.front();
                queue.pop_front();
                return true;  // resume.
            });

        pushCv.Notify(
            [this]() {
            });
        co_return value;
    }

    template <typename T>
    bool CoBlockingQueue<T>::IsEmpty()
    {
        std::lock_guard lock {queueMutex};
        return queue.empty();
    }

    template <typename T>
    void CoBlockingQueue<T>::Close()
    {
        takeCv.NotifyAll(
            [this] {
                this->closed = true;
            }
        );
        pushCv.NotifyAll(
            [this] {
                this->closed = true;
            }
        );
        
    }
    template <typename T>
    CoBlockingQueue<T>::CoBlockingQueue(size_t maxLength)
    :maxLength(maxLength)
    {

    }

    template <typename T>
    bool CoBlockingQueue<T>::IsClosed()
    {
        return takeCv.Test(
            [this] {
                return this->closed;
            }
        );
    }

    template <typename T>
    CoBlockingQueue<T>::~CoBlockingQueue()
    {
        for (auto i = queue.begin(); i != queue.end(); ++i)
        {
            delete (*i);
        }
    }

} // namespace
