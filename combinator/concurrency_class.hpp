#pragma once

#include "stateful_class.hpp"
#include "../gen.hpp"
#include "../Random.hpp"
#include "../Shrinkable.hpp"
#include "../api.hpp"
#include "../PropertyContext.hpp"
#include "../GenBase.hpp"
#include "../util/std.hpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

namespace proptest {
namespace concurrent {
namespace alt {

using std::atomic;
using std::atomic_bool;
using std::atomic_int;
using std::mutex;
using std::thread;

using proptest::stateful::alt::Action;
using proptest::stateful::alt::actionListGenOf;
using proptest::stateful::alt::SimpleAction;

template <typename ActionType>
class PROPTEST_API Concurrency {
public:
    using ObjectType = typename ActionType::ObjectType;
    using ObjectTypeGen = GenFunction<ObjectType>;
    using ModelType = typename ActionType::ModelType;
    using ModelTypeGen = function<ModelType(ObjectType&)>;
    using ActionList = list<shared_ptr<ActionType>>;
    using ActionListGen = GenFunction<ActionList>;

    static constexpr uint32_t defaultNumRuns = 200;

    Concurrency(shared_ptr<ObjectTypeGen> _initialGenPtr, shared_ptr<ActionListGen> _actionListGenPtr)
        : initialGenPtr(_initialGenPtr),
          actionListGenPtr(_actionListGenPtr),
          seed(getCurrentTime()),
          numRuns(defaultNumRuns)
    {
    }

    Concurrency(shared_ptr<ObjectTypeGen> _initialGenPtr, shared_ptr<ModelTypeGen> _modelFactoryPtr,
                shared_ptr<ActionListGen> _actionListGenPtr)
        : initialGenPtr(_initialGenPtr),
          modelFactoryPtr(_modelFactoryPtr),
          actionListGenPtr(_actionListGenPtr),
          seed(getCurrentTime()),
          numRuns(defaultNumRuns)
    {
    }

