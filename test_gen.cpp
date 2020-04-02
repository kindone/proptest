#include "testing/proptest.hpp"
#include "googletest/googletest/include/gtest/gtest.h"
#include "googletest/googlemock/include/gmock/gmock.h"
#include "testing/Random.hpp"
#include <chrono>
#include <iostream>

using namespace PropertyBasedTesting;

class PropTestCase : public ::testing::Test {
};


template<typename T>
struct NumericTest : public testing::Test
{
    using NumericType = T;
};

template<typename T>
struct SignedNumericTest : public testing::Test
{
    using NumericType = T;
};

template<typename T>
struct IntegralTest : public testing::Test
{
    using NumericType = T;
};

using NumericTypes = testing::Types<int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t, float, double>;
using SignedNumericTypes = testing::Types<int8_t, int16_t, int32_t, int64_t, float, double>;
using IntegralTypes = testing::Types<int8_t, int16_t, int32_t, int64_t, uint8_t, uint16_t, uint32_t, uint64_t>;

TYPED_TEST_CASE(NumericTest, NumericTypes);
TYPED_TEST_CASE(SignedNumericTest, SignedNumericTypes);
TYPED_TEST_CASE(IntegralTest, IntegralTypes);

template <typename T>
T abs(T t) {
    return t;
}

template <>
int8_t abs<int8_t>(int8_t val) {
    return std::abs(val);
}

template <>
int16_t abs<int16_t>(int16_t val) {
    return std::abs(val);
}

template <>
int32_t abs<int32_t>(int32_t val) {
    return std::abs(val);
}

template <>
int64_t abs<int64_t>(int64_t val) {
    return std::abs(val);
}

std::ostream& operator<<(std::ostream& os, const std::vector<int> &input)
{
	os << "[ ";
	for (auto const& i: input) {
		os << i << " ";
	}
	os << "]";
	return os;
}

std::ostream& operator<<(std::ostream& os, const std::vector<int8_t> &input)
{
	os << "[ ";
	for (auto const& i: input) {
		os << static_cast<int>(i) << " ";
	}
	os << "]";
	return os;
}


template<typename ARG1, typename ARG2>
std::ostream& operator << (std::ostream& os, const std::tuple<ARG1, ARG2>& tuple) {
    os << "(" << std::get<0>(tuple) << ", " << std::get<1>(tuple) << ")";
    return os;
}

template<typename ARG1, typename ARG2, typename ARG3>
std::ostream& operator << (std::ostream& os, const std::tuple<ARG1, ARG2, ARG3>& tuple) {
    os << "(" << std::get<0>(tuple) << ", " << std::get<1>(tuple) << ", " << std::get<2>(tuple) << ")";
    return os;
}

TEST(PropTest, GenerateBool) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    Arbitrary<bool> gen;

    for(int i = 0; i < 20; i++) {
        bool val = gen(rand);
        std::cout << val << " " << std::endl;
    }
}


TYPED_TEST(NumericTest, GenNumericType) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    Arbitrary<TypeParam> gen;

    for(int i = 0; i < 20; i++) {
        TypeParam val = gen(rand);
        std::cout << val << " " << abs<TypeParam>(static_cast<TypeParam>(val)) << std::endl;
    }
}

TYPED_TEST(IntegralTest, GenInRange) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    bool isSigned = std::numeric_limits<TypeParam>::min() < 0;

    auto gen0 = isSigned ? inRange<TypeParam>(-10, 10) : inRange<TypeParam>(0, 20);
    TypeParam min = gen0(rand).get();
    TypeParam max = min + rand.getRandomInt32(0,10);
    auto gen = inRange<TypeParam>(min, max);
    std::cout << "min: " << min << ", max: " << max << std::endl;

    for(int i = 0; i < 20; i++) {
        TypeParam val = gen(rand);
        std::cout << val << " " << abs<TypeParam>(static_cast<TypeParam>(val)) << std::endl;
    }
}

