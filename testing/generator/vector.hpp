#pragma once

#include "testing/gen.hpp"
#include "testing/Random.hpp"
#include "testing/printing.hpp"
#include "testing/generator/util.hpp"
#include <vector>
#include <iostream>
#include <cmath>
#include <memory>

namespace PropertyBasedTesting
{

template <typename T>
class PROPTEST_API Arbitrary< std::vector<T>> : public Gen< std::vector<T> >
{
public:
    static size_t defaultMinLen;
    static size_t defaultMaxLen;

    Arbitrary() : elemGen(Arbitrary<T>()), minLen(defaultMinLen), maxLen(defaultMaxLen)  {
    }

    Arbitrary(Arbitrary<T> _elemGen)
     : elemGen([_elemGen](Random& rand)->Shrinkable<T>{ return _elemGen(rand); })
     , minLen(defaultMinLen)
     , maxLen(defaultMaxLen) {
    }

    Arbitrary(std::function<Shrinkable<T>(Random&)> _elemGen) : elemGen(_elemGen), minLen(defaultMinLen), maxLen(defaultMaxLen)  {
    }

    using vector_t = std::vector<Shrinkable<T>>;
    using shrinkable_t = Shrinkable<vector_t>;
    using stream_t = Stream<shrinkable_t>;
    using e_stream_t = Stream<Shrinkable<T>>;

    static stream_t shrinkBulk(const shrinkable_t& ancestor, size_t power, size_t offset) {

        static std::function<stream_t(const shrinkable_t&, size_t, size_t, const shrinkable_t&, size_t, size_t, std::shared_ptr<std::vector<e_stream_t>>)> genStream =
        [](const shrinkable_t& ancestor, size_t power, size_t offset, const shrinkable_t& parent, size_t frompos, size_t topos, std::shared_ptr<std::vector<e_stream_t>> elemStreams) {
            const size_t size = topos - frompos;
            if(size == 0)
                return stream_t::empty();

            if(elemStreams->size() != size)
                throw std::runtime_error("element streams size error");

            std::shared_ptr<std::vector<e_stream_t>> newElemStreams = std::make_shared<std::vector<e_stream_t>>();
            newElemStreams->reserve(size);

            vector_t newVec = parent.getRef();
            vector_t& ancestorVec = ancestor.getRef();

            if(newVec.size() != ancestorVec.size())
                throw std::runtime_error("vector size error: " + std::to_string(newVec.size()) + " != " + std::to_string(ancestorVec.size()));

            // shrink each element in frompos~topos, put parent if shrink no longer possible
            bool nothingToDo = true;

            for(size_t i = 0; i < elemStreams->size(); i++) {
                if((*elemStreams)[i].isEmpty()) {
                    newVec[i+frompos] = make_shrinkable<T>(ancestorVec[i+frompos].get());
                    newElemStreams->push_back(e_stream_t::empty());  // [1] -> []
                }
                else {
                    newVec[i+frompos] = (*elemStreams)[i].head();
                    newElemStreams->push_back((*elemStreams)[i].tail()); // [0,4,6,7] -> [4,6,7]
                    nothingToDo = false;
                }
            }
            if(nothingToDo)
                return stream_t::empty();

            auto newShrinkable = make_shrinkable<vector_t>(newVec);
            newShrinkable = newShrinkable.with([newShrinkable, power, offset]() {
                return shrinkBulk(newShrinkable, power, offset);
            });
            return stream_t(newShrinkable, [ancestor, power, offset, newShrinkable, frompos, topos, newElemStreams]() {
                return genStream(ancestor, power, offset, newShrinkable, frompos, topos, newElemStreams);
            });
        };

        size_t parentSize = ancestor.getRef().size();
        size_t numSplits = std::pow(2,power);
        if(parentSize / numSplits < 1)
            return stream_t::empty();

        if(offset >= numSplits)
            throw std::runtime_error("offset should not reach numSplits");

        size_t frompos = parentSize * offset / numSplits;
        size_t topos = parentSize * (offset+1) / numSplits;

        if(topos < parentSize)
            throw std::runtime_error("topos error: " + std::to_string(topos) + " != " + std::to_string(parentSize));

        const size_t size = topos - frompos;
        vector_t& parentVec = ancestor.getRef();
        std::shared_ptr<std::vector<e_stream_t>> elemStreams = std::make_shared<std::vector<e_stream_t>>();
        elemStreams->reserve(size);

        bool nothingToDo = true;
        for(size_t i = frompos; i < topos; i++) {
            auto shrinks = parentVec[i].shrinks();
            elemStreams->push_back(shrinks);
            if(!shrinks.isEmpty())
                nothingToDo = false;
        }

        if(nothingToDo)
            return stream_t::empty();

        return genStream(ancestor, power, offset, ancestor, frompos, topos, elemStreams);
    }

