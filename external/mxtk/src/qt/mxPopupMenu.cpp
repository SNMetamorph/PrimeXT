//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxPopupMenu.cpp
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxPopupMenu_p.h"
#include <QApplication>



mxPopupMenu::mxPopupMenu ()
: mxWidget (0, 0, 0, 0, 0)
{
	d_this = new mxPopupMenu_i (this);
	d_this->menuAction()->setCheckable (true);
	d_this->connect (d_this, SIGNAL (triggered (QAction*)), d_this, SLOT (activatedEvent (QAction*)));

	setHandle ((void *) d_this);
	setType (MX_POPUPMENU);
}



mxPopupMenu::~mxPopupMenu ()
{
	delete d_this;
}



int
mxPopupMenu::popup (mxWidget *widget, int x, int y)
{
	QPoint p (x, y);
	QWidget *w = (QWidget *) widget->getHandle ();
	p = w->mapToGlobal (p);
	d_this->popup (p);

	while (d_this->d_lastItemId == -1)
		qApp->processEvents ();

	int id = d_this->d_lastItemId;
	d_this->d_lastItemId = -1;

	return id;
}



void
mxPopupMenu::add (const char *item, int id)
{
	d_this->d_actionList[id] = d_this->addAction (item);
}


/*
void
mxPopupMenu::addMenu (const char *item, mxPopupMenu *menu)
{
	QMenu *_menu = (QMenu *) menu->getHandle ();
	_menu->setTitle(item);
	d_this->addMenu (_menu);
}
*/


void
mxPopupMenu::addSeparator ()
{
	d_this->addSeparator ();
}



void
mxPopupMenu::setEnabled (int id, bool b)
{
	d_this->d_actionList[id]->setEnabled (b);
}



void
mxPopupMenu::setChecked (int id, bool b)
{
	d_this->d_actionList[id]->setChecked (b);
}



bool
mxPopupMenu::isEnabled (int id) const
{
	return d_this->d_actionList[id]->isEnabled ();
}



bool
mxPopupMenu::isChecked (int id) const
{
	return d_this->d_actionList[id]->isChecked ();
}
