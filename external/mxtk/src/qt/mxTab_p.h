//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxTab_i.h
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxTab.h>
#include <mx/mxWindow.h>
#include <QTabWidget>



class mxTab_i : public QTabWidget
{
	Q_OBJECT
	mxTab *d_widget;
	int d_currentItem;	

public:
	mxTab_i (QWidget *parent, mxTab *widget) : QTabWidget (parent)
	{
		d_widget = widget;
		d_currentItem = 0;
	}
	
	void select (int index)
	{
		d_currentItem = index;
	}

	int getSelectedIndex () const
	{
		return d_currentItem;
	}

	~mxTab_i ()
	{
	}

public slots:
	void tabBarClickedEvent (int index)
	{
		d_currentItem = index;

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
