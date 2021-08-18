//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxGlWindow.cpp
// implementation: Win32 API
// last modified:  Apr 21 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//

#include <mxGlWindow.h>
#include <GL\gl.h>
#include <GL\wglext.h>
#include <stdio.h>

static BOOL (WINAPI *wglGetPixelFormatAttribiv)( HDC hDC, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttrib, int *piValues);
static BOOL (WINAPI *wglChoosePixelFormat)( HDC hDC, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFmts, int *piFmts, UINT *nNumFmts );

void MsLog (const char *fmt, ...)
{
#if 0	// _DEBUG_
	char	output[1024];
	va_list	argptr;

	va_start( argptr, fmt );
	_vsnprintf( output, sizeof( output ), fmt, argptr );
	va_end( argptr );

	::MessageBox (0, output, "Error", MB_OK|MB_ICONERROR );
#endif
}

class mxGlWindow_i
{
public:
	HDC hdc;
	HGLRC hglrc;
};

mxGlWindow::mxGlWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style)
: mxWindow (parent, x, y, w, h, label, style)
{
	PIXELFORMATDESCRIPTOR pfd;

	d_this = new mxGlWindow_i;

	setType (MX_GLWINDOW);
	setDrawFunc (0);

	bool error = false;

	MsLog ("mxGlWindow, GetDC");
	if(( d_this->hdc = GetDC(( HWND )getHandle( ))) == NULL )
	{
		MsLog( "ERROR: mxGlWindow, GetDC" );
		error = true;
		goto done;
	}

	MsLog( "mxGlWindow, ChoosePixelFormat" );
	int pfm;
	if(( pfm = choosePixelFormat( &pfd )) == 0 ) // g-cont. new style initialization with multisamples
	{
		MsLog ("ERROR: mxGlWindow, ChoosePixelFormat");
		error = true;
		goto done;
	}
	
	MsLog ("mxGlWindow, SetPixelFormat");
	if( ::SetPixelFormat( d_this->hdc, pfm, &pfd ) == FALSE )
	{
		MsLog( "ERROR: mxGlWindow, SetPixelFormat" );
		error = true;
		goto done;
	}

	MsLog ("mxGlWindow, wglCreateContext");	
	if ((d_this->hglrc = wglCreateContext (d_this->hdc)) == 0)
	{
		MsLog ("ERROR: mxGlWindow, wglCreateContext");	
		error = true;
		goto done;
	}

	MsLog ("mxGlWindow, wglMakeCurrent");	
	if (!wglMakeCurrent (d_this->hdc, d_this->hglrc))
	{
		MsLog ("ERROR: mxGlWindow, wglMakeCurrent");	
		error = true;
		goto done;
	}
done:
	if (error)
	{
		::MessageBox(0, "Error creating OpenGL window, please install the latest graphics drivers or Mesa 3.x!", "Error", MB_OK|MB_ICONERROR);
		exit(-1);
	}
	//else if (parent)
	//	parent->addWidget (this);
}



mxGlWindow::~mxGlWindow ()
{
	if (d_this->hglrc)
	{
		wglMakeCurrent (NULL, NULL);
		wglDeleteContext (d_this->hglrc);
	}

	if (d_this->hdc)
		ReleaseDC ((HWND) getHandle (), d_this->hdc);

	delete d_this;
}

/*
==================
initFakeOpenGL

Fake OpenGL stuff to work around the crappy WGL limitations.
Do this silently.
==================
*/
void mxGlWindow :: initFakeOpenGL( void )
{
	WNDCLASSEX		wndClass;
	PIXELFORMATDESCRIPTOR	pfd;
	int			pixelFormat;

	// Register the window class
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = 0;
	wndClass.lpfnWndProc = DefWindowProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = (HINSTANCE)GetModuleHandle( NULL );
	wndClass.hIcon = NULL;
	wndClass.hIconSm = NULL;
	wndClass.hCursor = NULL;
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = "fake";

	if( !RegisterClassEx( &wndClass ))
		return;

	// Create the fake window
	if(( hWndFake = CreateWindowEx( 0, "fake", "mxtk", 0, 0, 0, 100, 100, NULL, NULL, wndClass.hInstance, NULL )) == NULL )
	{
		UnregisterClass( "fake", wndClass.hInstance );
		return;
	}

	// Get a DC for the fake window
	if(( hDCFake = GetDC( hWndFake )) == NULL )
		return;

	// Choose a pixel format
	memset( &pfd, 0, sizeof( PIXELFORMATDESCRIPTOR ));

	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.iLayerType = PFD_MAIN_PLANE;
	pfd.cColorBits = 32;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;

	pixelFormat = ChoosePixelFormat( hDCFake, &pfd );
	if( !pixelFormat ) return;

	// Set the pixel format
	if( !SetPixelFormat( hDCFake, pixelFormat, &pfd ))
		return;

	// Create the fake GL context
	if(( hGLRCFake = wglCreateContext( hDCFake )) == NULL )
		return;

	// Make the fake GL context current
	if( !wglMakeCurrent( hDCFake, hGLRCFake ))
		return;

	// We only need these function pointers if available
	wglGetPixelFormatAttribiv = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProcAddress( "wglGetPixelFormatAttribivARB" );
	wglChoosePixelFormat = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress( "wglChoosePixelFormatARB" );
}

