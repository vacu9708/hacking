#include <iostream>
#include <Windows.h>

inline void draw_rect(int left, int top, int width, int height) {
    HDC screenDC = ::GetDC(0);
    int right = left + width, bottom = top + height;
    ::Rectangle(screenDC, left, top, right, bottom);
    ::ReleaseDC(0, screenDC);
}