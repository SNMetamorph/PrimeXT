//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxPopupMenu_i.h
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxPopupMenu.h>
#include <mx/mxWindow.h>
#include <QMenu>



class mxPopupMenu_i : public QMenu
{
	Q_OBJECT
	mxPopupMenu *d_widget;

public:
	QMap <int, QAction*> d_actionList;
	int d_lastItemId;
	mxPopupMenu_i (mxPopupMenu *popupMenu) : QMenu ()
	{
		d_widget = popupMenu;
		d_lastItemId = -1;
	}

	~mxPopupMenu_i ()
	{
	}

public slots:
	void activatedEvent (QAction *action)
	{
		d_lastItemId = d_actionList.key(action);
	}
};