TEST(PropTest, GenUTF8String) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    Arbitrary<UTF8String> gen(5);

    for(int i = 0; i < 20; i++) {
        std::cout << "str: \"" << static_cast<UTF8String>(gen(rand)) << "\"" << std::endl;
    }
}

TEST(PropTest, GenLttVectorOfInt) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    Arbitrary<std::vector<int>> gen;
    gen.maxLen = 5;

    for(int i = 0; i < 20; i++) {
        std::vector<int> val(gen(rand));
        std::cout << "vec: ";
        for(size_t j = 0; j < val.size(); j++)
        {
            std::cout << val[j] << ", ";
        }
        std::cout << std::endl;
    }
}

TEST(PropTest, GenShrinks) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    Arbitrary<int> intGen;
    auto evenGen = filter<int>(intGen, [](const int& val) -> bool{
        return val % 2 == 0;
    });

    auto shrinkable = evenGen(rand);
    std::cout << "GenShrinks: " << shrinkable.get() << std::endl;
    auto shrinks = shrinkable.shrinks();
    for(auto itr = shrinks.iterator(); itr.hasNext();) {
        std::cout << "  shrinks: " << itr.next() << std::endl;
    }
}

TEST(PropTest, ShrinkableAndThen) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    Arbitrary<int> intGen;
    auto evenGen = filter<int>(intGen, [](const int& val) -> bool{
        return val % 2 == 0;
    });

    auto evenShrinkable = evenGen(rand);
    std::cout<< "evenShrinkable: " << evenShrinkable.get() << std::endl;
    {
        auto shrinks = evenShrinkable.shrinks();
        while(!shrinks.isEmpty()) {
            auto tempItr = shrinks.iterator();
            tempItr.next(); // ignore first one
            auto shrink = tempItr.hasNext() ? tempItr.next() : shrinks.head();
            std::cout<< "initial: " << shrink.get() << std::endl;
            shrinks = shrink.shrinks();
            for(auto itr = shrinks.iterator(); itr.hasNext(); ) {
                std::cout << "  shrinks: " << itr.next() << std::endl;
            }
        }
    }

    auto andThen = evenShrinkable.andThen([evenShrinkable]() {
        return Stream<Shrinkable<int>>::one(make_shrinkable<int>(1000));
    });

    std::cout<< "andThen: " << andThen.get() << std::endl;
    {
        auto shrinks = andThen.shrinks();
        while(!shrinks.isEmpty()) {
            auto tempItr = shrinks.iterator();
            tempItr.next(); // ignore first one
            auto shrink = tempItr.hasNext() ? tempItr.next() : shrinks.head();
            std::cout<< "andThen: " << shrink.get() << std::endl;
            shrinks = shrink.shrinks();
            for(auto itr = shrinks.iterator(); itr.hasNext(); ) {
                std::cout << "  shrinks: " << itr.next() << std::endl;
            }
        }
    }

    auto concat = evenShrinkable.concat([evenShrinkable]() {
        return evenShrinkable.shrinks();
    });

    std::cout<< "concat: " << concat.get() << std::endl;
    {
        auto shrinks = concat.shrinks();
        while(!shrinks.isEmpty()) {
            auto tempItr = shrinks.iterator();
            tempItr.next(); // ignore first one
            auto shrink = tempItr.hasNext() ? tempItr.next() : shrinks.head();
            std::cout<< "concat: " << shrink.get() << std::endl;
            shrinks = shrink.shrinks();
            for(auto itr = shrinks.iterator(); itr.hasNext(); ) {
                std::cout << "  shrinks: " << itr.next() << std::endl;
            }
        }
    }
}

template <typename T>
void exhaustive(const Shrinkable<T>& shrinkable, int level) {
    for(int i = 0; i < level; i++)
        std::cout << "  ";
    std::cout<< "shrinkable: " << shrinkable.get() << std::endl;

    auto shrinks = shrinkable.shrinks();
    for(auto itr = shrinks.iterator(); itr.hasNext(); ) {
        auto shrinkable2 = itr.next();
        exhaustive(shrinkable2, level + 1);
    }
}

