//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxMenuBar_i.h
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxMenuBar.h>
#include <mx/mxWindow.h>
#include <QMenuBar>

extern QMap<int, QAction*> g_actionList;

class mxMenuBar_i : public QMenuBar
{
	Q_OBJECT
	mxMenuBar *d_widget;
public:
	mxMenuBar_i (QWidget *parent, mxMenuBar *menuBar) : QMenuBar (parent)
	{
		d_widget = menuBar;
	}

	~mxMenuBar_i ()
	{
	}

public slots:
	void activatedEvent (QAction *action)
	{
		int itemId = g_actionList.key(action);
		if (itemId > 0)
		{
			mxWindow *parent = d_widget->getParent ();
			mxEvent event;
			event.event = mxEvent::Action;
			event.action = itemId;
			while (parent)
			{
				if (parent->handleEvent (&event))
					break;
				parent = parent->getParent ();
			}
		}
	}
};
