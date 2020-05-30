#pragma once

#include <memory>
#include "../gen.hpp"
#include "../Shrinkable.hpp"
#include "../Random.hpp"
#include "../util/nullable.hpp"

namespace PropertyBasedTesting {

template <typename T>
class Arbitrary<Nullable<T>> final : public Gen<Nullable<T>> {
public:
    Arbitrary() : elemGen(Arbitrary<T>()) {}

    Arbitrary(Arbitrary<T>& _elemGen)
        : elemGen([_elemGen](Random& rand) mutable -> Shrinkable<T> { return _elemGen(rand); })
    {
    }

    Arbitrary(std::function<Shrinkable<T>(Random&)> _elemGen) : elemGen(_elemGen) {}

    Shrinkable<Nullable<T>> operator()(Random& rand) override
    {
        auto gen = Arbitrary<T>();
        // not null
        if (rand.getRandomBool(0.95)) {
            Shrinkable<T> shrinkable = gen(rand);
            return shrinkable.template transform<Nullable<T>>(
                [](const Shrinkable<T>& shr) { return make_shrinkable<Nullable<T>>(shr.getSharedPtr()); });
        } else {
            // null
            return make_shrinkable<Nullable<T>>();
        }
    }

    std::function<Shrinkable<T>(Random&)> elemGen;
};

}  // namespace PropertyBasedTesting