TEST(PropTest, ShrinkableBinary) {
    {
        auto shrinkable = binarySearchShrinkable<int>(0);
        std::cout << "# binary of 0" << std::endl;
        exhaustive(shrinkable, 0);
    }
    {
        auto shrinkable = binarySearchShrinkable<int>(1);
        std::cout << "# binary of 1" << std::endl;
        exhaustive(shrinkable, 0);
    }
    {
        auto shrinkable = binarySearchShrinkable<int>(8);
        std::cout << "# binary of 8" << std::endl;
        exhaustive(shrinkable, 0);
    }

    {
        auto shrinkable = binarySearchShrinkable<int>(7);
        std::cout << "# binary of 7" << std::endl;
        exhaustive(shrinkable, 0);
    }

    {
        auto shrinkable = binarySearchShrinkable<int>(9);
        std::cout << "# binary of 9" << std::endl;
        exhaustive(shrinkable, 0);
    }

    {
        auto shrinkable = binarySearchShrinkable<int>(-1);
        std::cout << "# binary of -1" << std::endl;
        exhaustive(shrinkable, 0);
    }
    {
        auto shrinkable = binarySearchShrinkable<int>(-3);
        std::cout << "# binary of -3" << std::endl;
        exhaustive(shrinkable, 0);
    }
    {
        auto shrinkable = binarySearchShrinkable<int>(-8);
        std::cout << "# binary of -8" << std::endl;
        exhaustive(shrinkable, 0);
    }

    {
        auto shrinkable = binarySearchShrinkable<int>(-7);
        std::cout << "# binary of -7" << std::endl;
        exhaustive(shrinkable, 0);
    }

    {
        auto shrinkable = binarySearchShrinkable<int>(-9);
        std::cout << "# binary of -9" << std::endl;
        exhaustive(shrinkable, 0);
    }
}

TEST(PropTest, ShrinkableConcat) {
    auto shrinkable = binarySearchShrinkable<int>(8);

    auto concat = shrinkable.concat([shrinkable]() {
        return shrinkable.shrinks();
    });

    exhaustive(concat, 0);
}

TEST(PropTest, ShrinkVector) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    using T = int;
    int len = 8;
    std::vector<T> vec;
    vec.reserve(len);
    for(int i = 0; i < len; i++)
        vec.push_back(8);

    //return make_shrinkable<std::vector<T>>(std::move(vec));

    auto shrinkableVector = binarySearchShrinkable<int>(len).template transform<std::vector<T>>([vec](const int& len) {
        if(len <= 0)
            return std::vector<T>();

        auto begin = vec.begin();
        auto last = vec.begin() + len;
        return std::vector<T>(begin, last);;
    });

    auto shrinkableVector2 = shrinkableVector.concat([](const Shrinkable<std::vector<T>>& shr){
        std::vector<T> copy = shr;
        if(!copy.empty())
            copy[0] /= 2;
        return Stream<Shrinkable<std::vector<T>>>(make_shrinkable<std::vector<T>>(copy));
    });

    exhaustive(shrinkableVector, 0);
    exhaustive(shrinkableVector2, 0);
}



TEST(PropTest, ShrinkVectorFromGen) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    using T = int8_t;
    auto genVec = Arbitrary<std::vector<T>>(inRange<T>(-4, 4));
    genVec.maxLen = 4;
    auto vecShrinkable = genVec(rand);
    //return make_shrinkable<std::vector<T>>(std::move(vec));
    exhaustive(vecShrinkable, 0);
}

struct GenSmallInt : public Gen<int32_t> {
    GenSmallInt() : step(0ULL) {
    }
    Shrinkable<int32_t> operator()(Random& rand) {
        constexpr size_t num = sizeof(boundaryValues)/sizeof(boundaryValues[0]);
        return make_shrinkable<int32_t>(boundaryValues[step++ % num]);
    }

