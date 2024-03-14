//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxRadioButton.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxRadioButton_p.h"



QButtonGroup *mxRadioButton_i::s_buttonGroup = 0;



mxRadioButton::mxRadioButton (mxWindow *parent, int x, int y, int w, int h, const char *label, int id, bool newGroup)
: mxWidget (parent, x, y, w, h, label)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxRadioButton_i (p, this, newGroup);
	d_this->connect (d_this, SIGNAL (clicked ()), d_this, SLOT (clickedEvent ()));

	setHandle ((void *) d_this);
	setType (MX_RADIOBUTTON);
	setParent (parent);
	
	setLabel (label);
	setBounds (x, y, w, h);
	setId (id);
	setVisible (true);
	setChecked (newGroup);
}



mxRadioButton::~mxRadioButton ()
{
	delete d_this;
}



void
mxRadioButton::setChecked (bool b)
{
	QObject::disconnect (d_this, SIGNAL (clicked ()), d_this, SLOT (clickedEvent ()));
	d_this->setChecked (b);
	d_this->connect (d_this, SIGNAL (clicked ()), d_this, SLOT (clickedEvent ()));
}



bool
mxRadioButton::isChecked () const
{
	return d_this->isChecked ();
}

