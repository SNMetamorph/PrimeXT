//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxEditBox.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxLineEdit_p.h"



mxLineEdit::mxLineEdit (mxWindow *parent, int x, int y, int w, int h, const char
*label, int id, int style)
: mxWidget (parent, x, y, w, h, label)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	d_this = new mxLineEdit_i (p, this);
	d_this->connect (d_this, SIGNAL(textChanged (const QString &)), d_this, SLOT (textChangedEvent (const QString &)));

	if (style == Password)
		d_this->setEchoMode (QLineEdit::Password);

	else if (style == ReadOnly)
		setEnabled (false);
				
	setHandle ((void *) d_this);
	setType (MX_LINEEDIT);
	setParent (parent);
	
	setBounds (x, y, w, h);
	setId (id);
	setLabel (label);
	setVisible (true);
}



mxLineEdit::~mxLineEdit ()
{
	QObject::disconnect (d_this, SIGNAL(textChanged (const QString &)), d_this, SLOT (textChangedEvent (const QString &)));
	d_this->setText ("");
	delete d_this;
}



void
mxLineEdit::setMaxLength (int max)
{
	d_this->setMaxLength (max);
}



int
mxLineEdit::getMaxLength () const
{
	return d_this->maxLength ();
}
