#include "text_measure.h"

namespace ui {

namespace {
MeasureTextFn g_measurer = nullptr;
} // namespace

void set_text_measurer(MeasureTextFn fn) { g_measurer = fn; }
MeasureTextFn text_measurer() { return g_measurer; }

} // namespace ui