    size_t step;
    static constexpr int32_t boundaryValues[13] = {INT32_MIN, 0, INT32_MAX, -1, 1, -2, 2, INT32_MIN+1, INT32_MAX-1, INT16_MIN, INT16_MAX, INT16_MIN+1, INT16_MAX-1};
};

constexpr int32_t GenSmallInt::boundaryValues[13];



TEST(PropTest, TestCheckBasic) {
    check([](const int& a, const int& b) -> bool {
        EXPECT_EQ(a+b, b+a);
        PROP_TAG("a+b > 0", a+b > 0);
        return true;
    });

    check([](int a) -> bool {
        PROP_TAG("a > 0", a > 0);
        return true;
    });

    check([](std::string a, std::string b) -> bool {
        std::string c/*(allocator())*/, d/*(allocator())*/;
        c = a+b;
        d = b+a;
        EXPECT_EQ(c.size(), d.size());
        return true;
    });

    check([](UTF8String a, UTF8String b) -> bool {
        std::string c/*(allocator())*/, d/*(allocator())*/;
        c = a+b;
        d = b+a;
        PROP_ASSERT(c.size() == d.size(), {});
        //EXPECT_EQ(c, d);// << "a: " << a << " + b: " << b << ", a+b: " << (a+b) << ", b+a: " << (b+a);
        return true;
    });
}

TEST(PropTest, TestCheckGen) {

    /*check([](std::vector<int> a) -> bool {
        std::cout << "a: " << a << std::endl;
        PROP_TAG("a.size() > 0", a.size() > 5);
        return true;
    });*/

    // supply custom generator
    check([](int a, int b) -> bool {
        PROP_TAG("a > 0", a > 0);
        PROP_TAG("b > 0", b > 0);
        return true;
    }, GenSmallInt(), GenSmallInt());

    //
    check([](int a, int b) -> bool {
        PROP_TAG("a > 0", a > 0);
        PROP_TAG("b > 0", b > 0);
        return true;
    }, GenSmallInt());

    GenSmallInt genSmallInt;

    check([](int a, int b) -> bool {
        PROP_TAG("a > 0", a > 0);
        PROP_TAG("b > 0", b > 0);
        return true;
    }, genSmallInt, genSmallInt);
}


TEST(PropTest, TupleGen) {
    int64_t seed = getCurrentTime();
    Random rand(seed);
    while(true) {
        auto intGen = Arbitrary<int>();
        auto shrinkable = intGen(rand);
        auto value = shrinkable.get();
        if(value > -20 && value < 20)
        {
            exhaustive(shrinkable, 0);
            break;
        }
    }

    auto smallIntGen = inRange(-40, 40);
    auto tupleGen = tuple(smallIntGen, smallIntGen, smallIntGen);
    while(true) {
        auto shrinkable = tupleGen(rand);
        auto valueTup = shrinkable.get();
        auto arg1 = std::get<0>(valueTup);
        auto arg2 = std::get<1>(valueTup);
        auto arg3 = std::get<2>(valueTup);
        if(arg1 > -20 && arg1 < 20 && arg2 > -20 && arg2 < 20 && arg3 > -20 && arg3 < 20)
        {
            exhaustive(shrinkable, 0);
            break;
        }
    }
}

TEST(PropTest, TestCheckAssert) {

    check([](std::string a, int i, std::string b) -> bool {
        if(i % 2 == 0)
            PROP_DISCARD();
        return true;
    });

    check([](std::string a, int i, std::string b) -> bool {
        if(i % 2 == 0)
            PROP_SUCCESS();
        PROP_DISCARD();
        return true;
    });
}

TEST(PropTest, TestPropertyBasic) {

    property([](std::vector<int> a) -> bool {
        std::cout << "a: " << a << std::endl;
        return true;
    }).check();


    auto func = [](std::vector<int> a) -> bool {
        return true;
    };

    property(func).check();
    check(func);

    auto prop = property([](std::string a, int i, std::string b) -> bool {
        std::cout << "deferred check!" << std::endl;
        PROP_ASSERT(false, {});
        return true;
    });

    // chaining
    prop.setSeed(0).check();
    // with specific arguments
    prop.example(std::string("hello"), 10, std::string("world"));
}

