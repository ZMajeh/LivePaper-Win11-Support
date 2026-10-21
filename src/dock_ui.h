#pragma once

#include <SDL.h>

struct Dock;

// The drawing of the bar: the background, the add button, the items and the
// system details on the right.
void DockUiDraw(struct Dock *dock);
