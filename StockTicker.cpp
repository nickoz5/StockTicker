// StockTicker.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "StockTicker.h"
#include "StockPrices.h"

#include <WinSock2.h>
#include <vector>
#include <commctrl.h>
#include <Windowsx.h>
#include <shellapi.h>
#include <time.h>
#include <strsafe.h>


#define MAX_LOADSTRING			100
#define ID_TIMER_SCROLL			1
#define ID_TIMER_UPDATE_QUOTES	2


// Configuration variables:
BOOL m_bAlwaysOnTop = TRUE;				// window is always on top
int m_nTimerDrawSpeed = 20;				// redraw time in ms
int m_nDrawStep = 1;					// 
int m_nTimerUpdateQuotes = 60000 * 5;	// update time in ms
RECT m_wndRect;							// window rect

std::vector<CStockPrices*> m_vecStockPrices;


// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
TCHAR* szPopupWindowClass = _T("PopupInfoWnd");

HWND m_hWndPopup = NULL;

HWND hwndCaptureOld = NULL;
HANDLE hUpdateThread = NULL;

BOOL bIsMoving = FALSE;
BOOL bIsSizing = FALSE;

RECT rcDragStart;
POINT ptDragStart;	// start point for dragging window

HFONT hFontSymbol = NULL;
HFONT hFontPrice = NULL;

HMENU m_hMenuPopup = NULL;
HMENU m_hMenu = NULL;

HBITMAP m_hGripper = NULL;
HBITMAP m_hSizer = NULL;

CStockPrices* m_pHitItem = NULL;


// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
ATOM				MyRegisterPopupClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK	WndProcPopupInfo(HWND, UINT, WPARAM, LPARAM);

INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	Settings(HWND, UINT, WPARAM, LPARAM);
void				OnPaint(HWND,HDC);

void				ReadSettings(HWND hWnd);
void				WriteSettings();

void				UpdateWindowState(HWND hWnd);
CStockPrices*		HitTest(HWND hWnd, POINT pt);
DWORD WINAPI		UpdateQuotesFunc(LPVOID lpParam);
void CALLBACK		TimerFunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime);
void				ShowContextMenu(HWND hWnd, POINT pt);

void				HideMoreInfo();
void				ShowMoreInfo(HWND hWndParent);

void				ErrorExit(LPTSTR lpszFunction);

int APIENTRY _tWinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;
	
	WSADATA wsaData;
	int wsaret = WSAStartup(0x101, &wsaData);
	if (wsaret)
		return -1;

	// Initialize global strings
	LoadString(hInstance, IDC_STOCKTICKER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);
	MyRegisterPopupClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		ErrorExit("InitInstance");
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_STOCKTICKER));
	
	m_hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDC_STOCKTICKER));
	if (m_hMenu == NULL)
		return FALSE;
	m_hMenuPopup = GetSubMenu(m_hMenu, 0);
	if (m_hMenuPopup == NULL) {
		return FALSE;
	}

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
		
	// finished.
	WSACleanup();

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    This function and its usage are only necessary if you want this code
//    to be compatible with Win32 systems prior to the 'RegisterClassEx'
//    function that was added to Windows 95. It is important to call this function
//    so that the application will get 'well formed' small icons associated
//    with it.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_STOCKTICKER));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;//MAKEINTRESOURCE(IDC_STOCKTICKER);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

