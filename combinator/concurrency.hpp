#pragma once

#include "stateful.hpp"
#include "../gen.hpp"
#include "../Random.hpp"
#include "../Shrinkable.hpp"
#include "../api.hpp"
#include "../PropertyContext.hpp"
#include <memory>
#include <list>
#include <type_traits>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace PropertyBasedTesting {

template <typename ActionType>
class PROPTEST_API Concurrency {
public:
    using SystemType = typename ActionType::SystemType;
    using SystemTypeGen = std::function<Shrinkable<SystemType>(Random&)>;
    using Actions = std::vector<std::shared_ptr<ActionType>>;
    using ActionsGen = std::function<Shrinkable<Actions>(Random&)>;

    static constexpr uint32_t defaultNumRuns = 100;

    Concurrency(std::shared_ptr<SystemTypeGen> initialGenPtr, std::shared_ptr<ActionsGen> actionsGenPtr)
        : initialGenPtr(initialGenPtr), actionsGenPtr(actionsGenPtr), seed(getCurrentTime()), numRuns(defaultNumRuns)
    {
    }

    bool check();
    bool invoke(Random& rand);
    void handleShrink(Random& savedRand, const PropertyFailedBase& e);

    Concurrency& setSeed(uint64_t s)
    {
        seed = s;
        return *this;
    }

    Concurrency& setNumRuns(uint32_t runs)
    {
        numRuns = runs;
        return *this;
    }

private:
    std::shared_ptr<SystemTypeGen> initialGenPtr;
    std::shared_ptr<ActionsGen> actionsGenPtr;
    uint64_t seed;
    int numRuns;
};

template <typename ActionType>
bool Concurrency<ActionType>::check()
{
    Random rand(seed);
    Random savedRand(seed);
    std::cout << "random seed: " << seed << std::endl;
    int i = 0;
    try {
        for (; i < numRuns; i++) {
            bool pass = true;
            do {
                pass = true;
                try {
                    savedRand = rand;
                    invoke(rand);
                    pass = true;
                } catch (const Success&) {
                    pass = true;
                } catch (const Discard&) {
                    // silently discard combination
                    pass = false;
                }
            } while (!pass);
        }
    } catch (const PropertyFailedBase& e) {
        std::cerr << "Falsifiable, after " << i << " tests: " << e.what() << " (" << e.filename << ":" << e.lineno
                  << ")" << std::endl;

        // shrink
        handleShrink(savedRand, e);
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Falsifiable, after " << i << " tests - std::exception occurred: " << e.what() << std::endl;
        std::cerr << "    seed: " << seed << std::endl;
        return false;
    }

    std::cout << "OK, passed " << numRuns << " tests" << std::endl;

    return true;
}

template <typename ActionType>
struct RearRunner
{
    using SystemType = typename ActionType::SystemType;
    using Actions = std::vector<std::shared_ptr<ActionType>>;

    RearRunner(int n, SystemType& obj, Actions& actions, std::atomic_bool& thread_ready, std::atomic_bool& sync_ready,
               std::vector<int>& log, std::atomic_int& counter)
        : n(n),
          obj(obj),
          actions(actions),
          thread_ready(thread_ready),
          sync_ready(sync_ready),
          log(log),
          counter(counter)
    {
    }

    void operator()()
    {
        thread_ready = true;
        while (!sync_ready) {}

        for (auto action : actions) {
            if (!action->precondition(obj))
                continue;
            PROP_ASSERT(action->run(obj));
            // std::cout << "rear2" << std::endl;
            log[counter++] = n;
        }
    }

    int n;
    SystemType& obj;
    Actions& actions;
    std::atomic_bool& thread_ready;
    std::atomic_bool& sync_ready;
    std::vector<int>& log;
    std::atomic_int& counter;
};

template <typename ActionType>
bool Concurrency<ActionType>::invoke(Random& rand)
{
    Shrinkable<SystemType> initialShr = (*initialGenPtr)(rand);
    SystemType& obj = initialShr.getRef();
    Shrinkable<Actions> frontShr = (*actionsGenPtr)(rand);
    Shrinkable<Actions> rear1Shr = (*actionsGenPtr)(rand);
    Shrinkable<Actions> rear2Shr = (*actionsGenPtr)(rand);
    Actions& front = frontShr.getRef();
    Actions& rear1 = rear1Shr.getRef();
    Actions& rear2 = rear2Shr.getRef();

    // front
    for (auto action : front) {
        if (action->precondition(obj))
            PROP_ASSERT(action->run(obj));
    }

    // rear
    std::thread spawner([&]() {
        std::atomic_bool thread1_ready(false);
        std::atomic_bool thread2_ready(false);
        std::atomic_bool sync_ready(false);
        std::atomic<int> counter{0};
        std::vector<int> log;
        log.resize(5000);

        std::thread rearRunner1(RearRunner<ActionType>(1, obj, rear1, thread1_ready, sync_ready, log, counter));
        std::thread rearRunner2(RearRunner<ActionType>(2, obj, rear2, thread2_ready, sync_ready, log, counter));
        while (!thread1_ready) {}
        while (!thread2_ready) {}

        sync_ready = true;

        rearRunner1.join();
        rearRunner2.join();

        std::cout << "count: " << counter << std::endl;
        std::cout << "threads: ";
        for (int i = 0; i < counter; i++) {
            std::cout << log[i];
        }
        std::cout << std::endl;
    });

    spawner.join();

    return true;
}

template <typename ActionType>
void Concurrency<ActionType>::handleShrink(Random& savedRand, const PropertyFailedBase& e)
{
}

template <typename ActionType, typename InitialGen, typename ActionsGen>
decltype(auto) concurrency(InitialGen&& initialGen, ActionsGen&& actionsGen)
{
    using SystemType = typename ActionType::SystemType;
    using SystemTypeGen = std::function<Shrinkable<SystemType>(Random&)>;
    using Actions = std::vector<std::shared_ptr<ActionType>>;
    auto initialGenPtr = std::make_shared<SystemTypeGen>(std::forward<InitialGen>(initialGen));
    auto actionsGenPtr =
        std::make_shared<std::function<Shrinkable<Actions>(Random&)>>(std::forward<ActionsGen>(actionsGen));
    return Concurrency<ActionType>(initialGenPtr, actionsGenPtr);
}

}  // namespace PropertyBasedTesting