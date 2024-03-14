//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxMenuBar.cpp
// implementation: Qt Free Edition
// last modified:  May 04 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxMenuBar_p.h"

QMap<int, QAction*> g_actionList;

mxMenuBar::mxMenuBar (mxWindow *parent)
: mxWidget (parent, 0, 0, 0, 0)
{
	QWidget *p = nullptr;
	if (parent)
		p = (QWidget *) ((mxWidget *) parent)->getHandle ();

	d_this = new mxMenuBar_i (p, this);
	d_this->connect (d_this, SIGNAL (triggered (QAction*)), d_this, SLOT (activatedEvent (QAction*)));

	setHandle ((void *) d_this);
	setType (MX_MENUBAR);
	setParent (parent);
	setVisible (true);
}



mxMenuBar::~mxMenuBar ()
{
	g_actionList.clear ();
	delete d_this;
}



void
mxMenuBar::addMenu (const char *item, mxMenu *menu)
{
	QMenu *_menu = (QMenu *) ((mxWidget *) menu)->getHandle ();
	_menu->setTitle (item);
	d_this->addMenu (_menu);
}



void
mxMenuBar::setEnabled (int id, bool b)
{
	g_actionList[id]->setEnabled (b);
}



void
mxMenuBar::setChecked (int id, bool b)
{
	g_actionList[id]->setCheckable (true);
	g_actionList[id]->setChecked (b);
}



void
mxMenuBar::modify (int id, int newId, const char *newItem)
{
	// probably, id will be always the same
	QAction *action = g_actionList.value (id);
	action->setText (newItem);
	// g_actionList[newId] = action;
}



bool
mxMenuBar::isEnabled (int id) const
{
	return g_actionList[id]->isEnabled ();
}



bool
mxMenuBar::isChecked (int id) const
{
	return g_actionList[id]->isChecked ();
}



int
mxMenuBar::getHeight () const
{
	mxWindow *window = getParent ();

	if (window)
		return d_this->heightForWidth (window->w ());

	return 0;
}
