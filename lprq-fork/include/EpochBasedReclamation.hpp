#pragma once

#include <vector>
#include <functional>
#include <atomic>

template <typename T>
class EpochBasedReclamation
{
public:
    struct alignas(512) ThreadLocalSpace
    {
        alignas(64) std::atomic<long long> announcement = -1;
        int padding[32];
        long long epoch = 0;
        int count = 0;
        std::vector<T *> *old_retire_bag = nullptr;
        std::vector<T *> *cur_retire_bag = nullptr;
    };

    static const int REFRESH_STEPS = 16;
    static const int NEW_BATCH = 32;

    int thread_count;
    std::vector<ThreadLocalSpace> tls;
    int PADDING_1[32];
    std::atomic<long long> current_epoch = 0;
    int PADDING_2[32];

private:
    long long update_global_epoch()
    {
        long long current_e = current_epoch.load();
        for (int j = 0; j < 2; j++)
        {
            for (int i = 0; i < thread_count; i++)
            {
                long long thread_epoch = tls[i].announcement.load();
                if (thread_epoch != -1 && thread_epoch < current_e)
                    return -1;
            }
        }

        bool success = current_epoch.compare_exchange_strong(current_e, current_e + 1);
        return success ? current_e + 1 : -1;
    }

    void recycle(std::vector<T *> *retire_bag)
    {
        for (T *p : *retire_bag)
            delete p;
        retire_bag->clear();
    }

public:
    EpochBasedReclamation(int thread_count, int init_count = 256)
    {
        this->thread_count = thread_count;
        tls = std::vector<ThreadLocalSpace>(thread_count);
        for (int i = 0; i < thread_count; i++)
        {
            tls[i].old_retire_bag = new std::vector<T *>();
            tls[i].old_retire_bag->reserve(512);
            tls[i].cur_retire_bag = new std::vector<T *>();
            tls[i].cur_retire_bag->reserve(512);
        }
    }

    ~EpochBasedReclamation()
    {
        update_global_epoch();
        for (int i = 0; i < thread_count; i++)
        {
            recycle(tls[i].old_retire_bag);
            delete tls[i].old_retire_bag;
            recycle(tls[i].cur_retire_bag);
            delete tls[i].cur_retire_bag;
            tls[i].old_retire_bag = tls[i].cur_retire_bag = nullptr;
        }
    }

    void enterCritical(int id)
    {
        long long epoch = current_epoch.load();
        tls[id].announcement.exchange(epoch, std::memory_order_acquire);
    }
    void exitCritical(int id)
    {
        tls[id].announcement.store(-1ll, std::memory_order_release);
    }

    T *get_new(int id)
    {
        return new T();
    }
    void retire(T *p, int id)
    {
        if (tls[id].epoch < current_epoch.load())
        {
            recycle(tls[id].old_retire_bag);
            std::swap(tls[id].old_retire_bag, tls[id].cur_retire_bag);
            tls[id].epoch = current_epoch.load();
        }

        // update epoch
        tls[id].count += 1;
        if (tls[id].count % (REFRESH_STEPS * thread_count) == REFRESH_STEPS * id)
        {
            long long updated = update_global_epoch();
            // if (updated > 0)
            // {
            //     std::cout << "Updated to " << updated << std::endl;
            // }
        }

        tls[id].cur_retire_bag->push_back(p);
    }
};
