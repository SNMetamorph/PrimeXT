//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxColorDialog.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxChooseColor.h>
#include <mx/mxWindow.h>
#include <QColorDialog>



bool
mxChooseColor (mxWindow *parent, int *r, int *g, int *b)
{
	QWidget *p = 0;
	if (parent)
	{
		p = (QWidget *) parent->getHandle ();
		QColor cc (*r, *g, *b);
		cc = QColorDialog::getColor (cc, p, "Color");
		*r = cc.red ();
		*g = cc.green ();
		*b = cc.blue ();
		return true;
	}
	return false;
}