    bool go();
    bool go(function<void(ObjectType&, ModelType&)> postCheck);
    bool go(function<void(ObjectType&)> postCheck);
    bool invoke(Random& rand, function<void(ObjectType&, ModelType&)> postCheck);
    void handleShrink(Random& savedRand);

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
    shared_ptr<ObjectTypeGen> initialGenPtr;
    shared_ptr<ModelTypeGen> modelFactoryPtr;
    shared_ptr<ActionListGen> actionListGenPtr;
    uint64_t seed;
    int numRuns;
};

template <typename ActionType>
bool Concurrency<ActionType>::go()
{
    static auto emptyPostCheck = +[](ObjectType&, ModelType&) {};
    return go(emptyPostCheck);
}

template <typename ActionType>
bool Concurrency<ActionType>::go(function<void(ObjectType&)> postCheck)
{
    auto fullPostCheck = [postCheck](ObjectType& sys, ModelType&) { postCheck(sys); };
    return go(fullPostCheck);
}

template <typename ActionType>
bool Concurrency<ActionType>::go(function<void(ObjectType&, ModelType&)> postCheck)
{
    Random rand(seed);
    Random savedRand(seed);
    cout << "random seed: " << seed << endl;
    int i = 0;
    try {
        for (; i < numRuns; i++) {
            bool pass = true;
            do {
                pass = true;
                try {
                    savedRand = rand;
                    invoke(rand, postCheck);
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
        cerr << "Falsifiable, after " << (i + 1) << " tests: " << e.what() << " (" << e.filename << ":" << e.lineno
                  << ")" << endl;

        // shrink
        handleShrink(savedRand);
        return false;
    } catch (const exception& e) {
        cerr << "Falsifiable, after " << (i + 1) << " tests - exception occurred: " << e.what() << endl;
        cerr << "    seed: " << seed << endl;
        // shrink
        handleShrink(savedRand);
        return false;
    }

    cout << "OK, passed " << numRuns << " tests" << endl;

    return true;
}

template <typename ActionType>
struct RearRunner
{
    using ObjectType = typename ActionType::ObjectType;
    using ModelType = typename ActionType::ModelType;
    using ActionList = list<shared_ptr<ActionType>>;

    RearRunner(int _n, ObjectType& _obj, ModelType& _model, ActionList& _actions, atomic_bool& _thread_ready,
               atomic_bool& _sync_ready, vector<int>& _log, atomic_int& _counter)
        : n(_n),
          obj(_obj),
          model(_model),
          actions(_actions),
          thread_ready(_thread_ready),
          sync_ready(_sync_ready),
          log(_log),
          counter(_counter)
    {
    }

    void operator()()
    {
        thread_ready = true;
        while (!sync_ready) {}

        for (auto action : actions) {
            if (!action->precondition(obj, model))
                continue;
            PROP_ASSERT(action->run(obj, model));
            // cout << "rear2" << endl;
            log[counter++] = n;
        }
    }

    int n;
    ObjectType& obj;
    ModelType& model;
    ActionList& actions;
    atomic_bool& thread_ready;
    atomic_bool& sync_ready;
    vector<int>& log;
    atomic_int& counter;
};

template <typename ActionType>
bool Concurrency<ActionType>::invoke(Random& rand, function<void(ObjectType&, ModelType&)> postCheck)
{
    Shrinkable<ObjectType> initialShr = (*initialGenPtr)(rand);
    ObjectType& obj = initialShr.getRef();
    ModelType model = modelFactoryPtr ? (*modelFactoryPtr)(obj) : ModelType();
    Shrinkable<ActionList> frontShr = (*actionListGenPtr)(rand);
    Shrinkable<ActionList> rear1Shr = (*actionListGenPtr)(rand);
    Shrinkable<ActionList> rear2Shr = (*actionListGenPtr)(rand);
    ActionList& front = frontShr.getRef();
    ActionList& rear1 = rear1Shr.getRef();
    ActionList& rear2 = rear2Shr.getRef();

    // front
    for (auto action : front) {
        if (action->precondition(obj, model))
            PROP_ASSERT(action->run(obj, model));
    }

    // rear
    thread spawner([&]() {
        atomic_bool thread1_ready(false);
        atomic_bool thread2_ready(false);
        atomic_bool sync_ready(false);
        atomic<int> counter{0};
        vector<int> log;
        log.resize(5000);

        thread rearRunner1(RearRunner<ActionType>(1, obj, model, rear1, thread1_ready, sync_ready, log, counter));
        thread rearRunner2(RearRunner<ActionType>(2, obj, model, rear2, thread2_ready, sync_ready, log, counter));
        while (!thread1_ready) {}
        while (!thread2_ready) {}

        sync_ready = true;

        rearRunner1.join();
        rearRunner2.join();

        cout << "count: " << counter << ", order: ";
        for (int i = 0; i < counter; i++) {
            cout << log[i];
        }
        cout << endl;
    });

    spawner.join();
    postCheck(obj, model);

    return true;
}

template <typename ActionType>
void Concurrency<ActionType>::handleShrink(Random&)
{
}

template <typename ActionType, typename InitialGen, typename ActionListGen>
decltype(auto) concurrency(InitialGen&& initialGen, ActionListGen&& actionListGen)
{
    using ObjectType = typename ActionType::ObjectType;
    using ObjectTypeGen = GenFunction<ObjectType>;
    using ActionList = list<shared_ptr<ActionType>>;
    auto initialGenPtr = util::make_shared<ObjectTypeGen>(util::forward<InitialGen>(initialGen));
    auto actionListGenPtr = util::make_shared<GenFunction<ActionList>>(util::forward<ActionListGen>(actionListGen));
    return Concurrency<ActionType>(initialGenPtr, actionListGenPtr);
}

template <typename ActionType, typename InitialGen, typename ModelFactory, typename ActionListGen>
decltype(auto) concurrency(InitialGen&& initialGen, ModelFactory&& modelFactory, ActionListGen&& actionListGen)
{
    using ObjectType = typename ActionType::ObjectType;
    using ModelType = typename ActionType::ModelType;
    using ModelFactoryFunction = function<ModelType(ObjectType&)>;
    using ObjectTypeGen = GenFunction<ObjectType>;
    using ActionList = list<shared_ptr<ActionType>>;

    shared_ptr<ModelFactoryFunction> modelFactoryPtr =
        util::make_shared<ModelFactoryFunction>(util::forward<ModelFactory>(modelFactory));

    auto initialGenPtr = util::make_shared<ObjectTypeGen>(util::forward<InitialGen>(initialGen));
    auto actionListGenPtr = util::make_shared<GenFunction<ActionList>>(util::forward<ActionListGen>(actionListGen));
    return Concurrency<ActionType>(initialGenPtr, modelFactoryPtr, actionListGenPtr);
}

}  // namespace alt
}  // namespace concurrent

}  // namespace proptest
