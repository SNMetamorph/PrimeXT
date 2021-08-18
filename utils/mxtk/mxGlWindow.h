//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxGlWindow.h
// implementation: all
// last modified:  Apr 21 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#ifndef INCLUDED_MXGLWINDOW
#define INCLUDED_MXGLWINDOW

#include <windows.h>

#ifndef INCLUDED_MXWINDOW
#include <mxWindow.h>
#endif



class mxGlWindow_i;
class mxGlWindow : public mxWindow
{
	mxGlWindow_i *d_this;
	void (*d_drawFunc) (void);

public:
	// ENUMS
	enum { FormatDouble, FormatSingle };

	// CREATORS
	mxGlWindow (mxWindow *parent, int x, int y, int w, int h, const char *label = 0, int style = 0);
	virtual ~mxGlWindow ();

	// MANIPULATORS
	virtual int handleEvent (mxEvent *event);
	virtual void redraw ();
	virtual void draw ();

	int makeCurrent ();
	int swapBuffers ();

	// MANIPULATORS
	void setDrawFunc (void (*func) (void));
private:
	// NOT IMPLEMENTED
	mxGlWindow (const mxGlWindow&);
	mxGlWindow& operator= (const mxGlWindow&);

	void initFakeOpenGL( void );
	void shutdownFakeOpenGL( void );
	int choosePixelFormat( PIXELFORMATDESCRIPTOR *pfd );
protected:
	HWND	hWndFake;
	HDC	hDCFake;
	HGLRC	hGLRCFake;
};



#endif // INCLUDED_MXGLWINDOW