ATOM MyRegisterPopupClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProcPopupInfo;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= 0;
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= 0;//MAKEINTRESOURCE(IDC_STOCKTICKER);
	wcex.lpszClassName	= szPopupWindowClass;
	wcex.hIconSm		= 0;

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindowEx(WS_EX_NOACTIVATE, szWindowClass, NULL, WS_VISIBLE|WS_POPUP,
      m_wndRect.left, m_wndRect.top, m_wndRect.right - m_wndRect.left, m_wndRect.bottom - m_wndRect.top, NULL, NULL, hInstance, NULL);
   if (!hWnd)
   {
      return FALSE;
   }
   
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   m_hWndPopup = CreateWindowEx(WS_EX_NOACTIVATE, szPopupWindowClass, NULL, WS_VISIBLE|WS_POPUP,
      0, 0, 0, 0, NULL, NULL, hInstance, NULL);

   if (!m_hWndPopup)
   {
	   return FALSE;
   }
   ShowWindow(m_hWndPopup, SW_HIDE);
   UpdateWindow(m_hWndPopup);

   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_ADDQUOTE:
		{
			INT_PTR iResult = DialogBox(hInst, MAKEINTRESOURCE(IDD_SETTINGS), hWnd, Settings);

			if (iResult == IDOK) {
				KillTimer(hWnd, ID_TIMER_SCROLL);
				SetTimer(hWnd, ID_TIMER_SCROLL, m_nTimerDrawSpeed, &TimerFunc);
			}

			break;
		}
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case IDM_ALWAYSONTOP:
			m_bAlwaysOnTop = !m_bAlwaysOnTop;
			UpdateWindowState(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_UPDATEUISTATE:

		break;

	case WM_CREATE:
	{
		LPCREATESTRUCT lpCreate = (LPCREATESTRUCT)lParam;

		// Show the notification icon
		NOTIFYICONDATA nid;
		ZeroMemory(&nid,sizeof(nid));
		nid.cbSize				=	sizeof(NOTIFYICONDATA);
		nid.hWnd				=	hWnd;
		nid.uID					=	0;
		nid.uFlags				=	NIF_ICON | NIF_MESSAGE | NIF_TIP;
		nid.uCallbackMessage	=	WM_USER;
		nid.hIcon				=	LoadIcon(hInst, MAKEINTRESOURCE(IDI_STOCKTICKER));
		lstrcpy(nid.szTip, "Stock ticker");

		Shell_NotifyIcon(NIM_ADD,&nid);

		hdc = BeginPaint(hWnd, &ps);

		// init our fonts..
		int nHeight = -MulDiv(13, GetDeviceCaps(hdc, LOGPIXELSY), 72);
		hFontSymbol = ::CreateFontA(nHeight, 0, 0, 0, FW_DONTCARE,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Tahoma"));
		
		nHeight = -MulDiv(7, GetDeviceCaps(hdc, LOGPIXELSY), 72);
		hFontPrice = ::CreateFontA(nHeight, 0, 0, 0, FW_DONTCARE,
			FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
			CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Tahoma"));

		m_hGripper = ::LoadBitmapA(hInst, MAKEINTRESOURCE(IDB_GRIPPER));
		m_hSizer = ::LoadBitmapA(hInst, MAKEINTRESOURCE(IDB_SIZER));
		
		ReadSettings(hWnd);

		UpdateQuotesFunc(hWnd);
		UpdateWindowState(hWnd);
		
		RECT rcClient;
		GetClientRect(hWnd, &rcClient);
		
		int nDrawStartPos = 0;
		std::vector<CStockPrices*>::const_iterator iter;
		for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
			CStockPrices* pUnit = (*iter);
			pUnit->SetDrawPos(nDrawStartPos);
			nDrawStartPos += pUnit->GetWidth();
		}

		TimerFunc(hWnd, WM_TIMER, ID_TIMER_SCROLL, 0);
		SetTimer(hWnd, ID_TIMER_SCROLL, m_nTimerDrawSpeed, &TimerFunc);
		SetTimer(hWnd, ID_TIMER_UPDATE_QUOTES, m_nTimerUpdateQuotes, &TimerFunc);
		
		EndPaint(hWnd, &ps);

		break;
	}

	case WM_DESTROY:
	{
		KillTimer(hWnd, ID_TIMER_SCROLL);
		KillTimer(hWnd, ID_TIMER_UPDATE_QUOTES);

		if (hUpdateThread) {
			// wait for thread..
			WaitForMultipleObjects(1, &hUpdateThread, TRUE, INFINITE);
			CloseHandle(hUpdateThread);
			hUpdateThread = NULL;
		}
		
		WriteSettings();

		DeleteObject(hFontPrice);
		DeleteObject(hFontSymbol);

		DeleteObject(m_hGripper);
		DeleteObject(m_hSizer);
		
		DestroyMenu(m_hMenuPopup);
		DestroyMenu(m_hMenu);
		
		std::vector<CStockPrices*>::const_iterator iter;
		for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
			CStockPrices* pUnit = (*iter);
			delete pUnit;
		}
		m_vecStockPrices.clear();
		
		// delete notification icon..
		NOTIFYICONDATA tnid;
		tnid.cbSize = sizeof(NOTIFYICONDATA);
		tnid.hWnd = hWnd;
		tnid.uID = 0;

		BOOL res = Shell_NotifyIcon(NIM_DELETE, &tnid);

		PostQuitMessage(0);
		break;
	}

	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		OnPaint(hWnd, hdc);
		EndPaint(hWnd, &ps);
		break;

	case WM_LBUTTONDOWN:
	{
		GetWindowRect(hWnd, &rcDragStart);

		RECT rcClient;
		GetClientRect(hWnd, &rcClient);

		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		
		RECT rc;
		
		bIsMoving = FALSE;
		bIsSizing = FALSE;

		// check move window..
		rc.left = 0;
		rc.right = 10;
		rc.top = 0;
		rc.bottom = rcClient.bottom;
		if (PtInRect(&rc, pt)) {
			bIsMoving = TRUE;
			ptDragStart = pt;
			ClientToScreen(hWnd, &ptDragStart);
		}
		
		// check size window..
		rc.left = rcClient.right - 10;
		rc.right = rcClient.right;
		rc.top = 0;
		rc.bottom = rcClient.bottom;
		if (PtInRect(&rc, pt)) {
			bIsSizing = TRUE;
			ptDragStart = pt;
			ClientToScreen(hWnd, &ptDragStart);
		}

		hwndCaptureOld = SetCapture(hWnd);
		break;
	}

	case WM_RBUTTONDOWN:
	{
		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);

		ClientToScreen(hWnd, &pt);
		ShowContextMenu(hWnd, pt);

		break;
	}

	case WM_MOUSEMOVE:
	{
		HMONITOR hMonitor;
		MONITORINFO mi;

		KillTimer(hWnd, ID_TIMER_SCROLL);
		
		POINT pt;
		pt.x = GET_X_LPARAM(lParam);
		pt.y = GET_Y_LPARAM(lParam);
		ClientToScreen(hWnd, &pt);

		RECT rcTemp;
		rcTemp.left = rcTemp.right = pt.x;
		rcTemp.top = rcTemp.bottom = pt.y;
			
		hMonitor = MonitorFromRect(&rcTemp, MONITOR_DEFAULTTONEAREST);
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(hMonitor, &mi);

		if (bIsMoving && wParam & MK_LBUTTON) {

			pt.x = pt.x - ptDragStart.x;
			pt.y = pt.y - ptDragStart.y;

			m_wndRect.left = rcDragStart.left + pt.x;
			m_wndRect.top = rcDragStart.top + pt.y;
			m_wndRect.right = m_wndRect.left + (rcDragStart.right - rcDragStart.left);
			m_wndRect.bottom = m_wndRect.top + (rcDragStart.bottom - rcDragStart.top);

			// snap client window to edges..
			if (m_wndRect.left <= mi.rcWork.left + 10 && m_wndRect.left >= mi.rcWork.left - 10)
				OffsetRect(&m_wndRect, mi.rcWork.left - m_wndRect.left, 0);
			if (m_wndRect.right <= mi.rcWork.right + 10 && m_wndRect.right >= mi.rcWork.right - 10)
				OffsetRect(&m_wndRect, mi.rcWork.right - m_wndRect.right, 0);
			if (m_wndRect.bottom <= mi.rcWork.bottom + 10 && m_wndRect.bottom >= mi.rcWork.bottom - 10)
				OffsetRect(&m_wndRect, 0, mi.rcWork.bottom - m_wndRect.bottom);
			if (m_wndRect.top <= mi.rcWork.top + 10 && m_wndRect.top >= mi.rcWork.top - 10)
				OffsetRect(&m_wndRect, 0, mi.rcWork.top - m_wndRect.top);

			MoveWindow(hWnd, m_wndRect.left, m_wndRect.top, m_wndRect.right - m_wndRect.left, m_wndRect.bottom - m_wndRect.top, TRUE);
		}
		if (bIsSizing && wParam & MK_LBUTTON) {

			pt.x = pt.x - ptDragStart.x;
			pt.y = pt.y - ptDragStart.y;

			int width = rcDragStart.right - rcDragStart.left;
			int height = rcDragStart.bottom - rcDragStart.top;
			
			m_wndRect.left = rcDragStart.left;
			m_wndRect.top = rcDragStart.top;
			m_wndRect.right = m_wndRect.left + width + pt.x;
			m_wndRect.bottom = m_wndRect.top + height;
			
			MoveWindow(hWnd, m_wndRect.left, m_wndRect.top, m_wndRect.right - m_wndRect.left, m_wndRect.bottom - m_wndRect.top, TRUE);
		}

		CStockPrices* pHitItem = HitTest(hWnd, pt);
		
		if (m_pHitItem != NULL && pHitItem != m_pHitItem) {
			RECT rcHitItem = m_pHitItem->GetRect();
			m_pHitItem->m_bHighlight = FALSE;
			InvalidateRect(hWnd, &rcHitItem, FALSE);
			if (pHitItem == NULL)
				HideMoreInfo();
			m_pHitItem = NULL;
		}

		if (m_pHitItem != pHitItem && pHitItem != NULL) {
			m_pHitItem = pHitItem;
			pHitItem->m_bHighlight = TRUE;
			RECT rcHitItem = pHitItem->GetRect();
			InvalidateRect(hWnd, &rcHitItem, FALSE);
			ShowMoreInfo(hWnd);
		}

		TRACKMOUSEEVENT tme;
		tme.cbSize = sizeof(TRACKMOUSEEVENT);
		tme.dwFlags = TME_LEAVE | TME_HOVER;
		tme.dwHoverTime = 100;
		tme.hwndTrack = hWnd;
		TrackMouseEvent(&tme);

		break;
	}

	case WM_LBUTTONUP:
		if (hwndCaptureOld != NULL)
			SetCapture(hwndCaptureOld);
		hwndCaptureOld = NULL;
		bIsMoving = FALSE;
		bIsSizing = FALSE;
		break;

	case WM_MOUSEHOVER:
		// show popup info for item under the mouse

		break;

	case WM_MOUSELEAVE:
		if (m_pHitItem != NULL) {
			RECT rcHitItem = m_pHitItem->GetRect();
			m_pHitItem->m_bHighlight = FALSE;
			InvalidateRect(hWnd, &rcHitItem, FALSE);
			HideMoreInfo();
			m_pHitItem = NULL;
		}

		SetTimer(hWnd, ID_TIMER_SCROLL, m_nTimerDrawSpeed, &TimerFunc);
		break;

	case WM_USER:
	{
		UINT uID; 
		UINT uMouseMsg; 
 
		uID = (UINT) wParam; 
		uMouseMsg = (UINT) lParam; 

		if (uMouseMsg == WM_RBUTTONDOWN) {
			POINT pt;
			GetCursorPos(&pt);
			ShowContextMenu(hWnd, pt);
		}
		if (uMouseMsg == WM_LBUTTONDBLCLK) {
			SetForegroundWindow(hWnd);
		}

		break;
	}

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

