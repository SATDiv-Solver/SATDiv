#include <utility>
#include <iostream>
#include <cstring>
#include <cstdint>

class MyBitSet {
public:
    MyBitSet(): bitmap(nullptr), siz(0), cap(0) {}
    explicit MyBitSet(size_t _siz): siz(_siz) {
        cap = (_siz + 63) >> 6;
        bitmap = new uint64_t[cap]{0};
    }
    MyBitSet(const MyBitSet& bs): siz(bs.siz), cap(bs.cap) {
        bitmap = new uint64_t[bs.cap]{0};
        memcpy(bitmap, bs.bitmap, sizeof(uint64_t) * cap);
    }
    MyBitSet(MyBitSet&& bs) noexcept
        : bitmap(std::exchange(bs.bitmap, nullptr)),
          siz(std::exchange(bs.siz, 0)),
          cap(std::exchange(bs.cap, 0)) {}
    ~MyBitSet() { Free(); }

    void Free() {
        delete [] bitmap;
        bitmap = nullptr;
    }

    MyBitSet& operator = (const MyBitSet &bs) {
        if (this == &bs) return *this;
        Free();
        siz = bs.siz;
        cap = bs.cap;
        bitmap = new uint64_t[cap]{0};
        memcpy(bitmap, bs.bitmap, sizeof(uint64_t) * cap);
        return *this;
    }

    MyBitSet& operator = (MyBitSet &&bs) noexcept {
        if (this == &bs) return *this;
        swap(bs);
        return *this;
    }

    void swap(MyBitSet &bs) noexcept {
        std::swap(bitmap, bs.bitmap);
        std::swap(cap, bs.cap);
        std::swap(siz, bs.siz);
    }

    MyBitSet& operator |= (const MyBitSet &bs) {
        size_t len = cap < bs.cap ? cap : bs.cap;
        for (size_t i = 0; i < len; i ++)
            bitmap[i] |= bs.bitmap[i];
        return *this;
    }

    MyBitSet& operator &= (const MyBitSet &bs) {
        size_t len = cap < bs.cap ? cap : bs.cap;
        for (size_t i = 0; i < len; i ++)
            bitmap[i] &= bs.bitmap[i];
        return *this;
    }

    MyBitSet& operator ^= (const MyBitSet &bs) {
        size_t len = cap < bs.cap ? cap : bs.cap;
        for (size_t i = 0; i < len; i ++)
            bitmap[i] ^= bs.bitmap[i];
        return *this;
    }

    MyBitSet& operator %= (const MyBitSet &bs) { // |= ~bs
        size_t len = cap < bs.cap ? cap : bs.cap;
        for (size_t i = 0; i < len; i ++)
            bitmap[i] |= ~bs.bitmap[i];
        return *this;
    }

    bool set(size_t idx) {
        uint64_t& t1 = bitmap[idx >> 6];
        uint64_t cg = (1ull << (idx & 63));
        if ((t1 & cg) == 0ull){
            t1 |= cg;
            return true;
        }
        return false;
    }

    bool unset(size_t idx) {
        uint64_t& t1 = bitmap[idx >> 6];
        uint64_t cg = (1ull << (idx & 63));
        if ((t1 & cg) != 0ull){
            t1 ^= cg;
            return true;
        }
        return false;
    }

    bool get(size_t idx) const {
        return (bitmap[idx >> 6] >> (idx & 63)) & 1;
    }

    int getone() const {
        int cnt = 0;
        for (size_t i = 0; i < cap; i ++)
            cnt += __builtin_popcountll(bitmap[i]);
        return cnt;
    }

    friend std::ostream& operator << (std::ostream& os, const MyBitSet &bs) {
        for (size_t i = 0; i < bs.siz; i ++)
            os << (i ? " " : "") << bs.get(i);
        return os;
    }

    void print(std::ostream& os = std::cout) {
        os << *this << '\n';
    }

private:
    uint64_t* bitmap;
    size_t siz, cap;
};
