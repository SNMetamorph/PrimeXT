//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxMessageBox.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxMessageBox.h>
#include <mx/mxWindow.h>
#include <QMessageBox>


int
mxMessageBox (mxWindow *parent, const char *msg, const char *title, int
style)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	int b1 = 0, b2 = 0, b3 = 0;
	
	if (style & MX_MB_OK)
	{
		b1 = QMessageBox::Ok;
	}
	else if (style & MX_MB_YESNO)
	{
		b1 = QMessageBox::Yes;
		b2 = QMessageBox::No;
	}
	else if (style & MX_MB_YESNOCANCEL)
	{
		b1 = QMessageBox::Yes;
		b2 = QMessageBox::No;
		b3 = QMessageBox::Cancel;
	}

	int ret;
	if (style & MX_MB_INFORMATION)
		ret = QMessageBox::information (p, title, msg, b1, b2, b3);
	else if (style & MX_MB_ERROR)
		ret = QMessageBox::critical (p, title, msg, b1, b2, b3);
	else if (style & MX_MB_WARNING)
		ret = QMessageBox::warning (p, title, msg, b1, b2, b3);
	else if (style & MX_MB_QUESTION)
		ret = QMessageBox::question (p, title, msg, b1, b2, b3);

	switch (ret)
	{
	case 1:
	case 3:
		return 0;
			
	case 4:
		return 1;
		
	case 2:
		return 2;
	}
	
	return 0;
}
