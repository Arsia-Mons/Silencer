// Single translation unit that pulls in Clay's implementation.
// CLAY_IMPLEMENTATION must be defined in exactly one .cpp in the program
// (the header is otherwise declarations only). Keep this file's contents
// minimal — adding code here forces a full Clay re-implementation rebuild.

#define CLAY_IMPLEMENTATION
#include "clay/clay.h"