// Message handler for add symbol dialog.
INT_PTR CALLBACK Settings(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hWndList = GetDlgItem(hDlg, IDC_SYMBOL_LIST);
	HWND hWndDrawSpeed = GetDlgItem(hDlg, IDC_DRAW_SPEED);
	HWND hWndAlwaysOnTop = GetDlgItem(hDlg, IDC_ALWAYS_ON_TOP);

	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{
		//BOOL bResult = MakeDragList(hWndList);

		int iItem = 0;
		std::vector<CStockPrices*>::const_iterator iter;
		for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
			CStockPrices* prices = (*iter);
			SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)prices->m_szCode);
		}

		SendMessage(hWndAlwaysOnTop, BM_SETCHECK, (m_bAlwaysOnTop) ? BST_CHECKED : BST_UNCHECKED, 0);
		
		SendMessage(hWndDrawSpeed, TBM_SETRANGE, 0, MAKELPARAM(0, 100));
		SendMessage(hWndDrawSpeed, TBM_SETTICFREQ, 5, 0);
		SendMessage(hWndDrawSpeed, TBM_SETPOS, TRUE, m_nTimerDrawSpeed);
		

		return (INT_PTR)TRUE;
	}
	
	case WM_HSCROLL:
		switch(LOWORD(wParam))
		{
			case TB_THUMBPOSITION :
			{
				DWORD dwPos;
				dwPos = SendMessage(hWndDrawSpeed, TBM_GETPOS, 0, 0);
				m_nTimerDrawSpeed = dwPos;
				break;
			}
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_ADD_QUOTE) {
			TCHAR szSymbol[12];
			HWND hWnd = GetDlgItem(hDlg, IDC_SYMBOL);
			GetWindowText(hWnd, szSymbol, sizeof(szSymbol));
			CStockPrices* unit = new CStockPrices(szSymbol);
			if (!unit->UpdatePrice()) {
				delete unit;
				MessageBox(hDlg, _T("Not found."), "Error", MB_OK);
				return TRUE;
			}
			
			unit->m_hFontSymbol = hFontSymbol;
			unit->m_hFontPrice = hFontPrice;
			unit->m_hWnd = hWnd;
			unit->CalculateLayout();

			SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)unit->m_szCode);

			m_vecStockPrices.push_back(unit);

			return (INT_PTR)TRUE;
		}

		if (LOWORD(wParam) == IDOK)
		{
			// save settings..
			LRESULT lResult;
			
			lResult = SendMessage(hWndAlwaysOnTop, BM_GETCHECK, 0, 0);
			if (lResult == BST_CHECKED)
				m_bAlwaysOnTop = TRUE;
			else
				m_bAlwaysOnTop = FALSE;

			WriteSettings();

			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}

		if (LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}

	return (INT_PTR)FALSE;
}

