//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxButton.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxButton_p.h"



mxButton::mxButton (mxWindow *parent, int x, int y, int w, int h, const char *label, int id)
: mxWidget (parent, x, y, w, h, label)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxButton_i (p, this);
	d_this->connect (d_this, SIGNAL (clicked ()), d_this, SLOT (clickedEvent ()));
	setHandle ((void *) d_this);
	setType (MX_BUTTON);
	setParent (parent);
	
	setLabel (label);
	setBounds (x, y, w, h);
	setId (id);
	setVisible (true);
}



mxButton::~mxButton ()
{
	delete d_this;
}

