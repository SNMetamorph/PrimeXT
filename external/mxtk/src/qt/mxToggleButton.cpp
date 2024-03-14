//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxToggleButton.cpp
// implementation: Qt Free Edition
// last modified:  Apr 28 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxToggleButton_p.h"



mxToggleButton::mxToggleButton (mxWindow *parent, int x, int y, int w, int h, const char *label, int id)
: mxWidget (parent, x, y, w, h, label)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxToggleButton_i (p, this);
	d_this->connect (d_this, SIGNAL (clicked ()), d_this, SLOT (clickedEvent ()));
	d_this->setCheckable(true);
	setHandle ((void *) d_this);
	setType (MX_TOGGLEBUTTON);
	setParent (parent);
	
	setLabel (label);
	setBounds (x, y, w, h);
	setId (id);
	setVisible (true);
}



mxToggleButton::~mxToggleButton ()
{
	delete d_this;
}



void
mxToggleButton::setChecked (bool b)
{
	d_this->setChecked(b);
}



bool
mxToggleButton::isChecked () const
{
	return d_this->isChecked();
}