void ReadSettings(HWND hWnd)
{
	DWORD dwValue;
	HKEY hKey;
	LONG returnStatus;
	DWORD dwType;
	DWORD dwSize;
	TCHAR szSymbolList[2048];

	// set defaults;
	m_wndRect.left = 0;
	m_wndRect.top = 0;
	m_wndRect.right = 500;
	m_wndRect.bottom = 20;

	m_bAlwaysOnTop = TRUE;
	m_nTimerDrawSpeed = 20;
	m_nTimerUpdateQuotes = 60000 * 5;

	strcpy_s(szSymbolList, _T("^AORD,^IXIC,EKM.AX,QBE.AX,PRU.AX,AVO.AX,CSR.AX,OZL.AX,ALL.AX,QRN.AX"));


	returnStatus = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\HSD\\StockTicker"), 0L, KEY_ALL_ACCESS, &hKey);
	if (returnStatus != ERROR_SUCCESS)
		returnStatus = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\HSD\\StockTicker", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);

	if (returnStatus == ERROR_SUCCESS)
	{
		//always on top;
		dwType = REG_DWORD;
		dwSize = sizeof(DWORD);
		returnStatus = RegQueryValueEx(hKey, "AlwaysOnTop", NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
		if (returnStatus == ERROR_SUCCESS)
			m_bAlwaysOnTop = (BOOL)dwValue;
		
		dwType = REG_DWORD;
		dwSize = sizeof(DWORD);
		returnStatus = RegQueryValueEx(hKey, "DrawSpeed", NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
		if (returnStatus == ERROR_SUCCESS)
			m_nTimerDrawSpeed = (int)dwValue;
		
		dwType = REG_DWORD;
		dwSize = sizeof(DWORD);
		returnStatus = RegQueryValueEx(hKey, "UpdateQuotesTime", NULL, &dwType, (LPBYTE)&dwValue, &dwSize);
		if (returnStatus == ERROR_SUCCESS)
			m_nTimerUpdateQuotes = (int)dwValue;
		
		RECT rc;
		dwType = REG_BINARY;
		dwSize = sizeof(RECT);
		returnStatus = RegQueryValueEx(hKey, "WindowPos", NULL, &dwType, (LPBYTE)&rc, &dwSize);
		if (returnStatus == ERROR_SUCCESS)
		{
			m_wndRect = rc;
			MoveWindow(hWnd, m_wndRect.left, m_wndRect.top, m_wndRect.right - m_wndRect.left, m_wndRect.bottom - m_wndRect.top, FALSE);
		}

		dwType = REG_SZ;
		dwSize = sizeof(szSymbolList);
		returnStatus = RegQueryValueEx(hKey, "Quotes", NULL, &dwType, (LPBYTE)&szSymbolList, &dwSize);

		// parse symbol list.
		TCHAR* p = &szSymbolList[0];
		while (*p != '\0') {
			TCHAR symbol[12];
			int iChar = 0;
			while (*p != '\0' && *p != ',') {
				symbol[iChar++] = *p;
				p++;
			}
			if (*p != '\0') p++;

			symbol[iChar] = '\0';
			if (symbol[0] == '\0')
				break;

			CStockPrices* unit = new CStockPrices(symbol);
			unit->m_hFontSymbol = hFontSymbol;
			unit->m_hFontPrice = hFontPrice;
			unit->m_hWnd = hWnd;
			m_vecStockPrices.push_back(unit);
		}
	}

	RegCloseKey(hKey);
}

void WriteSettings()
{
	DWORD dwValue;
	HKEY hKey;
	LONG returnStatus;

	returnStatus = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\HSD\\StockTicker", 0L, KEY_ALL_ACCESS, &hKey);
	if (returnStatus != ERROR_SUCCESS)
		returnStatus = RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\HSD\\StockTicker", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);

	if (returnStatus == ERROR_SUCCESS)
	{
		//always on top;
		dwValue = (DWORD)m_bAlwaysOnTop;
		returnStatus = RegSetKeyValue (hKey, NULL, "AlwaysOnTop", REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));
		
		dwValue = (DWORD)m_nTimerDrawSpeed;
		returnStatus = RegSetKeyValue (hKey, NULL, "DrawSpeed", REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

		dwValue = (DWORD)m_nTimerUpdateQuotes;
		returnStatus = RegSetKeyValue (hKey, NULL, "UpdateQuotesTime", REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD));

		RECT rc = m_wndRect;
		returnStatus = RegSetKeyValue (hKey, NULL, "WindowPos", REG_BINARY, (LPBYTE)&rc, sizeof(RECT));

		TCHAR buf[2048];
		memset(buf, 0, sizeof(buf));
		
		std::vector<CStockPrices*>::const_iterator iter;
		for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
			CStockPrices* prices = (*iter);
			strcat_s(buf, prices->m_szCode);
			if (iter+1 != m_vecStockPrices.end())
				strcat_s(buf, ",");
		}
		
		returnStatus = RegSetKeyValue (hKey, NULL, "Quotes", REG_SZ, (LPBYTE)&buf, sizeof(buf));
	}

	RegCloseKey(hKey);
}

