//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxChoice_i.h
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxChoice.h>
#include <mx/mxWindow.h>
#include <QComboBox>



class mxChoice_i : public QComboBox
{
	Q_OBJECT
	mxChoice *d_widget;

public:
	int d_currentItem;

	mxChoice_i (QWidget *parent, mxChoice *widget) : QComboBox (parent)
	{
		d_widget = widget;
		d_currentItem = 0;
	}

	~mxChoice_i () {}

public slots:
	void activatedEvent (int index)
	{
		if (d_widget->getId () > 0)
		{
			mxWindow *parent = d_widget->getParent ();
			mxEvent event;
			event.event = mxEvent::Action;
			event.widget = d_widget;
			event.action = d_widget->getId ();
			while (parent)
			{
				if (parent->handleEvent (&event))
					break;
				parent = parent->getParent ();
			}
		}
	}
};

