#pragma once
#include "../util/std.hpp"
#include "../Shrinkable.hpp"
#include "../Random.hpp"
#include "../GenBase.hpp"

namespace proptest {

template <typename GEN>
decltype(auto) generator(GEN&& gen);
template <typename T>
struct Generator;

// returns a shrinkable pair of <T,U> where U depends on T
template <typename T, typename U>
Generator<U> derive(GenFunction<T> gen1, function<GenFunction<U>(T&)> gen2gen)
{
    auto gen1Ptr = util::make_shared<decltype(gen1)>(gen1);
    auto gen2genPtr = util::make_shared<function<Generator<U>(const T&)>>(
        [gen2gen](const T& t) { return gen2gen(const_cast<T&>(t)); });

    auto genU = [gen1Ptr, gen2genPtr](Random& rand) -> Shrinkable<U> {
        // generate T
        Shrinkable<T> shrinkableT = (*gen1Ptr)(rand);
        using Intermediate = pair<T, Shrinkable<U>>;

        // shrink strategy 1: expand Shrinkable<T>
        Shrinkable<pair<T, Shrinkable<U>>> intermediate =
            shrinkableT.template flatMap<pair<T, Shrinkable<U>>>([&rand, gen2genPtr](const T& t) {
                // generate U
                auto gen2 = (*gen2genPtr)(t);
                Shrinkable<U> shrinkableU = gen2(rand);
                return make_shrinkable<pair<T, Shrinkable<U>>>(util::make_pair(t, shrinkableU));
            });

        // shrink strategy 2: expand Shrinkable<U>
        intermediate =
            intermediate.andThen(+[](const Shrinkable<Intermediate>& interShr) -> Stream<Shrinkable<Intermediate>> {
                // assume interShr has no shrinks
                Intermediate& interpair = interShr.getRef();
                T& t = interpair.first;
                Shrinkable<U>& shrinkableU = interpair.second;
                Shrinkable<Intermediate> newShrinkableU =
                    shrinkableU.template flatMap<Intermediate>([t](const U& u) mutable {
                        return make_shrinkable<pair<T, Shrinkable<U>>>(util::make_pair(t, make_shrinkable<U>(u)));
                    });
                return newShrinkableU.shrinks();
            });

        // reformat pair<T, Shrinkable<U>> to U
        return intermediate.template flatMap<U>(
            +[](const Intermediate& interpair) -> Shrinkable<U> { return interpair.second; });
    };

    return generator(genU);
}

}  // namespace proptest