void OnPaint(HWND hWnd, HDC hdc)
{
	RECT rcClient;
	::GetClientRect(hWnd, &rcClient);

	//HBRUSH hbr = CreateSolidBrush(RGB(0x0,0x0,0x0));
	//FillRect(hdc, &rcClient, hbr);

	HDC hdcTemp = CreateCompatibleDC(hdc);
	SelectObject(hdcTemp, m_hGripper);
	BitBlt(hdc, rcClient.left, rcClient.top, rcClient.left+10, rcClient.bottom, hdcTemp, 0, 0, SRCCOPY);
	
	SelectObject(hdcTemp, m_hSizer);
	BitBlt(hdc, rcClient.right-10, rcClient.top, rcClient.right, rcClient.bottom, hdcTemp, 0, 0, SRCCOPY);

	DeleteDC(hdcTemp);

	HRGN hrgn = CreateRectRgn(10, 0, rcClient.right - 10, rcClient.bottom);
	SelectClipRgn(hdc, hrgn);

	std::vector<CStockPrices*>::const_iterator iter;
	for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
		CStockPrices* prices = (*iter);
		(*iter)->OnDraw(hdc);
	}

	SelectClipRgn(hdc, NULL);
	DeleteObject(hrgn);
	//DeleteObject(hbr);
}

