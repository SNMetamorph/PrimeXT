//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxWindow_i.h
// implementation: Qt Free Edition
// last modified:  Apr 20 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx/mxWindow.h>
#include <mx/mx.h>
#include <QResizeEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QApplication>



class mxWindow_i : public QMainWindow
{
	Q_OBJECT
	mxWindow *d_window;
	bool d_dragging;
	int d_button;
	int d_timerId;
public:
	mxWindow_i (QWidget *parent, mxWindow *window) : QMainWindow ((QMainWindow *)parent)
	{
		d_window = window;
		d_dragging = false;
		d_button = 0;
		d_timerId = 0;
		//setMouseGrabEnabled(true);
		setMouseTracking (true);
	}

	~mxWindow_i ()
	{
	}

	void setTimerId(int timerId)
	{
		d_timerId = timerId;
	}

	int getTimerId()
	{
		return d_timerId;
	}
public slots:
	void idleEvent ()
	{
		mxEvent event;
		event.event = mxEvent::Idle;
		d_window->handleEvent (&event);
	}
	
protected:
	virtual void resizeEvent (QResizeEvent *e)
	{
		QMainWindow::resizeEvent (e);
		mxEvent event;
		event.event = mxEvent::Size;
		event.width = e->size ().width ();
		event.height = e->size ().height ();
		d_window->handleEvent (&event);
	}

	virtual void mousePressEvent (QMouseEvent *e)
	{
		QMainWindow::mousePressEvent (e);
		d_dragging = true;

		d_button = 0;
		if (e->buttons () & Qt::LeftButton)
			d_button |= mxEvent::MouseLeftButton;
		if (e->buttons () & Qt::RightButton)
			d_button |= mxEvent::MouseRightButton;
		if (e->buttons () & Qt::MidButton)
			d_button |= mxEvent::MouseMiddleButton;
		
		mxEvent event;
		if (e->buttons () & Qt::ControlModifier)
			event.modifiers |= mxEvent::KeyCtrl;
		if (e->buttons () & Qt::ShiftModifier)
			event.modifiers |= mxEvent::KeyShift;
		event.event = mxEvent::MouseDown;
		event.buttons = d_button;
		event.x = e->x ();
		event.y = e->y ();
		d_window->handleEvent (&event);
	}

	virtual void mouseMoveEvent (QMouseEvent *e)
	{
		QMainWindow::mouseMoveEvent (e);

		d_button = 0;
		if (e->buttons () & Qt::LeftButton)
			d_button |= mxEvent::MouseLeftButton;
		if (e->buttons () & Qt::RightButton)
			d_button |= mxEvent::MouseRightButton;
		if (e->buttons () & Qt::MidButton)
			d_button |= mxEvent::MouseMiddleButton;
		
		mxEvent event;
		if (e->buttons () & Qt::ControlModifier)
			event.modifiers |= mxEvent::KeyCtrl;
		if (e->buttons () & Qt::ShiftModifier)
			event.modifiers |= mxEvent::KeyShift;

		event.buttons = d_button;
		event.x = e->x ();
		event.y = e->y ();
		
		if (d_dragging)
			event.event = mxEvent::MouseDrag;
		else
			event.event = mxEvent::MouseMove;
			
		d_window->handleEvent (&event);
	}

	virtual void mouseReleaseEvent (QMouseEvent *e)
	{
		QMainWindow::mouseReleaseEvent (e);
		d_dragging = false;
		d_button = 0;
		
		if (e->buttons () & Qt::LeftButton)
			d_button |= mxEvent::MouseLeftButton;
		if (e->buttons () & Qt::RightButton)
			d_button |= mxEvent::MouseRightButton;
		if (e->buttons () & Qt::MidButton)
			d_button |= mxEvent::MouseMiddleButton;
		
		mxEvent event;
		if (e->buttons () & Qt::ControlModifier)
			event.modifiers |= mxEvent::KeyCtrl;
		if (e->buttons () & Qt::ShiftModifier)
			event.modifiers |= mxEvent::KeyShift;

		event.event = mxEvent::MouseUp;
		event.buttons = d_button;
		event.x = e->x ();
		event.y = e->y ();
		
		d_window->handleEvent (&event);
		d_button = 0;
	}
	
	virtual void timerEvent (QTimerEvent *e)
	{
		mxEvent event;
		event.event = mxEvent::Timer;
		d_window->handleEvent (&event);
	}
};
