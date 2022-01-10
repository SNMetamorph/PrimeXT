//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mx.cpp
// implementation: Win32 API
// last modified:  Jun 01 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include <mx.h>
#include <mxWindow.h>
#include <mxEvent.h>
#include <mxLinkedList.h>
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utlarray.h>

#define CLASSNAME "MXTK::CLASSNAME"

extern HACCEL g_hAccel;



void mxTab_resizeChild (HWND hwnd);



mxWindow *g_mainWindow = 0;
//static mxLinkedList *g_widgetList = 0;
static mxWindow *g_idleWindow = 0;

static MSG msg;
static HWND g_hwndToolTipControl = 0;
static bool isClosing = false;
static HWND g_CurrentHWND = 0;
static HACCEL g_hAcceleratorTable = NULL;

void mx::createAccleratorTable( int numentries, Accel_t *entries )
{
	CUtlArray< ACCEL > accelentries;

	for ( int i = 0; i < numentries; ++i )
	{
		const Accel_t& entry = entries[ i ];

		ACCEL add;
		add.key = entry.key;
		add.cmd = entry.command;
		add.fVirt = 0;
		if ( entry.flags & ACCEL_ALT )
		{
			add.fVirt |= FALT;
		}
		if ( entry.flags & ACCEL_CONTROL )
		{
			add.fVirt |= FCONTROL;
		}
		if ( entry.flags & ACCEL_SHIFT )
		{
			add.fVirt |= FSHIFT;
		}
		if ( entry.flags & ACCEL_VIRTKEY )
		{
			add.fVirt |= FVIRTKEY;
		}

		accelentries.AddToTail( add );
	}

	g_hAcceleratorTable = ::CreateAcceleratorTable( accelentries.Base(), accelentries.Count() );
}

