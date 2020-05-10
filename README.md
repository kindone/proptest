# proptest

`proptest` is a property-based testing framework for C++. 

## Property-based testing

Many property-based testing frameworks derive from [QuickCheck](https://en.wikipedia.org/wiki/QuickCheck) in Haskell. 
Its basic idea is to quickly prove a theorem, as the name 'QuickCheck' suggests. 
You can define an abstract property of a component, and let the test framework prove or disprove the property by feeding in massive input combinations. 

This approach is often said to be somewhere in the middle of static analysis and dynamic analysis. Software integrity and defects can be validated in definitive fashion, as in static code analysis, but by actually running the code, as in dynamic code analysis.

A property is in the form of function `(Input0, ... , InputN) -> bool` (or `(Input0, ... , InputN) -> void`)

```cpp
[](int a, int b) -> bool {
    return a + b == b + a;
}
```

A property-based testing framework attempts to generate combinations of `a` and `b` and validate the function whether it always returns `true` for all the combinations. 

> OK, passed 1000 tests

or 

> Falsifiable after 12 tests, where
>   a = 4,
>   b = -4 **(this failure is not actual and only exemplary)**

Among many other benefits, property-based tests can immediately replace dull dummy-based tests, such as:

```cpp
// typical dummy-based test 
TEST(Suite, test) {
    // a text encoded and then decoded must be identical to original
    MyEncoder encoder;
    MyDecoder decoder;    
    auto original = "Hello world";
    auto encoded = encoder.encode(original);
    auto decoded = decoder.decode(encoded);
    ASSERT_EQ(original, decoded);
}
```

This can be turned into a property-based test, which fully tests the component againt arbitrary input strings:

```cpp
// property test 
TEST(Suite, test) {
    check([](std::string original) {
        // a text encoded and then decoded must be identical to original
        MyEncoder encoder;
        MyDecoder decoder;    
        auto encoded = encoder.encode(original);
        auto decoded = decoder.decode(encoded);
        PROP_ASSERT_EQ(original, decoded);
    });
}
```

Here are some of the benefits of turning conventional unit tests into property based tests:


|                   | Conventional Unit Tests   | **Property-Based Tests**     |
| ----------------- |---------------------------| ---------------------------- |
| Paradigm          | Procedural                | Functional                   |
| Test inputs       | Dummy values (with bias)  | Auto-generated combinations  |
| Style             | Concrete & imperative     | Abstract & declarative       |
| Finding bugs      | Less likely               | More likely                  |
| Code Coverage     | Low                       | High                         |
| Readability       | Low                       | High                         |
| Extensibility     |                           |                              |
| &emsp; of params  | No                        | Yes                          |
| &emsp; of mocks   | No                        | Yes                          |
| Encourages change | Yes                       | Yes, even more               |
| Input generation  | Manual                    | Automatic                    |
| Debugging failures| Manual                    | Backed by automatic shrinking|
| Stateful tests    | Manual                    | Semi-automatic               |
| Concurrency tests | Manual                    | Semi-automatic               |
| Developer efforts | More                      | Less                         |

&nbsp;

## `property` and `check`

`property` defines a property with optional configuration and `check` is the shorthand for `Property::check`.
`Property::check` performs property-based test using supplied callable (function, functor, or lambda).

```cpp
check([](int a, int b) -> bool {
    return a + b == b + a;
});
```

is equivalent to

```cpp
property([](int a, int b) -> bool {
    return a + b == b + a;
}).check();
```

### Defining a property
Defining a property requires a callable. For example, a lambda as following is such a callable with an `int` as parameter:

```cpp
[](int a) -> bool {
    return a >= 0;
}
```

Arguments are generated automatically by the framework and the return value of the function indicates success(`true`) or failure(`false`) of a property.  
In above case, the function is called with an integer argument randomly generated by the test framework. 

For a property to be checked, the framework requires generators for parameter types. Either an `Arbitrary<T>` should be defined for a parameter type, or a custom generator should be provided. In above example, a predefined generator `Arbitrary<int>` is used to generate an integer argument.

You can supply a custom generator as additional argument(s) to `property()` function, as following.

```cpp
property([](int a) -> bool {
    return true;
}, myIntGenerator);
```

Many primitive types and containers have their `Arbitrary<T>` defined by the framework for convenience.

&nbsp;

## Generators and Arbitraries

You can use generators to generate randomized arguments for properties.

A generator is a callable (function, functor, or lambda) with following signature:

```cpp
(Random&) -> Shrinkable<T>
```

You can refer to [`Shrinkable`](doc/Shrinking.md) for further detail, but you can basically treat it as a wrapper for a value of type T here. So a generator generates a value of type T from a random generator. A generator can be defined as functor or lambda, as you would prefer.  

```cpp
auto myIntGen = [](Random& rand) {
    int smallInt = rand.getRandomInt8();
    return make_shrinkable<int>(smallInt);
};
```

An `Arbitrary` refers to default generators for a type. You can additionaly define an `Arbitrary<T>` for your type `T`. By defining an `Arbitrary`, you can omit the custom generator argument that was needed to be passed everytime you defined a property for that type. Following shows an example for defining an `Arbitrary`. Note that it should be defined under `PropertyBasedTesting` namespace in order to be accessible in the framework.

```cpp
namespace PropertyBasedTesting {

struct Arbitrary<Car> : Gen<Car> {
  Shrinkable<Car> operator()(Random& rand) {
    bool isAutomatic = rand.getRandomBool();
    return make_shrinkable<Car>(isAutomatic);
  }
};

}
```

There are useful helpers for creating new generators from existing ones. You can find the full list in [Generators](doc/Generators.md) page. 

`suchThat` is such a helper. It selectively generates values that satisfies a criteria function. Following is an even number generator from the integer `Arbitrary`.

```cpp
auto anyIntGen = Arbitrary<int>();
// generates even numbers
auto evenGen = suchThat<int>(anyIntGen, [](const int& num) {
    return num % 2 == 0;
});
```

&nbsp;

## Further topics and details of the framework can be found in:

* [Getting Started](doc/GettingStarted.md)
* [Using and Defining Generators](doc/Generators.md)
* [Counter Examples and Shrinking](doc/Shrinking.md)
* [Stateful Testing with Property-based Testing Framework](doc/StatefulTesting.md)
* [Concurrency Testing with Property-based Testing Framework](doc/ConcurrencyTesting.md)
* [Advanced Mocking with Property-based Testing Framework](doc/Mocking.md)

