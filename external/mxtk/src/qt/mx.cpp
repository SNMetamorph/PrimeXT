//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mx.cpp
// implementation: Qt Free Edition
// last modified:  May 04 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mx.h>
#include <mx/mxWindow.h>
#include <mx/mxLinkedList.h>
#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>
#include <time.h>
#include <unistd.h>


QApplication *d_Application;
mxWindow *d_mainWindow;
mxLinkedList *d_widgetList;
QTimer *d_timer;
static char d_appPath[256];
static int d_argc; // kludge for stupid bug in QCoreApplication::arguments()



void mx_addWidget (mxWidget *widget)
{
	if (d_widgetList)
		d_widgetList->add ((void *) widget);
}



void mx_removeWidget (mxWidget *widget)
{
	if (d_widgetList)
		d_widgetList->remove ((void *) widget);
}



int
mx::init (int argc, char *argv[])
{
	strcpy (d_appPath, argv[0]);
	char *ptr = strrchr (d_appPath, '/');
	if (ptr)
		*ptr = '\0';
		
	d_argc = argc;
	d_Application = new QApplication (d_argc, argv);
	d_widgetList = new mxLinkedList ();
	return 0;
}



int
mx::run ()
{
	return d_Application->exec ();
}



int
mx::check ()
{
	d_Application->processEvents ();
	return 0;
}



void
mx::sleep (unsigned int msec)
{
	usleep (msec * 1000);
}



void
mx::quit ()
{
/*
	if (d_widgetList)
	{

		mxListNode *node = d_widgetList->getLast ();
		while (node)
		{
			mxWidget *widget = (mxWidget *) d_widgetList->getData (node);
			node = d_widgetList->getPrev (node);
			if (widget)
			{
				delete widget;
			}
		}

		delete d_widgetList;
	}
*/
	delete d_mainWindow;
	d_Application->exit (0);
	delete d_Application;
	exit (0);
}



int
mx::setDisplayMode (int w, int h, int bpp)
{
	return 0;
}



void
mx::setIdleWindow (mxWindow *window)
{
	if (d_timer)
	{
		d_timer->stop ();
		delete d_timer;
		d_timer = 0;
	}
	
	if (window)
	{
		QWidget *w = (QWidget *) window->getHandle ();
		d_timer = new QTimer (w);
		QObject::connect (d_timer, SIGNAL (timeout ()), w, SLOT (idleEvent ()));
		d_timer->start (1);
	}
}


 
int
mx::getDisplayWidth ()
{
	return QApplication::desktop ()->width ();
}



int
mx::getDisplayHeight ()
{
	return QApplication::desktop ()->height ();
}




mxWindow*
mx::getMainWindow ()
{
	return d_mainWindow;
}



const char *
mx::getApplicationPath ()
{
	return d_appPath;
}



int
mx::getTickCount ()
{
	return clock () * 1000	/ CLOCKS_PER_SEC;
}

void
mx::cleanup ()
{
}
