//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxMenu.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxMenu.h>
#include <QMenu>

extern QMap<int, QAction*> g_actionList;

class mxMenu_i : public QMenu
{
	mxMenu *d_widget;

public:
	mxMenu_i (mxMenu *menu) : QMenu ()
	{
		d_widget = menu;
	}

	~mxMenu_i ()
	{
	}
};



mxMenu::mxMenu ()
: mxWidget (0, 0, 0, 0, 0)
{
	d_this = new mxMenu_i (this);

	setHandle ((void *) d_this);
	setType (MX_MENU);
}



mxMenu::~mxMenu ()
{
	delete d_this;
}



void
mxMenu::add (const char *item, int id)
{
	g_actionList[id] = d_this->addAction (item);
}



void
mxMenu::addMenu (const char *item, mxMenu *menu)
{
	QMenu *_menu = (QMenu*)menu->getHandle ();
	_menu->setTitle (item);
	d_this->addMenu (_menu);
}



void
mxMenu::addSeparator ()
{
	d_this->addSeparator ();
}



void
mxMenu::setEnabled (int id, bool b)
{
	g_actionList[id]->setEnabled (b);
}



void
mxMenu::setChecked (int id, bool b)
{
	g_actionList[id]->setCheckable (true);
	g_actionList[id]->setChecked (b);
}



bool
mxMenu::isEnabled (int id) const
{
	return g_actionList[id]->isEnabled ();
}



bool
mxMenu::isChecked (int id) const
{
	return g_actionList[id]->isChecked ();
}