DWORD WINAPI UpdateQuotesFunc(LPVOID lpParam)
{
	HWND hwnd = (HWND)lpParam;

	std::vector<CStockPrices*>::const_iterator iter;
	for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
		CStockPrices* pUnit = (*iter);
#ifndef _DEBUG
		pUnit->UpdatePrice();
#endif //_DEBUG
		pUnit->CalculateLayout();
	}

	CloseHandle(hUpdateThread);
	hUpdateThread = NULL;

	// calculate date/time of last refresh..
	TCHAR buf[256], szDate[9], szTime[9];

	_strdate_s( szDate);
	_strtime_s( szTime );
	sprintf_s(buf, "Stock ticker - Last updated: %s %s", szDate, szTime);
	
	NOTIFYICONDATA nid;
	ZeroMemory(&nid,sizeof(nid));
	nid.cbSize				= sizeof(NOTIFYICONDATA);
	nid.hWnd				= hwnd;
	nid.uID					= 0;
	nid.uFlags				= NIF_TIP;
	lstrcpy(nid.szTip, buf);
	BOOL res = Shell_NotifyIcon(NIM_MODIFY, &nid);

	return 0;
}

void CALLBACK TimerFunc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
	switch(idEvent) {
		case ID_TIMER_UPDATE_QUOTES:
			
			if (hUpdateThread) {
				// dont update if we're still updating..
				return;
			}

			hUpdateThread = CreateThread( 
				NULL,                   // default security attributes
				0,                      // use default stack size  
				&UpdateQuotesFunc,       // thread function name
				hwnd,          // argument to thread function 
				0,                      // use default creation flags 
				NULL);   // returns the thread identifier 

			break;

		case ID_TIMER_SCROLL:
		{
			static CStockPrices* pEndUnit = NULL;

			if (m_vecStockPrices.size() < 1)
				return;

			if (pEndUnit == NULL)
				pEndUnit = *(m_vecStockPrices.end() - 1);

			std::vector<CStockPrices*>::const_iterator iter;
			for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
				CStockPrices* pUnit = (*iter);

				int nPos = pUnit->GetDrawPos() - m_nDrawStep;
				if (nPos + pUnit->GetWidth() < 0) {
					nPos = pEndUnit->GetDrawPos() + pEndUnit->GetWidth();
					pEndUnit = pUnit;
				}

				pUnit->SetDrawPos(nPos);
			}

			InvalidateRect(hwnd, NULL, FALSE);
			break;
		}
	}
}