HWND
mx_CreateToolTipControl ()
{
	if (!g_hwndToolTipControl)
	{
		if (g_mainWindow)
		{
			g_hwndToolTipControl = CreateWindowEx (0, TOOLTIPS_CLASS, "", WS_POPUP | WS_EX_TOPMOST,
				0, 0, 0, 0, (HWND) g_mainWindow->getHandle (),
				(HMENU) NULL, (HINSTANCE) GetModuleHandle (NULL), NULL);
		}
	}

	return g_hwndToolTipControl;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *window - 
//			*event - 
// Output : static void
//-----------------------------------------------------------------------------
static void RecursiveHandleEvent( mxWindow *window, mxEvent *event )
{
	while ( window )
	{
		if ( window->handleEvent ( event ) )
			break;

		window = window->getParent();
	}
}

static LRESULT CALLBACK WndProc (HWND hwnd, UINT uMessage, WPARAM wParam, LPARAM lParam)
{
	static bool bDragging = FALSE;

	switch (uMessage)
	{
	case WM_COMMAND:
	{
		if (isClosing)
			break;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (LOWORD (wParam) > 0 && window)
		{
			WORD wNotifyCode = (WORD) HIWORD (wParam);
			HWND hwndCtrl = (HWND) lParam;
			mxEvent event;

			CHAR className[128];
			GetClassName (hwndCtrl, className, 128);
			if (!strcmpi (className, "edit"))
			{
				if (wNotifyCode != EN_CHANGE)
					break;
			}
			else if (!strcmpi (className, "combobox"))
			{
				if (wNotifyCode != CBN_SELCHANGE)
					break;
			}
			else if (!strcmpi (className, "listbox"))
			{
				if (wNotifyCode != LBN_SELCHANGE)
					break;
			}

			event.event = mxEvent::Action;
			event.widget = (mxWidget *) GetWindowLong (hwndCtrl, GWL_USERDATA);
			event.action = (int) LOWORD (wParam);
			RecursiveHandleEvent( window, &event );
		}
	}
	break;

	case WM_SYSCOMMAND:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		switch(wParam)
		{
		case SC_RESTORE:
			window->setMaximized( false );
			break;
		case SC_MAXIMIZE:
			window->setMaximized( true );
			break;
		case SC_CLOSE:
			break;
		}
	}
	break;

	case WM_NOTIFY:
	{
		if (isClosing)
			break;

		NMHDR *nmhdr = (NMHDR *) lParam;
		mxEvent event;

		if (nmhdr->code == TVN_SELCHANGED)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;

				RECT rc;
				HTREEITEM hItem = TreeView_GetSelection (nmhdr->hwndFrom);
				TreeView_GetItemRect (nmhdr->hwndFrom, hItem, &rc, TRUE);
				event.x = (int) rc.left;
				event.y = (int) rc.bottom;
				RecursiveHandleEvent( window, &event );
			}
		}
		else if (nmhdr->code == LVN_ITEMCHANGED)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;

				RecursiveHandleEvent( window, &event );
			}
		}
		else if (nmhdr->code == NM_RCLICK)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;
				event.flags = mxEvent::RightClicked;

				if (event.widget && (event.widget->getType () == MX_TREEVIEW))
				{
					RECT rc;
					HTREEITEM hItem = TreeView_GetSelection (nmhdr->hwndFrom);
					TreeView_GetItemRect (nmhdr->hwndFrom, hItem, &rc, TRUE);
					event.x = (int) rc.left;
					event.y = (int) rc.bottom;
				}

				RecursiveHandleEvent( window, &event );
			}
		}
		else if (nmhdr->code == NM_DBLCLK)
		{
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;
				event.flags = mxEvent::DoubleClicked;

				if (event.widget && event.widget->getType () == MX_TREEVIEW)
				{
					RECT rc;
					HTREEITEM hItem = TreeView_GetSelection (nmhdr->hwndFrom);
					TreeView_GetItemRect (nmhdr->hwndFrom, hItem, &rc, TRUE);
					event.x = (int) rc.left;
					event.y = (int) rc.bottom;
				}

				RecursiveHandleEvent( window, &event );

				return TRUE;
			}
		}
		else if (nmhdr->code == TCN_SELCHANGING)
		{
			TC_ITEM ti;

			int index = TabCtrl_GetCurSel (nmhdr->hwndFrom);
			if (index >= 0)
			{
				ti.mask = TCIF_PARAM;
				TabCtrl_GetItem (nmhdr->hwndFrom, index, &ti);
				mxWindow *window = (mxWindow *) ti.lParam;
				if (window)
					window->setVisible (false);
			}
		}
		else if (nmhdr->code == TCN_SELCHANGE)
		{
			mxTab_resizeChild (nmhdr->hwndFrom);
			if (nmhdr->idFrom > 0)
			{
				mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
				event.event = mxEvent::Action;
				event.widget = (mxWidget *) GetWindowLong (nmhdr->hwndFrom, GWL_USERDATA);
				event.action = (int) nmhdr->idFrom;
				RecursiveHandleEvent( window, &event );
			}
		}
	}
	break;

	case WM_SIZE:
	{
		if (isClosing)
			break;

		mxEvent event;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			event.event = mxEvent::Size;
			event.width = (int) LOWORD (lParam);
			event.height = (int) HIWORD (lParam);
			window->handleEvent (&event);
		}
	}
	break;

	case WM_ERASEBKGND:
	{
		if (isClosing)
			break;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			if (window->getType () == MX_GLWINDOW)
				return 0;
		}
	}
	break;

	case WM_HSCROLL:
	case WM_VSCROLL:
	{
		if (isClosing)
			break;

		switch (LOWORD (wParam))
		{
		case TB_ENDTRACK:
		case TB_THUMBTRACK:
		{
			mxWidget *widget = (mxWidget *) GetWindowLong ((HWND) lParam, GWL_USERDATA);
			mxEvent event;

			if (!widget)
				break;

			event.event = mxEvent::Action;
			event.widget = widget;
			event.action = widget->getId ();

			mxWindow *window = widget->getParent ();
			while (window && event.action > 0)
			{
				if (window->handleEvent (&event))
					break;

				window = window->getParent ();
			}
		}
		break;
		}
	}
	break;

	case WM_PAINT:
	{
		if (isClosing)
			break;

		PAINTSTRUCT ps;
		HDC hDC = BeginPaint (hwnd, &ps);

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			window->redraw ();
		}

		EndPaint (hwnd, &ps);
	}
	break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
	{
		bDragging = TRUE;
		SetCapture (hwnd);
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);

		if (window)
		{
			mxEvent event;
			event.event = mxEvent::MouseDown;
			event.x = (int) (signed short) LOWORD (lParam);
			event.y = (int) (signed short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (uMessage == WM_MBUTTONDOWN)
				event.buttons |= mxEvent::MouseMiddleButton;
			else if (uMessage == WM_RBUTTONDOWN)
				event.buttons |= mxEvent::MouseRightButton;
			else
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			window->handleEvent (&event);
		}
	}
	break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::MouseUp;
			event.x = (int) (signed short) LOWORD (lParam);
			event.y = (int) (signed short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (uMessage == WM_MBUTTONUP)
				event.buttons |= mxEvent::MouseMiddleButton;
			else if (uMessage == WM_RBUTTONUP)
				event.buttons |= mxEvent::MouseRightButton;
			else
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			window->handleEvent (&event);
		}
		bDragging = FALSE;
		ReleaseCapture ();
	}
	break;

	case WM_MOUSEMOVE:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;

			if (bDragging)
				event.event = mxEvent::MouseDrag;
			else
				event.event = mxEvent::MouseMove;

			event.x = (int) (signed short) LOWORD (lParam);
			event.y = (int) (signed short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			window->handleEvent (&event);
		}
	}
	break;

	case 0x020A://WM_MOUSEWHEEL:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;

			event.event = mxEvent::MouseWheel;

			POINT pt;

			GetCursorPos (&pt);
			event.x = (int) (signed short) LOWORD (lParam);
			event.y = (int) (signed short) HIWORD (lParam);
			event.buttons = 0;
			event.modifiers = 0;

			wParam = LOWORD(wParam);
			if (wParam & MK_LBUTTON)
				event.buttons |= mxEvent::MouseLeftButton;

			if (wParam & MK_RBUTTON)
				event.buttons |= mxEvent::MouseRightButton;

			if (wParam & MK_MBUTTON)
				event.buttons |= mxEvent::MouseMiddleButton;

			if (wParam & MK_CONTROL)
				event.modifiers |= mxEvent::KeyCtrl;

			if (wParam & MK_SHIFT)
				event.modifiers |= mxEvent::KeyShift;

			window->handleEvent (&event);
			return TRUE;
		}
	}
	break;

	case WM_KEYDOWN:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::KeyDown;
			event.key = (int) wParam;
			window->handleEvent (&event);
		}
	}
	break;

	case WM_CHAR:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::KeyDown;
			event.key = (int) wParam;
			window->handleEvent (&event);
		}
	}
	break;

	case WM_KEYUP:
	{
		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::KeyUp;
			event.key = (int) wParam;
			window->handleEvent (&event);
		}
	}
	break;

	case WM_TIMER:
	{
		if (isClosing)
			break;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			mxEvent event;
			event.event = mxEvent::Timer;
			window->handleEvent (&event);
		}
	}
	break;
