#pragma once

#include "testing/gen.hpp"
#include "testing/Random.hpp"
#include "testing/Shrinkable.hpp"
#include <utility>
#include <memory>


namespace PropertyBasedTesting
{

template <typename ARG1, typename ARG2>
class PairGenUtility {
    using out_pair_t = std::pair<ARG1, ARG2>;
    using pair_t = std::pair<Shrinkable<ARG1>, Shrinkable<ARG2>>;
    using shrinkable_t = Shrinkable<pair_t>;
    using stream_t = Stream<shrinkable_t>;

private:
    static std::function<stream_t(const shrinkable_t&)> genStream1() {
        using e_shrinkable_t = Shrinkable<ARG1>;
        using element_t = typename e_shrinkable_t::type;

        return [](const shrinkable_t& parent) -> stream_t {
            std::shared_ptr<pair_t> parentRef = std::make_shared<pair_t>(parent.getRef());

            e_shrinkable_t& elem = parentRef->first;
            shrinkable_t pairWithElems = elem.template transform<pair_t>([parentRef](const element_t& val) {
                parentRef->first = make_shrinkable<element_t>(val);
                return make_shrinkable<pair_t>(*parentRef);
            });
            return pairWithElems.shrinks();
        };
    };

    static std::function<stream_t(const shrinkable_t&)> genStream2() {
        using e_shrinkable_t = Shrinkable<ARG2>;
        using element_t = typename e_shrinkable_t::type;

        return [](const shrinkable_t& parent) -> stream_t {
            std::shared_ptr<pair_t> parentRef = std::make_shared<pair_t>(parent.getRef());

            e_shrinkable_t& elem = parentRef->second;
            shrinkable_t pairWithElems = elem.template transform<pair_t>([parentRef](const element_t& val) {
                parentRef->second = make_shrinkable<element_t>(val);
                return make_shrinkable<pair_t>(*parentRef);
            });
            return pairWithElems.shrinks();
        };
    };

public:
    static Shrinkable<out_pair_t> generateStream(const shrinkable_t& shrinkable) {
        auto concatenated = shrinkable.concat(genStream1()).concat(genStream2());
        return concatenated.template transform<out_pair_t>([](const pair_t& pair){
            return make_shrinkable<out_pair_t>(std::make_pair(pair.first.get(), pair.second.get()));
        });
    }
};


template <typename ARG1, typename ARG2>
Shrinkable<std::pair<ARG1, ARG2>> generatePairStream(const Shrinkable<std::pair<Shrinkable<ARG1>, Shrinkable<ARG2>>>& shrinkable) {
    return PairGenUtility<ARG1,ARG2>::generateStream(shrinkable);
}

// generates e.g. (int, int)
// and shrinks one parameter by one and then continues to the next
template <typename GEN1, typename GEN2>
decltype(auto) pair(GEN1&& gen1, GEN2&& gen2) {
    auto genPair = std::make_pair(gen1, gen2);
    // generator
    return [genPair](Random& rand) mutable {
        auto elemPair = std::make_pair(genPair.first(rand), genPair.second(rand));
        auto shrinkable = make_shrinkable<decltype(elemPair)>(elemPair);
        return generatePairStream(shrinkable);
    };
}

template <typename ARG1, typename ARG2>
class PROPTEST_API Arbitrary< std::pair<ARG1,ARG2>> : public Gen< std::pair<ARG1, ARG2> >
{
public:
    Shrinkable<std::pair<ARG1, ARG2>> operator()(Random& rand) {
        return pair(Arbitrary<ARG1>(), Arbitrary<ARG2>())(rand);
    }
};

}