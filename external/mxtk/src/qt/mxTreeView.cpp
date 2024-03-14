//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxTreeView.cpp
// implementation: Qt Free Edition
// last modified:  Apr 20 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxTreeView_p.h"



mxTreeView::mxTreeView (mxWindow *parent, int x, int y, int w, int h, int id)
: mxWidget (parent, x, y, w, h)
{
	QWidget *p = 0;
	if (parent)
		p = (QWidget *) parent->getHandle ();
	d_this = new mxTreeView_i (p, this);
	//d_this->addColumn ("");
	d_this->setRootIsDecorated (true);
	d_this->connect (d_this, SIGNAL (itemSelectionChanged ()), d_this, SLOT (selectionChangedEvent ()));	
	d_this->connect (d_this, SIGNAL (itemDoubleClicked(QTreeWidgetItem *, int)), d_this, SLOT (doubleClickedEvent (QTreeWidgetItem *, int)));	
	//d_this->connect (d_this, SIGNAL (itemRightButtonClicked (QTreeWidgetItem *, const QPoint&, int)), d_this, SLOT (rightButtonClickedEvent (QTreeWidgetItem *, const QPoint&, int)));	

	setHandle ((void *) d_this);
	setType (MX_TREEVIEW);
	setParent (parent);
	setId (id);
	setBounds (x, y, w, h);
	setVisible (true);
}



mxTreeView::~mxTreeView ()
{
	remove (0);
	delete d_this;
}



mxTreeViewItem*
mxTreeView::add (mxTreeViewItem *parent, const char *item)
{
	QTreeWidgetItem *lvi;

	if (parent)
		lvi = new QTreeWidgetItem ((QTreeWidgetItem *) parent, QStringList(item));
	else
		lvi = new QTreeWidgetItem ((QTreeWidget *) d_this, QStringList(item));

	return (mxTreeViewItem *) lvi;
}



void
mxTreeView::remove (mxTreeViewItem *item)
{
	if (!item)
		d_this->clear ();
	else
		delete (QTreeWidgetItem *) item;
}



mxTreeViewItem *
mxTreeView::getFirstChild (mxTreeViewItem *item) const
{
	if (item)
		return (mxTreeViewItem *) ((QTreeWidgetItem *) item)->child (0);
	else
		return (mxTreeViewItem *) d_this->topLevelItem (0);
}



mxTreeViewItem *
mxTreeView::getNextChild (mxTreeViewItem *item) const
{
	if (item)
	{
		QTreeWidgetItem *current = (QTreeWidgetItem *) item;
		QTreeWidgetItem *parent = current->parent ();
		QTreeWidgetItem *nextSibling;
		if (parent)
		{
			nextSibling = parent->child(parent->indexOfChild(current) + 1);
		}
		else
		{
			QTreeWidget *treeWidget = current->treeWidget();
			nextSibling = treeWidget->topLevelItem(treeWidget->indexOfTopLevelItem(current) + 1);
		}
		return (mxTreeViewItem *) nextSibling;
	}

	return 0;
}



mxTreeViewItem*
mxTreeView::getSelectedItem () const
{
	return (mxTreeViewItem *) d_this->currentItem ();
}



void
mxTreeView::setLabel (mxTreeViewItem *item, const char *label)
{
	if (item)
		((QTreeWidgetItem *) item)->setText (0, label);
}



void
mxTreeView::setUserData (mxTreeViewItem *item, void *userData)
{
}



void
mxTreeView::setOpen (mxTreeViewItem *item, bool b)
{
	if (item)
		((QTreeWidgetItem *) item)->setExpanded (b);
}



void
mxTreeView::setSelected (mxTreeViewItem *item, bool b)
{
	if (item)
		((QTreeWidgetItem *) item)->setSelected (b);
}



const char *
mxTreeView::getLabel (mxTreeViewItem *item) const
{
	if (item)
		return qPrintable(((QTreeWidgetItem *) item)->text (0));
		
	return 0;
}



void *
mxTreeView::getUserData (mxTreeViewItem *item) const
{
	return 0;
}



bool
mxTreeView::isOpen (mxTreeViewItem *item) const
{
	if (item)
		return ((QTreeWidgetItem *) item)->isExpanded ();

	return false;
}



bool
mxTreeView::isSelected (mxTreeViewItem *item) const
{
	if (item)
		return ((QTreeWidgetItem *) item)->isSelected ();
		
	return 0;
}



mxTreeViewItem *
mxTreeView::getParent (mxTreeViewItem *item) const
{
	if (item)
		return (mxTreeViewItem *) ((QTreeWidgetItem *) item)->parent ();

	return 0;
}