#if 1
	case WM_ACTIVATE:
	{
		if (isClosing)
			break;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			HWND hWnd = (HWND) lParam;
			if (::IsWindow(hWnd) && (LOWORD (wParam) == WA_ACTIVE || LOWORD (wParam) == WA_CLICKACTIVE))
			{
				mxWindow *window2 = (mxWindow *) GetWindowLong (hWnd, GWL_USERDATA);
				if (window2)
				{
					if (hWnd == g_CurrentHWND && window2->getType () == MX_DIALOGWINDOW)
					{
						//SetWindowPos ((HWND) lParam, HWND_TOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
						SetForegroundWindow (hWnd);
						return 0;
					}
				}
			}
		}
	}
	break;
#endif
/*	case WM_SHOWWINDOW:
	{
		if (isClosing)
			break;

		mxWindow *window = (mxWindow *) GetWindowLong (hwnd, GWL_USERDATA);
		if (window)
		{
			printf ("%s %d %d\n", window->getLabel (), wParam, window->getType ());
			if ((BOOL) wParam && window->getType () == MX_DIALOGWINDOW)
				g_CurrentHWND = hwnd;
			else
				g_CurrentHWND = 0;
			printf ("hwnd: %d\n", g_CurrentHWND);
		}
	}
	break;
*/
	case WM_CLOSE:
		if (g_mainWindow)
		{
			if ((void *) hwnd == g_mainWindow->getHandle ())
			{
				mx::quit ();
			}
			else
				ShowWindow (hwnd, SW_HIDE);
		}
		//else // shouldn't happen
			//DestroyWindow (hwnd);
		return 0;
/*
	case WM_DESTROY:
		if (g_mainWindow)
		{
			if ((void *) hwnd == g_mainWindow->getHandle ())
				mx::quit ();
		}
		break;
*/
	}

	return DefWindowProc (hwnd, uMessage, wParam, lParam);
}



int
mx::init(int argc, char **argv)
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = (HINSTANCE) GetModuleHandle (NULL);
	wc.hIcon = LoadIcon (wc.hInstance, "MX_ICON");
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_3DSHADOW;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = CLASSNAME;

	if (!wc.hIcon)
		wc.hIcon = LoadIcon (NULL, IDI_WINLOGO);

	if (!RegisterClass (&wc))
		return 0;

	InitCommonControls ();

	isClosing = false;

	return 1;
}



