//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxGroupBox.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxGroupBox.h>
#include <QGroupBox>


class mxGroupBox_i : public QGroupBox
{
	mxGroupBox *d_widget;

public:
	mxGroupBox_i (QWidget *parent, mxGroupBox *widget) : QGroupBox (parent)
	{
		d_widget = widget;
	}

	~mxGroupBox_i ()
	{
	}
};



mxGroupBox::mxGroupBox (mxWindow *parent, int x, int y, int w, int h, const char *label)
: mxWidget (parent, x, y, w, h, label)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) ((mxWidget *) parent)->getHandle ();
	d_this = new mxGroupBox_i (p, this);

	setHandle ((void *) d_this);
	setType (MX_GROUPBOX);
	setParent (parent);
	
	setLabel (label);
	setBounds (x, y, w, h);
	setVisible (true);
}



mxGroupBox::~mxGroupBox ()
{
	delete d_this;
}
