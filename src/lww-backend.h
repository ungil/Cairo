#ifndef __CAIRO_LWW_BACKEND_H__
#define __CAIRO_LWW_BACKEND_H__

#include "backend.h"

#if CAIRO_HAS_WIN32_SURFACE
#include <cairo-win32.h>
#endif

extern Rcairo_backend_def *RcairoBackendDef_lww;

Rcairo_backend *Rcairo_new_lww_backend(Rcairo_backend *be, const char *display, double width, double height, double umpl, void *hwnd);

#endif