int
mx::run()
{
	int messagecount = 0;

	while (!isClosing)
	{
		bool doframe = false;
		if ( PeekMessage (&msg, NULL, 0, 0, PM_NOREMOVE) || !g_idleWindow )
		{
			if (!GetMessage (&msg, NULL, 0, 0))
			{
				doframe = false;
				break;
			}

			if ( !g_hAcceleratorTable || !TranslateAccelerator( (HWND)g_mainWindow->getHandle (), g_hAcceleratorTable, &msg )) 
			{
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
			messagecount++;

			if ( messagecount > 10 )
			{
				messagecount = 0;
				doframe = true;
			}
		}
		else if (g_idleWindow)
		{
			doframe = true;
			messagecount = 0;
		}

		if ( doframe && g_idleWindow )
		{
			mxEvent event;
			event.event = mxEvent::Idle;
			g_idleWindow->handleEvent (&event);
		}
	}

	return msg.wParam;
}



int
mx::check ()
{
	if (PeekMessage (&msg, 0, 0, 0, PM_NOREMOVE))
	{
		if (GetMessage (&msg, 0, 0, 0))
		{
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		}

		return 1;
	}

	return 0;
}



void
mx::quit ()
{
	isClosing = true;
}



void
mx::cleanup ()
{
	delete g_mainWindow;

	if (g_hwndToolTipControl)
		DestroyWindow (g_hwndToolTipControl);

	if ( g_hAcceleratorTable )
	{
		DestroyAcceleratorTable( g_hAcceleratorTable );
		g_hAcceleratorTable = 0;
	}

	PostQuitMessage (0);
	UnregisterClass (CLASSNAME, (HINSTANCE) GetModuleHandle (NULL));
}



void
mx::setEventWindow (mxWindow *window)
{
	if (window)
		g_CurrentHWND = (HWND) window->getHandle ();
	else
		g_CurrentHWND = 0;
}



int
mx::setDisplayMode (int w, int h, int bpp)
{
	DEVMODE dm;

	dm.dmSize = sizeof (DEVMODE);
	dm.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	dm.dmBitsPerPel = bpp;
	dm.dmPelsWidth = w;
	dm.dmPelsHeight = h;

	if (w == 0 || h == 0 || bpp == 0)
		ChangeDisplaySettings (0, 0);
	else
		ChangeDisplaySettings (&dm, CDS_FULLSCREEN);

	return 0;
}



void
mx::setIdleWindow (mxWindow *window)
{
	g_idleWindow = window;
}



int
mx::getDisplayWidth ()
{
	return (int) GetSystemMetrics (SM_CXSCREEN);
}



int
mx::getDisplayHeight ()
{
	return (int) GetSystemMetrics (SM_CYSCREEN);
}



mxWindow*
mx::getMainWindow ()
{
	return g_mainWindow;
}



const char *
mx::getApplicationPath ()
{
	static char path[256];
	GetModuleFileName (0, path, 256);
	char *ptr = strrchr (path, '\\');
	if (ptr)
		*ptr = '\0';

	return path;
}



unsigned int mx :: getTickCount()
{
	static DWORD	timebase;

	if( !timebase )
	{
		timeBeginPeriod( 1 );
		timebase = timeGetTime();
	}

	return timeGetTime() - timebase;
}

bool mx :: regCreateKey( HKEY hKey, const char *SubKey )
{
	HKEY newKey = 0;	// Registry function result code
	ULONG dwDisposition;// Type of key opening event
	long lRet;


	if( lRet = RegCreateKeyEx( hKey, SubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &newKey, &dwDisposition ) != ERROR_SUCCESS )
	{
  		return false;
	}
	else
	{
		RegCloseKey( newKey );
	}

	return true;
}

bool mx :: regGetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer )
{
	DWORD dwBufLen = 256;
	long lRet;

	if( lRet = RegOpenKeyEx( hKey, SubKey, 0, KEY_READ, &hKey ) != ERROR_SUCCESS )
	{
  		return false;
	}
	else
	{
		lRet = RegQueryValueEx( hKey, Value, NULL, NULL, (byte *)pBuffer, &dwBufLen );

		if( lRet != ERROR_SUCCESS )
			return false;

		RegCloseKey( hKey );
	}
	return true;
}

bool mx :: regSetValue( HKEY hKey, const char *SubKey, const char *Value, char *pBuffer )
{
	DWORD dwBufLen = 256;
	long lRet;
	
	if( lRet = RegOpenKeyEx( hKey, SubKey, 0, KEY_WRITE, &hKey ) != ERROR_SUCCESS )
	{
		return false;
	}
	else
	{
		lRet = RegSetValueEx( hKey, Value, 0, REG_SZ, (byte *)pBuffer, dwBufLen );

		if( lRet != ERROR_SUCCESS )
			return false;

		RegCloseKey( hKey );
	}

	return true;	
}