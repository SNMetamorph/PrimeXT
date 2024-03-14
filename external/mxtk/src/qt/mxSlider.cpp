//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxSlider.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxSlider_p.h"



mxSlider::mxSlider (mxWindow *parent, int x, int y, int w, int h, int id, int style)
: mxWidget (parent, x, y, w, h)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxSlider_i (p, this);
	if (style == Horizontal)
	{
		d_this->setOrientation(Qt::Horizontal);
		d_this->setTickPosition (QSlider::TicksRight);
	}
	else if (style == Vertical)
	{
		d_this->setOrientation(Qt::Vertical);
		d_this->setTickPosition (QSlider::TicksBelow);
	}
	d_this->connect (d_this, SIGNAL (valueChanged (int)), d_this, SLOT (valueChangedEvent (int)));

	setHandle ((void *) d_this);
	setType (MX_SLIDER);
	setParent (parent);
	
	setVisible (true);
	setBounds (x, y, w, h);
	setId (id);
}



mxSlider::~mxSlider ()
{
	delete d_this;
}



void
mxSlider::setValue (int value)
{
	QObject::disconnect (d_this, SIGNAL (valueChanged (int)), d_this, SLOT (valueChangedEvent (int)));
	d_this->setValue (value);
	d_this->connect (d_this, SIGNAL (valueChanged (int)), d_this, SLOT (valueChangedEvent (int)));
}



void
mxSlider::setRange (int min, int max)
{
	QObject::disconnect (d_this, SIGNAL (valueChanged (int)), d_this, SLOT (valueChangedEvent (int)));
	d_this->setRange (min, max);
	d_this->connect (d_this, SIGNAL (valueChanged (int)), d_this, SLOT (valueChangedEvent (int)));
}



void
mxSlider::setSteps (int line, int page)
{
	QObject::disconnect (d_this, SIGNAL (valueChanged (int)), d_this, SLOT (valueChangedEvent (int)));
	//d_this->setSteps (line, page);
	d_this->setSingleStep(line);
	d_this->setPageStep(page);
	d_this->connect (d_this, SIGNAL (valueChanged (int)), d_this, SLOT (valueChangedEvent (int)));
}



int
mxSlider::getValue () const
{
	return d_this->value ();
}



int
mxSlider::getMinValue () const
{
	return d_this->minimum ();
}



int
mxSlider::getMaxValue () const
{
	return d_this->maximum ();
}



int
mxSlider::getLineStep () const
{
	return d_this->singleStep ();
}



int
mxSlider::getPageStep () const
{
	return d_this->pageStep ();
}
