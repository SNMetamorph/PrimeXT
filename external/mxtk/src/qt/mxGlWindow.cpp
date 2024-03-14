//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxGlWindow.cpp
// implementation: Qt Free Edition
// last modified:  Apr 20 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxGlWindow_p.h"



mxGlWindow::mxGlWindow (mxWindow *parent, int x, int y, int w, int h, const char *label,
int style)
: mxWindow (parent, x, y, w, h, label, style)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxGlWindow_i (p, this);

	setHandle ((void *) d_this);
	setType (MX_GLWINDOW);
	setParent (parent);
	
	setVisible (true);
	setLabel (label);
	setBounds (x, y, w, h);
}



mxGlWindow::~mxGlWindow ()
{
	//d_this->killTimer ();
	delete d_this;
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
	d_this->makeCurrent ();
	return 1;
}



int
mxGlWindow::swapBuffers ()
{
	d_this->context()->swapBuffers(d_this->context()->surface());
	return 1;
}



void
mxGlWindow::setFormat (int mode, int colorBits, int depthBits)
{
}
