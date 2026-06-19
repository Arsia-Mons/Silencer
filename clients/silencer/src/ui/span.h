#pragma once

namespace ui {

template <typename T>
struct Span {
    T *items = nullptr;
    int count = 0;

    T *begin() const { return items; }
    T *end() const { return items + count; }
    T &operator[](int index) const { return items[index]; }
};

} // namespace ui