void UpdateWindowState(HWND hWnd)
{
	ShowWindow(hWnd, SW_SHOW);
	if (m_bAlwaysOnTop)
		SetWindowPos(hWnd, HWND_TOPMOST, 0,0,0,0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE);
	else
		SetWindowPos(hWnd, HWND_NOTOPMOST, 0,0,0,0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOMOVE);
}

CStockPrices* HitTest(HWND hWnd, POINT pt)
{
	ScreenToClient(hWnd, &pt);
	std::vector<CStockPrices*>::const_iterator iter;
	for (iter = m_vecStockPrices.begin(); iter != m_vecStockPrices.end(); iter++) {
		CStockPrices* pUnit = (*iter);

		RECT rc = pUnit->GetRect();
		if (PtInRect(&rc, pt))
			return pUnit;
	}

	return NULL;
}

void ShowContextMenu(HWND hWnd, POINT pt)
{
	MENUITEMINFO mii;
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STATE;
	mii.fState = (m_bAlwaysOnTop) ? MFS_CHECKED : MFS_UNCHECKED;
	SetMenuItemInfo(m_hMenuPopup, IDM_ALWAYSONTOP, FALSE, &mii);
	
	// note: must set window to the foreground or the
	// menu won't disappear when it should
	SetForegroundWindow(hWnd);

	TrackPopupMenu(m_hMenuPopup, TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProcPopupInfo(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
		case WM_CREATE:
			break;

		case WM_PAINT:
		{
			TCHAR buf[BUFSIZ];

			hdc = BeginPaint(hWnd, &ps);

			RECT rcClient;
			GetClientRect(hWnd, &rcClient);

			RECT rcHitItem = m_pHitItem->GetRect();
			
			HBRUSH hbr = CreateSolidBrush(CStockPrices::m_rgbBackground);
			HBRUSH hbrOld = (HBRUSH)SelectObject(hdc, hbr);

			FillRect(hdc, &rcClient, hbr);

			SelectObject(hdc, hbrOld);
			DeleteObject(hbr);

			HFONT hOldFont = (HFONT)SelectObject(hdc, hFontPrice);
			int nMargin1 = 5;
			int nMargin2 = 80;

			RECT rcText;
			
			SetBkMode(hdc, TRANSPARENT);
			SetTextColor(hdc, RGB(255,255,255));

			// calc text height
			DrawText(hdc, "TEST", 5, &rcText, DT_CALCRECT);
			int nTextHeight = rcText.bottom - rcText.top;

			rcText.top = nMargin1;
			
			// description
			rcText.left = nMargin1;
			rcText.top = rcText.top + 2;
			rcText.right = rcClient.right - nMargin1;
			rcText.bottom = rcText.top + nTextHeight;
			strcpy_s(buf, m_pHitItem->m_szDescription);
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_LEFT);
			rcText.top += 5;


			// open price
			rcText.left = nMargin1;
			rcText.top = rcText.top + nTextHeight + 2;
			rcText.right = nMargin2 - nMargin1;
			rcText.bottom = rcText.top + nTextHeight;
			strcpy_s(buf, "Open Price:");
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_RIGHT);

			rcText.left = nMargin2;
			rcText.right = rcClient.right - nMargin1;
			sprintf_s(buf, "%.03f", m_pHitItem->m_dblOpenPrice);
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_LEFT);
			
			// change
			rcText.left = nMargin1;
			rcText.top = rcText.top + nTextHeight + 2;
			rcText.right = nMargin2 - nMargin1;
			rcText.bottom = rcText.top + nTextHeight;
			strcpy_s(buf, "Change:");
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_RIGHT);

			rcText.left = nMargin2;
			rcText.right = rcClient.right - nMargin1;
			sprintf_s(buf, "%.03f", m_pHitItem->m_dblChange);
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_LEFT);
			
			// day high
			rcText.left = nMargin1;
			rcText.top = rcText.top + nTextHeight + 2;
			rcText.right = nMargin2 - nMargin1;
			rcText.bottom = rcText.top + nTextHeight;
			strcpy_s(buf, "Day High:");
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_RIGHT);

			rcText.left = nMargin2;
			rcText.right = rcClient.right - nMargin1;
			sprintf_s(buf, "%.03f", m_pHitItem->m_dblDayHigh);
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_LEFT);
			
			// day low
			rcText.left = nMargin1;
			rcText.top = rcText.top + nTextHeight + 2;
			rcText.right = nMargin2 - nMargin1;
			rcText.bottom = rcText.top + nTextHeight;
			strcpy_s(buf, "Day low:");
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_RIGHT);

			rcText.left = nMargin2;
			rcText.right = rcClient.right - nMargin1;
			sprintf_s(buf, "%.03f", m_pHitItem->m_dblDayLow);
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_LEFT);


			// volume
			rcText.left = nMargin1;
			rcText.top = rcText.top + nTextHeight + 2;
			rcText.right = nMargin2 - nMargin1;
			rcText.bottom = rcText.top + nTextHeight;
			strcpy_s(buf, "Volume:");
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_RIGHT);

			rcText.left = nMargin2;
			rcText.right = rcClient.right - nMargin1;
			sprintf_s(buf, "%.0lf", m_pHitItem->m_dblVolume);
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_LEFT);
			
			// volume
			rcText.left = nMargin1;
			rcText.top = rcText.top + nTextHeight + 2;
			rcText.right = nMargin2 - nMargin1;
			rcText.bottom = rcText.top + nTextHeight;
			strcpy_s(buf, "Avg Volume:");
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_RIGHT);

			rcText.left = nMargin2;
			rcText.right = rcClient.right - nMargin1;
			sprintf_s(buf, "%.0lf", m_pHitItem->m_dblVolumeAverage);
			DrawText(hdc, buf, strlen(buf), &rcText, DT_VCENTER|DT_LEFT);

			
			HPEN hPen = CreatePen(PS_SOLID, 1, RGB(192,192,192));
			HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
			POINT* pts = new POINT[5];
			pts[0].x = rcClient.left+1; pts[0].y = rcClient.bottom;
			pts[1].x = rcClient.left+1; pts[1].y = rcClient.top+1;
			pts[2].x = rcClient.right-1; pts[2].y = rcClient.top+1;
			pts[3].x = rcClient.right-1; pts[3].y = rcClient.bottom;

			pts[4].x = rcClient.right-1; pts[4].y = rcClient.top;
			Polyline(hdc, pts, 5);
			SelectObject(hdc, hOldPen);
			DeleteObject(hPen);

			SelectObject(hdc, hOldFont);
			EndPaint(hWnd, &ps);
			break;
		}
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return (INT_PTR)FALSE;
}

