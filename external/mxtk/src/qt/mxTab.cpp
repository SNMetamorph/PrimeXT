//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxTab.cpp
// implementation: Qt Free Edition
// last modified:  Apr 20 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxTab_p.h"



mxTab::mxTab (mxWindow *parent, int x, int y, int w, int h, int id)
: mxWidget (parent, x, y, w, h)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();
	
	d_this = new mxTab_i (p, this);
	d_this->connect(d_this, SIGNAL(tabBarClicked(int)), d_this, SLOT(tabBarClickedEvent(int)));

	setHandle ((void *) d_this);
	setType (MX_TAB);
	setParent (parent);
	setId (id);
	
	setBounds (x, y, w, h);
	setVisible (true);
}



mxTab::~mxTab ()
{
	delete d_this;
}



void
mxTab::add (mxWidget *widget, const char *text)
{
	if (!widget)
		return;
		
	d_this->addTab ((QWidget *) widget->getHandle (), text);
}



void
mxTab::remove (int index)
{
	d_this->removeTab (index);
	d_this->select (d_this->currentIndex ());
}



void
mxTab::select (int index)
{
	d_this->setCurrentIndex (index);
	d_this->select (index);
}



int
mxTab::getSelectedIndex () const
{
	return d_this->getSelectedIndex ();
}
