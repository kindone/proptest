#include "testing/proptest.hpp"
#include "googletest/googletest/include/gtest/gtest.h"
#include "googletest/googlemock/include/gmock/gmock.h"
#include <iostream>

class UtilTestCase : public ::testing::Test {
};


using namespace PropertyBasedTesting;

extern std::ostream& operator<<(std::ostream& os, const std::vector<int> &input);

TEST(UtilTestCase, invokeTest) {
    int arg1 = 5;
    std::vector<int> arg2;
    std::string arg3("hello");

    auto func = [](int i, std::vector<int> v, std::string s) {
        std::cout << "i: " << i << " v: " << v << " s: " << s << std::endl;
    };

    invokeWithArgTuple(func, std::make_tuple(arg1, arg2, arg3));
    invokeWithArgs(func, arg1, arg2, arg3);
}

TEST(UtilTestCase, mapTest) {
    mapTuple(mapTuple(std::make_tuple(5,6,7), [](int i) {
        std::cout << "n: " << i << std::endl;
        return std::to_string(i);
    }), [](std::string s) {
        std::cout << "s: " << s << std::endl;
        return s.size();
    });
}


template <typename IN>
struct Mapper;

template <>
struct Mapper<int> {
    static std::string map(int&& v) {
        std::cout << "Mapper<int> - " << v << std::endl;
        return std::to_string(v + 1);
    }

};

template <>
struct Mapper<std::string> {
    static int map(std::string&& v) {
        std::cout << "Mapper<string> - " << v << std::endl;
        return v.size();
    }

};

TEST(UtilTestCase, mapHeteroTest) {
    mapHeteroTuple<Mapper>(std::make_tuple(5,6,7));
}


TEST(UtilTestCase, mapHeteroTest2) {
    mapHeteroTuple<Mapper>(mapHeteroTuple<Mapper>(std::make_tuple(5,6,7)));
}



TEST(UtilTestCase, mapHeteroTest3) {
    int a = 5;
    std::string b("a");
    mapHeteroTuple<Mapper>(mapHeteroTuple<Mapper>(std::make_tuple(a,b,7)));
}

template <typename T>
struct Bind {
    std::string to_string(int a) {
        return std::to_string(a);
    }
};

template <template <typename> class T, typename P>
decltype(auto) callToString(int value) {
    T<P> t;
    std::bind(&T<P>::to_string, &t, value);
}

TEST(UtilTestCase, stdbind) {
    int arg1 = 5;
    std::vector<int> arg2;
    std::string arg3("hello");

    auto func = [](int i, std::vector<int> v, std::string s) {
        std::cout << "i: " << i << " v: " << v << " s: " << s << std::endl;
    };

    std::bind(func, arg1, arg2, arg3);

    Bind<int> bind;
    std::bind(&Bind<int>::to_string, &bind, 5);

    callToString<Bind,int>(5);
}

template <typename ...ARGS>
decltype(auto) doTuple(std::tuple<ARGS...>& tup) {
    TypeList<typename std::remove_reference<ARGS>::type...> typeList;
    return typeList;
}

TEST(UtilTestCase, TypeList) {
    auto tup = std::make_tuple(1, 2.3, "abc");
    auto res = doTuple(tup);
    using type_tuple = typename decltype(res)::type_tuple;

}