void ShowMoreInfo(HWND hWndParent)
{
	if (m_hWndPopup == NULL || m_pHitItem == NULL)
		return;

	RECT rc = m_pHitItem->GetRect();

	int height = 150;
	int width = 180;

	POINT pt1;
	pt1.x = rc.left;
	pt1.y = rc.top;
	ClientToScreen(hWndParent, &pt1);


	RECT rcWnd;
	rcWnd.left = pt1.x;
	rcWnd.right = rcWnd.left + width;
	rcWnd.top = pt1.y - height;
	rcWnd.bottom = rcWnd.top + height;

	// make sure rcWnd is on-screen

	//MoveWindow(m_hWndPopup, rcWnd.left, rcWnd.top, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top,FALSE);
	//ShowWindow(m_hWndPopup, SW_SHOW);
	SetWindowPos(m_hWndPopup, HWND_TOPMOST, rcWnd.left, rcWnd.top, rcWnd.right - rcWnd.left, rcWnd.bottom - rcWnd.top, SWP_SHOWWINDOW);
	UpdateWindow(m_hWndPopup);

	InvalidateRect(m_hWndPopup, NULL, TRUE);
}

void HideMoreInfo()
{
	ShowWindow(m_hWndPopup, SW_HIDE);
	UpdateWindow(m_hWndPopup);
}

void ErrorExit(LPTSTR lpszFunction) 
{ 
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );

    // Display the error message and exit the process

    lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
        LocalSize(lpDisplayBuf) / sizeof(TCHAR),
        TEXT("%s failed with error %d: %s"), 
        lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);
}

