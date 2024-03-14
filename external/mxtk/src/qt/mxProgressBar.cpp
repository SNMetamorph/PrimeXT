//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxProgressBar.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxProgressBar.h>
#include <mx/mxWindow.h>
#include <QProgressBar>



class mxProgressBar_i : public QProgressBar
{
	mxProgressBar *d_widget;

public:
	mxProgressBar_i (QWidget *parent, mxProgressBar *widget) : QProgressBar (parent)
	{
		d_widget = widget;
		setRange (0, 100);
		setValue (0);
	}

	~mxProgressBar_i ()
	{
	}
};



mxProgressBar::mxProgressBar (mxWindow *parent, int x, int y, int w, int h, int style)
: mxWidget (parent, x, y, w, h)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxProgressBar_i (p, this);

	setHandle ((void *) d_this);
	setType (MX_PROGRESSBAR);
	setParent (parent);
	setVisible (true);
	setBounds (x, y, w, h);
	
	setTotalSteps (100);
}



mxProgressBar::~mxProgressBar ()
{
	delete d_this;
}



void
mxProgressBar::setValue (int value)
{
	if (value < d_this->value ())
		d_this->reset ();
	d_this->setValue (value);
}



void
mxProgressBar::setTotalSteps (int steps)
{
	d_this->setRange (0, steps);
}



int
mxProgressBar::getValue () const
{
	return d_this->value ();
}



int
mxProgressBar::getTotalSteps () const
{
	return d_this->maximum ();
}