TEST(PropTest, TestPropertyExample) {
    auto func = [](std::string a, int i, std::string b) -> bool {
        std::cout << "deferred check!" << std::endl;
        PROP_ASSERT(false, {});
        return true;
    };
    auto prop = property(func);
    // with specific arguments
    prop.example(std::string("hello"), 10, std::string("world"));
}

TYPED_TEST(SignedNumericTest, TestCheckFail) {

    check([](TypeParam a, TypeParam b/*,std::string str, std::vector<int> vec*/) -> bool {
        PROP_ASSERT(-10 < a && a < 100 && -20 < b && b < 200, {});
        return true;
    });
}

TEST(PropTest, TestStringCheckFail) {

    check([](std::string a) -> bool {
        std::cout << "a: " << a << std::endl;
        PROP_ASSERT(a.size() < 5, {});
        return true;
    });
}

TEST(PropTest, TestVectorCheckFail) {

    std::vector<int> vec;
    vec.push_back(5);
    auto tup = std::make_tuple(vec);
    std::cout << "tuple: ";
    show(std::cout, tup);
    std::cout << std::endl;

    auto vecGen = Arbitrary<std::vector<int>>();
    vecGen.maxLen = 32;

    check([](std::vector<int> a) -> bool {
        std::cout << "a: ";
        show(std::cout, a);
        std::cout << std::endl;
        PROP_ASSERT(a.size() < 5, {});
        return true;
    }, vecGen);
}

TEST(PropTest, TestTupleCheckFail) {

    check([](std::tuple<int, std::tuple<int>> tuple) -> bool {
        std::cout << "tuple: ";
        show(std::cout, tuple);
        std::cout << std::endl;
        int a = std::get<0>(tuple);
        std::tuple<int> subtup = std::get<1>(tuple);
        int b = std::get<0>(subtup);
        PROP_ASSERT((-10 < a && a < 100) || (-20 < b && b < 200), {});
        return true;
    });
}

bool propertyAsFunc(std::string a, int i, std::vector<int> v) {
    return true;
}

class PropertyAsClass {
public:
    bool operator()(std::string a, int i, std::vector<int> v) {
        return true;
    }

    static bool propertyAsMethod(std::string a, int i, std::vector<int> v) {
        return true;
    }
};


TEST(PropTest, TestPropertyFunctionLambdaMethod) {
    property(propertyAsFunc).check();
    check(propertyAsFunc);

    PropertyAsClass propertyAsClass;
    property(propertyAsClass).check();
    check(propertyAsClass);

    property(PropertyAsClass::propertyAsMethod).check();
    check(PropertyAsClass::propertyAsMethod);

}


struct Animal {
    Animal(int f, std::string n, std::vector<int>& m) : numFeet(f), name(n/*, allocator()*/), measures(m/*, allocator()*/) {

    }
    int numFeet;
    std::string name;
    std::vector<int> measures;
};

struct Animal2 {
    Animal2(int f, std::string n) : numFeet(f), name(n/*, allocator()*/) {

    }
    int numFeet;
    std::string name;
};

std::ostream& operator<<(std::ostream& os, const Animal &input)
{
	os << "{ ";
    os << "numFeet: " << input.numFeet;
    os << ", name: " << input.name;
    os << ", measures: " << input.measures;
	os << " }";
	return os;
}



TEST(PropTest, TestConstruct) {
    int64_t seed = getCurrentTime();
    Random rand(seed);

    using AnimalGen = Construct<Animal, int, std::string, std::vector<int>&>;
    auto gen = AnimalGen();
    Animal animal = gen(rand);
    std::cout << "Gen animal: " << animal << std::endl;

    check([](Animal animal) -> bool {
        std::cout << "animal: " << animal << std::endl;
        return true;
    }, gen);
}

