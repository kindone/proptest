#ifndef TESTING_PROP_FILTER_HPP
#define TESTING_PROP_FILTER_HPP

#include "testing/gen.hpp"

namespace PropertyBasedTesting {



template <typename GenT>
class Filter : public GenT {
public:
    using T = typename GenT::type;
    using FilterFunc = std::function<bool(T&)>;

    Filter(FilterFunc&& f) : filter(f) {
    }
    
    Shrinkable<T> generate(Random& rand) {
        while(true) {
            auto shrinkable = gen.generate(rand);
            if(filter(shrinkable.getRef())) {
                return shrinkable;
            }
        }
    }

    GenT gen;
    FilterFunc filter;
};

} // namespace
#endif