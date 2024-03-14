//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxFileDialog.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxFileDialog.h>
#include <mx/mxWindow.h>
#include <QFileDialog>



const char *
mxGetOpenFileName (mxWindow *parent, const char *path, const char *filter)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	static char filename[256];
	QString f = QFileDialog::getOpenFileName (p, "Open File", path, filter);
	if (!f.isEmpty () && !f.isNull ())
	{
		strcpy (filename, qPrintable (f));
		return filename;
	}

	return 0;
}



const char *
mxGetSaveFileName (mxWindow *parent, const char *path, const char *filter)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	static char filename[256];
	QString f = QFileDialog::getSaveFileName (p, "Save File", path, filter);
	if (!f.isEmpty () && !f.isNull ())
	{
		strcpy (filename, qPrintable (f));
		return filename;
	}

	return 0;
}



const char *
mxGetFolderPath (mxWindow *parent, const char *path)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();

	static char filename[256];
	QString f = QFileDialog::getExistingDirectory (p, "Pick a Folder", path);
	if (!f.isEmpty () && !f.isNull ())
	{
		strcpy (filename, qPrintable (f));
		return filename;
	}

	return 0;
}