    static stream_t shrinkBulkRecursive(const shrinkable_t& shrinkable, size_t power, size_t offset) {
        if(shrinkable.getRef().empty())
            return stream_t::empty();

        size_t vecSize = shrinkable.getRef().size();
        size_t numSplits = std::pow(2, power);
        if(vecSize / numSplits < 1 || offset >= numSplits)
            return stream_t::empty();
        // entirety
        shrinkable_t newShrinkable = shrinkable.concat([power, offset](const shrinkable_t& shr) {
            size_t vecSize = shr.getRef().size();
            size_t numSplits = std::pow(2, power);
            if(vecSize / numSplits < 1 || offset >= numSplits)
                return stream_t::empty();
            // std::cout << "entire: " << power << ", " << offset << std::endl;
            return shrinkBulk(shr, power, offset);
        });

        // // front part
        // newShrinkable = newShrinkable.concat([power, offset](const shrinkable_t& shr) {
        //     size_t vecSize = shr.getRef().size();
        //     size_t numSplits = std::pow(2, power+1);
        //     if(vecSize / numSplits < 1 || offset*2 >= numSplits)
        //         return stream_t::empty();
        //     // std::cout << "front: " << power << ", " << offset << std::endl;
        //     return shrinkBulkRecursive(shr, power+1, offset*2);
        // });

        // // rear part
        // newShrinkable = newShrinkable.concat([power, offset](const shrinkable_t& shr) {
        //     size_t vecSize = shr.getRef().size();
        //     size_t numSplits = std::pow(2, power+1);
        //     if(vecSize / numSplits < 1 || offset*2+1 >= numSplits)
        //         return stream_t::empty();
        //     // std::cout << "rear: " << power << ", " << offset << std::endl;
        //     return shrinkBulkRecursive(shr, power+1, offset*2+1);
        // });

        return newShrinkable.shrinks();
    }

    Shrinkable<std::vector<T>> operator()(Random& rand) {
        int len = rand.getRandomSize(minLen, maxLen+1);
        std::shared_ptr<vector_t> shrinkVec = std::make_shared<vector_t>();
        shrinkVec->reserve(len);
        for(int i = 0; i < len; i++)
            shrinkVec->push_back(elemGen(rand));

        // shrink vector size with subvector using binary numeric shrink of lengths
        int minLenCopy = minLen;
        auto rangeShrinkable = binarySearchShrinkable<int>(len - minLenCopy).template transform<int>([minLenCopy](const int& len) {
            return len + minLenCopy;
        });
        // this make sure shrinking is possible towards minLen
        shrinkable_t shrinkable = rangeShrinkable.template transform<std::vector<Shrinkable<T>>>([shrinkVec](const int& len) {
            if(len <= 0)
                return make_shrinkable<std::vector<Shrinkable<T>> >();

            auto begin = shrinkVec->begin();
            auto last = shrinkVec->begin() + len; // subvector of (0, len)
            return make_shrinkable<std::vector<Shrinkable<T>>>(begin, last);
        });

        // concat shrinks with parent as argument
        // const auto maxSize = shrinkable.getRef().size();
        // auto genStream = [](size_t i) {
        //     return [i](const shrinkable_t& parent) {
        //         vector_t parentRef = parent.getRef();
        //         const size_t size = parentRef.size();

        //         if(size == 0 || size - 1 < i)
        //             return stream_t::empty();

        //         size_t pos = size - 1 - i;
        //         e_shrinkable_t& elem = parentRef[pos];
        //         // {0,2,3} to {[x,x,x,0], ...,[x,x,x,3]}
        //         // make sure {1} shrinked from 2 is also transformed to [x,x,x,1]
        //         shrinkable_t vecWithElems = elem.template transform<vector_t>([pos, parentRef](const T& val) {
        //             auto copy = parentRef;
        //             copy[pos] = make_shrinkable<T>(val);
        //             return copy;
        //         });
        //         shrinkable_t cropped = vecWithElems;//.take(2);
        //         return cropped.shrinks();
        //     };
        // };

        // for(size_t i = 0; i < maxSize; i++) {
        //     shrinkable = shrinkable.concat(genStream(i));
        // }

        shrinkable = shrinkable.andThen([](const shrinkable_t& shr) {
            return shrinkBulkRecursive(shr, 0, 0);
        });

        auto vecShrinkable = shrinkable.template transform<std::vector<T>>([](const vector_t& shrinkVec) {
            auto value = make_shrinkable<std::vector<T>>();
            std::vector<T>& valueVec = value.getRef();
            std::transform(shrinkVec.begin(), shrinkVec.end(), std::back_inserter(valueVec), [](const Shrinkable<T>& shr) -> T {
                return std::move(shr.getRef());
            });
            return value;
        });

        return vecShrinkable;
    }

    std::function<Shrinkable<T>(Random&)> elemGen;
    int minLen;
    int maxLen;

};

template <typename T>
size_t Arbitrary<std::vector<T>>::defaultMinLen = 0;
template <typename T>
size_t Arbitrary<std::vector<T>>::defaultMaxLen = 200;


} // namespace PropertyBasedTesting



