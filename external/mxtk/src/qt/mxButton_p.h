//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxButton_i.h
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxButton.h>
#include <mx/mxWindow.h>
#include <QPushButton>



class mxButton_i : public QPushButton
{
	Q_OBJECT
	mxButton *d_widget;

public:
	mxButton_i (QWidget *parent, mxButton *widget) : QPushButton (parent)
	{
		d_widget = widget;
	}

	~mxButton_i () {}

public slots:
	void clickedEvent ()
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
