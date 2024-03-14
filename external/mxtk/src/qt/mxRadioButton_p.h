//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxRadioButton_i.h
// implementation: Qt Free Edition
// last modified:  Mar 19 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxRadioButton.h>
#include <mx/mxWindow.h>
#include <QRadioButton>
#include <QButtonGroup>



class mxRadioButton_i : public QRadioButton
{
	Q_OBJECT
	mxRadioButton *d_widget;
	QButtonGroup *d_buttonGroup;
	static QButtonGroup *s_buttonGroup;

public:
	mxRadioButton_i (QWidget *parent, mxRadioButton *widget, bool newGroup) : QRadioButton (parent)
	{
		d_widget = widget;
		d_buttonGroup = 0;

		if (newGroup)
		{
			d_buttonGroup = new QButtonGroup ();
			d_buttonGroup->setExclusive (true);
			s_buttonGroup = d_buttonGroup;
		}

		if (s_buttonGroup)
			s_buttonGroup->addButton (this);
	}

	~mxRadioButton_i ()
	{
		if (d_buttonGroup)
		{
			d_buttonGroup->removeButton (this);
		}
	}

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
