#include "testing/Random.hpp"
#include <random>
#include <limits>
#include <iostream>
#include <chrono>


namespace PropertyBasedTesting {

Random::Random(uint64_t seed) :
    engine(seed)
{
}

Random::Random(const Random& other) :
   engine(other.engine), dist(other.dist) 
{
}


uint64_t Random::next8U() {
    auto value = dist(engine);
    return value; 
}

bool Random::getRandomBool() {
    return (next8U() % 2 == 0);
}

int8_t Random::getRandomInt8() {
    uint8_t unsignedVal = getRandomUInt8();
    return *reinterpret_cast<int8_t*>(&unsignedVal);
}

uint8_t Random::getRandomUInt8() {
    return (next8U() % (std::numeric_limits<uint8_t>::max()+1));
}

int16_t Random::getRandomInt16() {
    uint16_t unsignedVal = getRandomUInt16();
    return *reinterpret_cast<int16_t*>(&unsignedVal);
}

uint16_t Random::getRandomUInt16() {
    return (next8U() % (std::numeric_limits<uint16_t>::max()+1));
}

int32_t Random::getRandomInt32() {
    uint32_t unsignedVal = getRandomUInt32();
    return *reinterpret_cast<int32_t*>(&unsignedVal);

}

uint32_t Random::getRandomUInt32() {
    return (next8U() % (static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())+1));
}

int64_t Random::getRandomInt64() {
    uint64_t unsignedVal = getRandomUInt64();
    return *reinterpret_cast<int64_t*>(&unsignedVal);
}

uint64_t Random::getRandomUInt64() {
    return next8U();
}

uint32_t Random::getRandomSize(size_t fromIncluded, size_t toExcluded) {
    return (next8U() % (toExcluded - fromIncluded)) + fromIncluded;
}


float Random::getRandomFloat() {
    uint32_t intVal = getRandomUInt32();
    return *reinterpret_cast<float*>(&intVal);
}

double Random::getRandomDouble() {
    uint64_t intVal = getRandomUInt64();
    return *reinterpret_cast<double*>(&intVal);
}

int64_t getCurrentTime() {
	auto curTime = std::chrono::system_clock::now();
	auto duration = curTime.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
	return millis;
}

template <>
int8_t Random::getRandom<int8_t>() {
    return getRandomInt8();
}

template <>
int16_t Random::getRandom<int16_t>() {
    return getRandomInt16();
}

template <>
int32_t Random::getRandom<int32_t>() {
    return getRandomInt32();
}

template <>
int64_t Random::getRandom<int64_t>() {
    return getRandomInt64();
}

template <>
uint8_t Random::getRandom<uint8_t>() {
    return getRandomUInt8();
}

template <>
uint16_t Random::getRandom<uint16_t>() {
    return getRandomUInt16();
}

template <>
uint32_t Random::getRandom<uint32_t>() {
    return getRandomUInt32();
}

template <>
uint64_t Random::getRandom<uint64_t>() {
    return getRandomUInt64();
}
} // namespace
