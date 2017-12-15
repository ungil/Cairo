/* -*- mode: C; tab-width: 4; c-basic-offset: 4 -*-
   Created 4/11/2007 by Simon Urbanek
   Copyright (C) 2007        Simon Urbanek

   License: GPL v2

   Modified by Carlos Ungil (C) 2017 for embedding in LispWorks CAPI.
*/

#include <stdlib.h>
#include <string.h>
#include "lww-backend.h"

#if CAIRO_HAS_WIN32_SURFACE

void *backend;

static const char *types_list[] = { "win", 0 };
static Rcairo_backend_def RcairoBackendDef_ = {
    BET_W32,
    types_list,
	"Windows",
    CBDF_VISUAL,
    0
};
Rcairo_backend_def *RcairoBackendDef_lww = &RcairoBackendDef_;

typedef struct {
	Rcairo_backend *be; /* back-link */
	HWND    wh;         /* window handle */
	HDC     cdc;        /* cache device context */
	HDC     hdc;        /* window device context */
	HBITMAP cb;         /* cache bitmap */
	int     width;
	int     height;
} Rcairo_lww_data;


/*---- save page ----*/
static void lww_save_page(Rcairo_backend* be, int pageno){
	cairo_show_page(be->cc);
	cairo_set_source_rgba(be->cc,1,1,1,1);
	cairo_reset_clip(be->cc);
	cairo_new_path(be->cc);
	cairo_paint(be->cc);
}

/*---- resize ----*/
static void lww_resize(Rcairo_backend* be, double width, double height){
	Rcairo_lww_data *xd = (Rcairo_lww_data *) be->backendSpecific;
	RECT r;
	HBITMAP ob;
	HDC hdc;
	if (!xd) return;
	if (xd->cdc) {
		DeleteDC(xd->cdc);
		DeleteObject(xd->cb);
	}

	if (be->cs) {
		cairo_destroy(be->cc); be->cc=0;
		cairo_surface_destroy(be->cs); be->cs=0;
	}

	be->width=xd->width=width;
	be->height=xd->height=height;

	/* manage HDC -- we have to re-get it after resize due to clipping issues */
	if (!xd->hdc)
		xd->hdc = GetDC( xd->wh );
	else {
		ReleaseDC(xd->wh, xd->hdc);
		xd->hdc = GetDC( xd->wh );
	}
	hdc=xd->hdc;
	/* first re-paint the window */
	GetClientRect( xd->wh, &r );
	be->cs = cairo_win32_surface_create( hdc );
	be->cc = cairo_create( be->cs );
	Rcairo_backend_repaint(be);
	cairo_copy_page(be->cc);
	cairo_surface_flush(be->cs);

	/* then create a cached copy */
	xd->cdc = CreateCompatibleDC( hdc );
	xd->cb = CreateCompatibleBitmap( hdc, r.right, r.bottom );
	ob = SelectObject(xd->cdc, xd->cb);
	BitBlt(xd->cdc, 0, 0, r.right, r.bottom, hdc, 0, 0, SRCCOPY);
}

/*---- mode ---- (-1=replay finished, 0=done, 1=busy, 2=locator) */
static void lww_mode(Rcairo_backend* be, int which){
	Rcairo_lww_data *xd = (Rcairo_lww_data *) be->backendSpecific;
	if (be->in_replay) return;
	if (which < 1) {
		cairo_copy_page(be->cc);
		cairo_surface_flush(be->cs);

		/* upate cache */
		if (xd->cdc) {
			DeleteDC(xd->cdc);
			DeleteObject(xd->cb);
		}
		xd->cdc = CreateCompatibleDC( xd->hdc );
		xd->cb = CreateCompatibleBitmap( xd->hdc, xd->width, xd->height );
		SelectObject(xd->cdc, xd->cb);
		BitBlt(xd->cdc, 0, 0, xd->width, xd->height, xd->hdc, 0, 0, SRCCOPY);
	}
}

/*---- locator ----*/
int  lww_locator(struct st_Rcairo_backend *be, double *x, double *y) {
	Rcairo_lww_data *cd = (Rcairo_lww_data *) be->backendSpecific;
	return 0;
}

/*---- destroy ----*/
static void lww_backend_destroy(Rcairo_backend* be)
{
	Rcairo_lww_data *xd = (Rcairo_lww_data *) be->backendSpecific;

	if (xd->cdc) {
		DeleteDC(xd->cdc); xd->cdc=0;
		DeleteObject(xd->cb); xd->cb=0;
	}
	if (xd->wh) {
		DestroyWindow(xd->wh);
	}
	cairo_surface_destroy(be->cs);
	cairo_destroy(be->cc);	
	free(be);
}

