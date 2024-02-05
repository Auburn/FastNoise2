#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>

template<typename T>
class GenerateQueue
{
public:
    void KillThreads()
    {
        std::unique_lock lock( mMutex );
        mKillThreads = true;
        mCond.notify_all();
    }

    bool ShouldKillThread()
    {
        return mKillThreads;        
    }

    void Clear()
    {
        std::unique_lock lock( mMutex );
        mQueue = {};
    }

    size_t Count()
    {
        std::unique_lock lock( mMutex );
        return mQueue.size();
    }

    T Pop()
    {
        std::unique_lock lock( mMutex );
        while( mQueue.empty() || mKillThreads )
        {
            if( mKillThreads )
            {
                return {};
            }

            mCond.wait( lock );
        }
        auto item = mQueue.front();
        mQueue.pop();
        return item;
    }

    size_t Push( const T& item )
    {
        std::unique_lock lock( mMutex );
        mQueue.push( item );
        size_t size = mQueue.size();
        lock.unlock();
        mCond.notify_one();
        return size;
    }

private:
    std::queue<T> mQueue;
    std::mutex mMutex;
    std::condition_variable mCond;
    bool mKillThreads = false;
};

template<typename T>
class CompleteQueue
{
public:
    uint32_t IncVersion()
    {
        std::unique_lock lock( mMutex );
        return ++mVersion;
    }

    size_t Count()
    {
        std::unique_lock lock( mMutex );
        return mQueue.size();
    }

    bool Pop( T& out )
    {
        std::unique_lock lock( mMutex );
        if( mQueue.empty() )
        {
            return false;
        }
        out = mQueue.front();
        mQueue.pop();
        return true;
    }

    bool Push( const T& item, uint32_t version = 0 )
    {
        std::unique_lock lock( mMutex );
        if( version == mVersion )
        {
            mQueue.push( item );
            return true;
        }
        return false;
    }

private:
    uint32_t mVersion = 0;
    std::queue<T> mQueue;
    std::mutex mMutex;
};
