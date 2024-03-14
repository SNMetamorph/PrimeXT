//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxChoice.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxChoice_p.h"



mxChoice::mxChoice (mxWindow *parent, int x, int y, int w, int h, int id)
: mxWidget (parent, x, y, w, h)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxChoice_i (p, this);
	d_this->connect (d_this, SIGNAL (activated (int)), d_this, SLOT (activatedEvent (int)));

	setHandle ((void *) d_this);
	setParent (parent);
	setType (MX_CHOICE);
	setVisible (true);
	setBounds (x, y, w, h);
	setId (id);
}



mxChoice::~mxChoice ()
{
	removeAll ();
	delete d_this;
}



void
mxChoice::add (const char *item)
{
	d_this->addItem (item);
}



void
mxChoice::select (int index)
{
	QObject::disconnect (d_this, SIGNAL (activated (int)), d_this, SLOT (activatedEvent (int)));
	d_this->d_currentItem = index;
	d_this->setCurrentIndex (index);
	d_this->connect (d_this, SIGNAL (activated (int)), d_this, SLOT (activatedEvent (int)));
}



void
mxChoice::remove (int index)
{
	d_this->removeItem (index);
}



void
mxChoice::removeAll ()
{
	d_this->clear ();
}



int
mxChoice::getItemCount () const
{
	return d_this->count ();
}



int
mxChoice::getSelectedIndex () const
{
	return d_this->currentIndex ();
}