/*-----------------*/

static int inited_w32 = 0;

typedef struct w32chain_s {
	HWND w;
	Rcairo_backend *be;
	struct w32chain_s *next;
} w32chain_t;

static w32chain_t wchain;

Rcairo_backend *Rcairo_new_lww_backend(Rcairo_backend *be, const char *display, double width, double height, double umpl, void *hwnd)
{
	Rcairo_lww_data *xd;
	w32chain_t *l = &wchain;

	if ( ! (xd = (Rcairo_lww_data*) calloc(1,sizeof(Rcairo_lww_data)))) {
		free(be);
		return NULL;
	}

	be->backend_type = BET_W32;
	be->backendSpecific = xd;
	xd->be = be;
	be->destroy_backend = lww_backend_destroy;
	be->save_page = lww_save_page;
	be->resize = lww_resize;
	be->mode = lww_mode;
	be->locator = lww_locator;
	be->truncate_rect = 1;

	if (be->dpix<=0) { /* try to find out the DPI setting */
		HWND dw = GetDesktopWindow();
		if (dw) {
			HDC  dc = GetDC(dw);
			if (dc) {
				int dpix = GetDeviceCaps(dc, LOGPIXELSX);
				int dpiy = GetDeviceCaps(dc, LOGPIXELSY);
				ReleaseDC(dw, dc);
				if (dpix>0) be->dpix=(double)dpix;
				if (dpiy>0) be->dpiy=(double)dpiy;
			}
		}
	}
	/* adjust width and height to be in pixels */
	if (umpl>0 && be->dpix<=0) {
		be->dpix = 96; be->dpiy = 96;
	}
	if (be->dpiy==0 && be->dpix>0) be->dpiy=be->dpix;
	if (umpl>0) {
		width = width * umpl * be->dpix;
		height = height * umpl * be->dpiy;
		umpl=-1;
	}
	if (umpl!=-1) {
		width *= (-umpl);
		height *= (-umpl);
	}

	be->width = width;
	be->height = height;

	while (l->be && l->next) l=l->next;
	if (l->be) { l->next = (w32chain_t*) calloc(1,sizeof(w32chain_t)); l = l->next; }

	l->be = be;	

	l->w = xd->wh = hwnd;
	backend = be;

	ShowWindow(xd->wh, SW_SHOWNORMAL);

	lww_resize(be, width, height);

	UpdateWindow(xd->wh);

	return be;
}

int backend_resize_lw(Rcairo_backend *be, double width, double height) {
	Rcairo_lww_data *xd = (Rcairo_lww_data *) be->backendSpecific;
	RECT r;
	HBITMAP ob;
	HDC hdc;
	if (!xd) return -1;
	if (xd->cdc) {
		DeleteDC(xd->cdc);
		DeleteObject(xd->cb);
	}
	if (be->cs) {
		cairo_destroy(be->cc); be->cc=0;
		cairo_surface_destroy(be->cs); be->cs=0;
	}
	be->width=xd->width=width;
	be->height=xd->height=height;
	Rcairo_backend_resize(be,width,height);
	/* manage HDC -- we have to re-get it after resize due to clipping issues */
	if (!xd->hdc)
		xd->hdc = GetDC( xd->wh );
	else {
		ReleaseDC(xd->wh, xd->hdc);
		xd->hdc = GetDC( xd->wh );
	}
	hdc=xd->hdc;
	/* first re-paint the window */
	GetClientRect(xd->wh,&r);
	be->cs=cairo_win32_surface_create(hdc);
	be->cc=cairo_create(be->cs);
	Rcairo_backend_repaint(be);
	//cairo_reset_clip(be->cc); //?
	cairo_copy_page(be->cc);
	cairo_surface_flush(be->cs);
	/* then create a cached copy */
	xd->cdc = CreateCompatibleDC(hdc);
	xd->cb = CreateCompatibleBitmap(hdc,r.right,r.bottom);
	ob = SelectObject(xd->cdc,xd->cb);
	BitBlt(xd->cdc, 0, 0, r.right, r.bottom, hdc, 0, 0, SRCCOPY);
	return 0;
}
#else
Rcairo_backend_def *RcairoBackendDef_lww = 0;

Rcairo_backend *Rcairo_new_lww_backend(Rcairo_backend *be, const char *display, double width, double height, double umpl, void *hwnd)
{
	error("cairo library was compiled without lispworks win32 back-end.");
	return NULL;
}
#endif
