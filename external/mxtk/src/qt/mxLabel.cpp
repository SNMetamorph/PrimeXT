//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxLabel.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxLabel.h>
#include <QLabel>


class mxLabel_i : public QLabel
{
	mxLabel *d_widget;

public:
	mxLabel_i (QWidget *parent, mxLabel *widget) : QLabel (parent)
	{
		d_widget = widget;
		setAlignment (Qt::AlignLeft | Qt::AlignTop);
	}

	~mxLabel_i ()
	{
	}
};



mxLabel::mxLabel (mxWindow *parent, int x, int y, int w, int h, const char *label)
: mxWidget (parent, x, y, w, h, label)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) ((mxWidget *) parent)->getHandle ();
	d_this = new mxLabel_i (p, this);

	setHandle ((void *) d_this);
	setType (MX_LABEL);
	setParent (parent);
	
	setLabel (label);
	setBounds (x, y, w, h);
	setVisible (true);
}



mxLabel::~mxLabel ()
{
	delete d_this;
}

