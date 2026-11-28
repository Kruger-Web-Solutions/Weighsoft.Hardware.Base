#pragma once

#include <stddef.h>

// One heap object per (type, tag), made on the FIRST call and never freed.
//
// Why: the yellow board (ESP32, no PSRAM) ran out of static DRAM by about 105 KB once the
// packer work landed (28 Nov 2026), so the big packer / Hi-Lo buffers live on the heap.
// They must NOT be made by global constructors: at that point the ESP32 heap is only a small
// part of RAM (the start-up stacks are not handed back yet) and `new` aborted at boot.
// Each owner calls its buffers once at the top of its *_begin() (from setup(), heap whole),
// so a later call only returns the pointer: no allocation inside a spinlock, none after the
// heap has had time to fragment.
//
// Tags are global across files (one template instance per type + tag): keep them unique.
// packers.cpp 1-19, hilo.cpp 20-29, hub.cpp 30-39.
template <typename T, int TAG>
T &wsoft_heap_once() {
  static T *p = new T();
  return *p;
}

template <typename T, size_t COUNT, int TAG>
T *wsoft_heap_array_once() {
  static T *p = new T[COUNT]();
  return p;
}
