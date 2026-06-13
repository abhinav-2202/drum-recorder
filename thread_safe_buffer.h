#ifndef THREAD_SAFE_BUFFER_H
#define THREAD_SAFE_BUFFER_H

#include <vector>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

// One unit of audio passed b/w threads. 'data' is sized to full period at
// construction. 'frames' records how many frames are actually valid
struct AudioChunk
{
    std::vector<int16_t> data;      // raw interleaved samples (L R L R for stereo)
    int frames = 0;
};

// Mutex and condition variable exist inside class so caller can't access buffer
// without holding the lock. All storage is allocated in constructor to avoid xruns
class ThreadSafeBuffer
{
public:
    // capacity = no.of slots in the buffer
    // chunk_size = samples per slot (period_size * channels)
    // channels = samples per frame (2 for stereo)
    ThreadSafeBuffer(size_t capacity, size_t chunk_size, size_t channels)
        : capacity_(capacity), channels_(channels)
    {
        buffer_.resize(capacity_);
        for (auto &slot : buffer_)
        {
            slot.data.resize(chunk_size);   // pre allocate every slot
            slot.frames = 0;
        }
    }
    
    // Making it impossible to copy or duplicate the buffer as it holds locks
    ThreadSafeBuffer(const ThreadSafeBuffer &) = delete;
    ThreadSafeBuffer &operator=(const ThreadSafeBuffer &) = delete;

    // Producer copies 'frames' worth of samples into next slot
    // Returns false if buffer is full (data is dropped)
    bool push(const std::vector<int16_t> &input, int frames)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);

            if (count_ == capacity_)
            {
                drops_.fetch_add(1, std::memory_order_relaxed);     // count dropped frames instead of logging (avoiding overruns)
                return false;
            }

            int samples = frames * static_cast<int>(channels_);
            for (int i = 0; i < samples; i++)
                buffer_[head_].data[i] = input[i];

            buffer_[head_].frames = frames;
            head_ = (head_ + 1) % capacity_;
            count_++;
        }   // Unlock before notifying. Notifying under the lock makes consumer block the mutex

        cv_.notify_one();
        return true;
    }

    // Consumer waits till 'timeout' for data. Then pops a chunk into 'out'
    // Returns false on timeout with empty buffer (shutdown loop)
    bool pop_wait(AudioChunk &out, std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait_for(lock, timeout, [this] { return count_ > 0; });  // Protection against false wakeups

        if (count_ == 0)    // Real timeout, no data
            return false;

        out.frames = buffer_[tail_].frames;
        int samples = out.frames * static_cast<int>(channels_);
        out.data.assign(buffer_[tail_].data.begin(), buffer_[tail_].data.begin() + samples);
        tail_ = (tail_ + 1) % capacity_;

        count_--;
        return true;
    }

    // Total dropped frames over the buffer's lifetime
    size_t drops() const { return drops_.load(std::memory_order_relaxed); }

private:
    std::vector<AudioChunk> buffer_;
    size_t head_ = 0;   // next slot to write (producer)
    size_t tail_ = 0;   // next slot to read (consumer)
    size_t count_ = 0;  // slots currently filled
    size_t capacity_;
    size_t channels_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<size_t> drops_{0};
};

#endif