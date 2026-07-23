#pragma once
#include <cstdint>
#include <string>

enum class FortuneWord : int {
    Yes = 0,
    No = 1,
    Pass = 2
};

struct FortuneVerdict {
    bool flashLeft = true;
    FortuneWord word = FortuneWord::Pass;
};

inline uint32_t digitalRootU64(uint64_t n) {
    if (n == 0) return 9;
    uint32_t r = (uint32_t)(n % 9ull);
    return r == 0 ? 9u : r;
}

inline uint64_t mix64(uint64_t x) {
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ull;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBull;
    x ^= x >> 31;
    return x;
}

inline FortuneVerdict sealFortune(
    uint64_t jiaSum,
    uint64_t fnvHash,
    uint64_t charCount,
    uint64_t oddCount,
    uint32_t dayYmd,
    uint32_t hourSlot)
{
    FortuneVerdict v;
    if (charCount == 0) {
        v.flashLeft = true;
        v.word = FortuneWord::Pass;
        return v;
    }

    const uint32_t root = digitalRootU64(jiaSum);
    const uint32_t dayRoot = digitalRootU64(dayYmd);

    uint64_t h = fnvHash ? fnvHash : 0x9E3779B97F4A7C15ull;
    h ^= mix64(jiaSum ^ 0xA5A5A5A5A5A5A5A5ull);
    h ^= mix64(((uint64_t)dayYmd << 16) | hourSlot);
    h ^= mix64(((uint64_t)root << 8) | (oddCount & 0xFFull));
    h ^= mix64(charCount * 0xD1B54A32D192ED03ull);
    h = mix64(h);

    uint32_t score = (uint32_t)(h % 100ull);
    const int adj = (int)root - 5;
    int s = (int)score + adj;
    if (s < 0) s = 0;
    if (s > 99) s = 99;
    score = (uint32_t)s;

    const bool thinAsk = (charCount < 2);
    const bool voidGate = (((hourSlot + root) % 9u) == (dayRoot % 9u));
    const bool balance = ((oddCount * 2ull + 1ull) == charCount) && (charCount >= 4);

    v.flashLeft = ((jiaSum ^ (uint64_t)root ^ (uint64_t)hourSlot) & 1ull) != 0ull;

    if (thinAsk || voidGate || balance) {
        v.word = FortuneWord::Pass;
        return v;
    }

    const uint32_t noHi = 36u + (dayRoot % 5u);
    const uint32_t passHi = noHi + 22u + (root % 4u);
    if (score < noHi)
        v.word = FortuneWord::No;
    else if (score < passHi)
        v.word = FortuneWord::Pass;
    else
        v.word = FortuneWord::Yes;

    return v;
}

inline float fortuneMinThinkSec(uint64_t charCount) {
    float extra = (float)charCount / 40000.f;
    if (extra > 10.f) extra = 10.f;
    return 2.f + extra;
}