/*
==================
shutdownFakeOpenGL

Fake OpenGL stuff to work around the crappy WGL limitations.
Do this silently.
==================
*/
void mxGlWindow :: shutdownFakeOpenGL( void )
{
	if( hGLRCFake )
	{
		wglMakeCurrent( NULL, NULL );
		wglDeleteContext( hGLRCFake );
		hGLRCFake = NULL;
	}

	if( hDCFake )
	{
		ReleaseDC( hWndFake, hDCFake );
		hDCFake = NULL;
	}

	if( hWndFake )
	{
		DestroyWindow( hWndFake );
		hWndFake = NULL;
		UnregisterClass( "fake", GetModuleHandle( NULL ));
	}

	wglGetPixelFormatAttribiv = NULL;
	wglChoosePixelFormat = NULL;
}

/*
==================
choosePixelFormat
==================
*/
int mxGlWindow :: choosePixelFormat( PIXELFORMATDESCRIPTOR *pfd )
{
	int		attribs[24];
	int		samples = 0;
	int		pixelFormat = 0;
	int		numPixelFormats;

	// Initialize the fake OpenGL stuff so that we can use the extended pixel
	// format functionality
	initFakeOpenGL();

	// Choose a pixel format
	if( wglChoosePixelFormat == NULL )
	{
		memset( pfd, 0, sizeof( PIXELFORMATDESCRIPTOR ));

		pfd->nSize = sizeof( PIXELFORMATDESCRIPTOR );
		pfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd->iPixelType = PFD_TYPE_RGBA;
		pfd->iLayerType = PFD_MAIN_PLANE;
		pfd->cColorBits = 32;
		pfd->cAlphaBits = 0;
		pfd->cDepthBits = 24;
		pfd->cStencilBits = 8;
		pfd->nVersion = 1;

		pixelFormat = ChoosePixelFormat( d_this->hdc, pfd );
	}
	else
	{
		attribs[0] = WGL_ACCELERATION_ARB;
		attribs[1] = WGL_FULL_ACCELERATION_ARB;
		attribs[2] = WGL_DRAW_TO_WINDOW_ARB;
		attribs[3] = TRUE;
		attribs[4] = WGL_SUPPORT_OPENGL_ARB;
		attribs[5] = TRUE;
		attribs[6] = WGL_DOUBLE_BUFFER_ARB;
		attribs[7] = TRUE;
		attribs[8] = WGL_PIXEL_TYPE_ARB;
		attribs[9] = WGL_TYPE_RGBA_ARB;
		attribs[10] = WGL_COLOR_BITS_ARB;
		attribs[11] = 32;
		attribs[12] = WGL_ALPHA_BITS_ARB;
		attribs[13] = 0;
		attribs[14] = WGL_DEPTH_BITS_ARB;
		attribs[15] = 24;
		attribs[16] = WGL_STENCIL_BITS_ARB;
		attribs[17] = 8;
		attribs[18] = WGL_SAMPLE_BUFFERS_ARB;
		attribs[19] = 1;
		attribs[20] = WGL_SAMPLES_ARB;
		attribs[21] = 4;
		attribs[22] = 0;
		attribs[23] = 0;

		wglChoosePixelFormat( d_this->hdc, attribs, NULL, 1, &pixelFormat, (UINT *)&numPixelFormats );

		if( pixelFormat )
		{
			samples = WGL_SAMPLES_ARB;
			wglGetPixelFormatAttribiv( d_this->hdc, pixelFormat, 0, 1, &samples, &samples );
		}
	}

	// shutdown the fake OpenGL stuff since we no longer need it
	shutdownFakeOpenGL();

	// Make sure we have a valid pixel format
	if( !pixelFormat ) return 0;

	// Describe the pixel format
	DescribePixelFormat( d_this->hdc, pixelFormat, sizeof( PIXELFORMATDESCRIPTOR ), pfd );

	if( pfd->dwFlags & ( PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED ))
	{
		MsLog( "...no hardware acceleration found\n" );
		return 0;
	}

	MsLog( "...hardware acceleration found\n" );

	if( pfd->cColorBits < 32 || pfd->cDepthBits < 24 || pfd->cStencilBits < 8 )
	{
		MsLog( "...insufficient pixel format\n" );
		return 0;
	}

	// report if multisampling is desired but unavailable
	if( samples <= 1 ) MsLog( "...failed to select a multisample pixel format\n" );

	return pixelFormat;
}

int
mxGlWindow::handleEvent (mxEvent *event)
{
	return 0;
}



void
mxGlWindow::redraw ()
{
	makeCurrent ();
	if (d_drawFunc)
		d_drawFunc ();
	else
		draw ();
	swapBuffers ();
}



void
mxGlWindow::draw ()
{
}



int
mxGlWindow::makeCurrent ()
{
	if (wglMakeCurrent (d_this->hdc, d_this->hglrc))
		return 1;

	return 0;
}



int
mxGlWindow::swapBuffers ()
{
	if (SwapBuffers (d_this->hdc))
		return 1;

	return 0;
}



void
mxGlWindow::setDrawFunc (void (*func) (void))
{
	d_drawFunc = func;
}