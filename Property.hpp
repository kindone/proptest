#pragma once

#include "api.hpp"
#include "gen.hpp"
#include "function_traits.hpp"
#include "tuple.hpp"
#include "Stream.hpp"
#include "printing.hpp"
#include "generator/util.hpp"
#include "PropertyContext.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>

#define PROP_STAT(VALUE)                                               \
    do {                                                               \
        std::stringstream key;                                         \
        key << (#VALUE);                                               \
        std::stringstream value;                                       \
        value << std::boolalpha;                                       \
        value << (VALUE);                                              \
        PropertyBase::tag(__FILE__, __LINE__, key.str(), value.str()); \
    } while (false);

#define PROP_TAG(KEY, VALUE)                                           \
    do {                                                               \
        std::stringstream key;                                         \
        key << (KEY);                                                  \
        std::stringstream value;                                       \
        value << std::boolalpha;                                       \
        value << (VALUE);                                              \
        PropertyBase::tag(__FILE__, __LINE__, key.str(), value.str()); \
    } while (false);

#define PROP_CLASSIFY(condition, KEY, VALUE)                   \
    do {                                                       \
        if (condition) {                                       \
            PropertyBase::tag(__FILE__, __LINE__, KEY, VALUE); \
        }                                                      \
    } while (false);

namespace PropertyBasedTesting {

class Random;

class PROPTEST_API PropertyBase {
public:
    PropertyBase();
    bool check();
    virtual ~PropertyBase() {}
    static void tag(const char* filename, int lineno, std::string key, std::string value);
    static void setDefaultNumRuns(uint32_t);

protected:
    static void setContext(PropertyContext* context);
    static PropertyContext* context;

protected:
    virtual bool invoke(Random& rand) = 0;
    virtual void handleShrink(Random& savedRand, const PropertyFailedBase& e) = 0;

    static uint32_t defaultNumRuns;

    // TODO: configurations
    uint64_t seed;
    uint32_t numRuns;

    friend struct PropertyContext;
};

namespace util {

template <typename T>
decltype(auto) ReturnTypeOf()
{
    TypeHolder<typename std::result_of<decltype (&T::operator())(T, Random&)>::type> typeHolder;
    return typeHolder;
}

template <typename... ARGS>
decltype(auto) ReturnTypeTupleFromGenTup(std::tuple<ARGS...>& tup)
{
    TypeList<typename decltype(ReturnTypeOf<ARGS>())::type...> typeList;
    return typeList;
}

}  // namespace util

template <typename CallableWrapper, typename GenTuple>
class Property : public PropertyBase {
public:
    Property(CallableWrapper&& c, const GenTuple& g) : callableWrapper(std::forward<CallableWrapper>(c)), genTup(g) {}

    virtual bool invoke(Random& rand)
    {
        return util::invokeWithGenTuple(
            rand, std::forward<decltype(callableWrapper.callable)>(callableWrapper.callable), genTup);
    }

    Property& setSeed(uint64_t s)
    {
        seed = s;
        return *this;
    }

    Property& setNumRuns(uint32_t runs)
    {
        numRuns = runs;
        return *this;
    }

    template <typename... ARGS>
    bool example(ARGS&&... args)
    {
        PropertyContext context;
        auto valueTup = std::make_tuple(args...);
        auto valueTupPtr = std::make_shared<decltype(valueTup)>(valueTup);
        try {
            try {
                try {
                    return util::invokeWithArgs(std::forward<typename CallableWrapper::T>(callableWrapper.callable),
                                                std::forward<ARGS>(args)...);
                } catch (const AssertFailed& e) {
                    throw PropertyFailed<decltype(valueTup)>(e, valueTupPtr);
                }
            } catch (const Success&) {
                return true;
            } catch (const Discard&) {
                // silently discard combination
                std::cerr << "Discard is not supported for single run" << std::endl;
            }
        } catch (const PropertyFailedBase& e) {
            std::cerr << "Property failed: " << e.what() << " (" << e.filename << ":" << e.lineno << ")" << std::endl;
            return false;
        } catch (const std::exception& e) {
            // skip shrinking?
            std::cerr << "std::exception occurred: " << e.what() << std::endl;
            return false;
        }
        return false;
    }

    virtual void handleShrink(Random& savedRand, const PropertyFailedBase& e)
    {
        auto retTypeTup = util::ReturnTypeTupleFromGenTup(genTup);
        using ValueTuple = typename decltype(retTypeTup)::type_tuple;
        auto failed = dynamic_cast<const PropertyFailed<ValueTuple>&>(e);
        shrink(savedRand, *failed.valueTupPtr);
    }

private:
    template <size_t N, typename ValueTuple, typename Replace>
    bool test(ValueTuple&& valueTup, Replace&& replace)
    {
        // std::cout << "    test: tuple ";
        // show(std::cout, valueTup);
        // std::cout << " replaced with arg " << N << ": ";
        // show(std::cout, replace);
        // std::cout << std::endl;

        bool result = false;
        auto values = util::transformHeteroTuple<ShrinkableGet>(std::forward<ValueTuple>(valueTup));
        try {
            result = util::invokeWithArgTupleWithReplace<N>(
                std::forward<typename CallableWrapper::T>(callableWrapper.callable),
                std::forward<decltype(values)>(values), replace.get());
            // std::cout << "    test done: result=" << (result ? "true" : "false") << std::endl;
        } catch (const AssertFailed& e) {
            // std::cout << "    test failed with AssertFailed: result=" << (result ? "true" : "false") << std::endl;
            // TODO: trace
        } catch (const std::exception& e) {
            // std::cout << "    test failed with std::exception: result=" << (result ? "true" : "false") << std::endl;
            // TODO: trace
        }
        return result;
    }

    template <typename Shrinks>
    static void printShrinks(const Shrinks& shrinks)
    {
        auto itr = shrinks.iterator();
        // std::cout << "    shrinks: " << std::endl;
        for (int i = 0; i < 4 && itr.hasNext(); i++) {
            std::cout << "    ";
            show(std::cout, itr.next());
            std::cout << std::endl;
        }
    }

    template <size_t N, typename ValueTuple, typename ShrinksTuple>
    decltype(auto) shrinkN(ValueTuple&& valueTup, ShrinksTuple&& shrinksTuple)
    {
        // std::cout << "  shrinking arg " << N << ":";
        // show(std::cout, valueTup);
        // std::cout << std::endl;
        auto shrinks = std::get<N>(shrinksTuple);
        // keep shrinking until no shrinking is possible
        while (!shrinks.isEmpty()) {
            // printShrinks(shrinks);
            auto iter = shrinks.iterator();
            bool shrinkFound = false;
            // keep trying until failure is reproduced
            while (iter.hasNext()) {
                // get shrinkable
                auto next = iter.next();
                if (!test<N>(std::forward<ValueTuple>(valueTup), next)) {
                    shrinks = next.shrinks();
                    std::get<N>(valueTup) = next;
                    shrinkFound = true;
                    // std::cout << "  shrinking arg " << N << " tested false: ";
                    // show(std::cout, valueTup);
                    // show(std::cout, next);
                    // std::cout << std::endl;
                    break;
                }
            }
            if (shrinkFound) {
                // std::cout << "  shrinking arg " << N << " found: ";
                show(std::cout, valueTup);
                std::cout << std::endl;
            } else {
                break;
            }
        }
        // std::cout << "  no more shrinking found for arg " << N << std::endl;
        return std::get<N>(valueTup);
    }

    template <typename ValueTuple, typename ShrinksTuple, std::size_t... index>
    decltype(auto) shrinkEach(ValueTuple&& valueTup, ShrinksTuple&& shrinksTup, std::index_sequence<index...>)
    {
        return std::make_tuple(
            shrinkN<index>(std::forward<ValueTuple>(valueTup), std::forward<ShrinksTuple>(shrinksTup))...);
    }

    template <typename ValueTuple>
    void shrink(Random& savedRand, ValueTuple&& valueTup)
    {
        // std::cout << "shrinking value: ";
        // show(std::cout, valueTup);
        // std::cout << std::endl;

        auto generatedValueTup = util::transformHeteroTupleWithArg<Generate>(std::forward<GenTuple>(genTup), savedRand);
        // std::cout << (valueTup == valueTup2 ? "gen equals original" : "gen not equals original") << std::endl;
        static constexpr auto Size = std::tuple_size<std::decay_t<ValueTuple>>::value;
        auto shrinksTuple =
            util::transformHeteroTuple<GetShrinks>(std::forward<decltype(generatedValueTup)>(generatedValueTup));
        auto shrunk = shrinkEach(std::forward<decltype(generatedValueTup)>(generatedValueTup),
                                 std::forward<decltype(shrinksTuple)>(shrinksTuple), std::make_index_sequence<Size>{});
        std::cout << "shrunk: ";
        show(std::cout, shrunk);
        std::cout << std::endl;
    }

private:
    CallableWrapper callableWrapper;
    GenTuple genTup;
};

template <class Callable>
class CallableWrapper {
public:
    using T = Callable;
    Callable callable;
    CallableWrapper(Callable&& c) : callable(std::forward<Callable>(c)) {}
};

template <class Callable>
auto make_CallableWrapper(Callable&& callable)
{
    return CallableWrapper<Callable>(std::forward<Callable>(callable));
}

template <typename Callable, typename... EXPGENS>
auto property(Callable&& callable, EXPGENS&&... gens)
{
    // acquire full tuple of generators
    typename function_traits<Callable>::argument_type_list argument_type_list;
    auto genTup = util::createGenTuple(argument_type_list, gens...);
    return Property<CallableWrapper<Callable>, decltype(genTup)>(make_CallableWrapper(std::forward<Callable>(callable)),
                                                                 genTup);
}

template <typename Callable, typename... EXPGENS>
bool check(Callable&& callable, EXPGENS&&... gens)
{
    return property(callable, gens...).check();
}

}  // namespace PropertyBasedTesting