namespace PropertyBasedTesting {

// define arbitrary of Animal using Construct
template<>
class Arbitrary<Animal> : public Construct<Animal, int, std::string, std::vector<int>&> {
};

}

TEST(PropTest, TestCheckArbitraryWithConstruct) {
    int64_t seed = getCurrentTime();
    Random rand(seed);

    check([](std::vector<Animal> animals) -> bool {
        if(!animals.empty())
            std::cout << "animal: " << animals[0] << std::endl;
        return true;
    });
}

TEST(PropTest, TestFilter) {
    int64_t seed = getCurrentTime();
    Random rand(seed);

    Filter<Arbitrary<Animal>> filteredGen([](Animal& a) -> bool {
        return a.numFeet >= 0 && a.numFeet < 100 && a.name.size() == 3 && a.measures.size() < 10;
    });

    std::cout << "filtered animal: " << filteredGen(rand) << std::endl;
}


TEST(PropTest, TestFilter2) {
    int64_t seed = getCurrentTime();
    Random rand(seed);

    Arbitrary<int> gen;
    auto evenGen = filter<int>(gen, [](const int& value) {
        return value % 2 == 0;
    });

    for(int i = 0; i < 10; i++) {
        std::cout << "even: " << evenGen(rand) << std::endl;
    }

    auto fours = filter<int>(evenGen, [](const int& value) {
        EXPECT_EQ(value % 2, 0);
        return value % 4 == 0;
    });

    for(int i = 0; i < 10; i++) {
        std::cout << "four: " << fours(rand) << std::endl;
    }

}

TEST(PropTest, TestTransform) {
    int64_t seed = getCurrentTime();
    Random rand(seed);

    Arbitrary<int> gen;
    auto stringGen = transform<int,std::string>(gen, [](const int& value) {
        return "(" + std::to_string(value) + ")";
    });

    for(int i = 0; i < 10; i++) {
        auto shrinkable = stringGen(rand);
        std::cout << "string: " << shrinkable.get() << std::endl;
        int j = 0;
        for(auto itr = shrinkable.shrinks().iterator(); itr.hasNext() && j < 3; j++) {
            auto shrinkable2 = itr.next();
            std::cout << "  shrink: " << shrinkable2.get() << std::endl;
            int k = 0;
            for(auto itr2 = shrinkable2.shrinks().iterator(); itr2.hasNext() && k < 3; k++) {
                std::cout << "    shrink: " << itr2.next().get() << std::endl;
            }
        }

    }

    auto vectorGen = transform<std::string,std::vector<std::string>>(stringGen, [](const std::string& value) {
        std::vector<std::string> vec;
        vec.push_back(value);
        return vec;
    });

    for(int i = 0; i < 10; i++) {
        std::cout << "vector " << vectorGen(rand).get()[0] << std::endl;
    }
}

TEST(PropTest, TestOneOf) {
    auto intGen = Arbitrary<int>();
    auto smallIntGen = GenSmallInt();

    auto gen = oneOf<int>(intGen, smallIntGen);
    int64_t seed = getCurrentTime();
    Random rand(seed);
    for(int i = 0;  i < 10; i++)
        std::cout << gen(rand) << std::endl;
}

TEST(PropTest, TestTuple) {
    auto intGen = Arbitrary<int>();
    auto smallIntGen = GenSmallInt();

    auto gen = tuple(intGen, smallIntGen);
    int64_t seed = getCurrentTime();
    Random rand(seed);
    for(int i = 0;  i < 10; i++)
        std::cout << gen(rand).get() << std::endl;
}


class Complicated {
public:
    int value;
    Complicated(int a) : value(a){
    }

    Complicated(const Complicated&) = delete;
    Complicated(Complicated&&) = default;
private:
    Complicated() {
    }
};


TEST(PropTest, TestShrinkable) {
    auto vec = make_shrinkable<std::vector<int>>(std::vector<int>());
    auto complicated = make_shrinkable<Complicated>(5);

    auto shrink = []() {
        return make_shrinkable<Complicated>(5);
    };

    shrink();
}



