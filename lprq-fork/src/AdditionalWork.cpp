#include <AdditionalWork.hpp>
#include <random>
#include <chrono>
#include <iostream>

static std::minstd_rand random_engine{
    static_cast<std::minstd_rand::result_type>(std::chrono::high_resolution_clock::now().time_since_epoch().count())};
static thread_local std::mt19937 rd_gen{random_engine()};

class RandomGen
{
public:
    int count = 0;
    RandomGen() = default;
    ~RandomGen()
    {
        std::cerr << "Generated " << count << " random numbers" << std::endl;
    }
    int operator()()
    {
        count++;
        return rd_gen();
    }
};

static thread_local RandomGen my_gen;

void random_additional_work(const int slots)
{
    int rd_work = 0;
    if (slots > 1)
    {
        int x = 1;
        while (x % slots != 0)
        {
            x = my_gen() % slots;
            rd_work++;
        }
    }
}
