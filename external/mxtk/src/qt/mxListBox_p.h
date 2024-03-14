//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxListBox_i.h
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxListBox.h>
#include <mx/mxWindow.h>
#include <QListWidget>



class mxListBox_i : public QListWidget
{
	Q_OBJECT
	mxListBox *d_widget;

public:
	mxListBox_i (QWidget *parent, mxListBox *widget) : QListWidget (parent)
	{
		d_widget = widget;
	}

	~mxListBox_i ()
	{
	}

public slots:
	void selectedEvent ()
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
				if (d_widget->getParent ()->handleEvent (&event))
					break;
				parent = parent->getParent ();
			}
		}
	}
};
