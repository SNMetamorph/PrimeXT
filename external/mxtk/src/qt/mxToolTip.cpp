//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxToolTip.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxToolTip.h>
#include <mx/mxWindow.h>
#include <QWidget>


void
mxToolTip::add (mxWidget *widget, const char *text)
{
	QWidget *w = nullptr;
	if (widget)
	{
		w = (QWidget *) widget->getHandle ();
		if(w)
			w->setToolTip(text);
	}
}
