#include "StdAfx.h"
#include "StockPrices.h"
#include <WinSock2.h>
#include <stdio.h>
#include <iostream>

LPCTSTR CStockPrices::YAHOO_SERVER_NAME = _T("download.finance.yahoo.com");

LPCTSTR CStockPrices::CSV_PARAMS = "GET /d/quotes.csv?s=%s&f=nl1d1t1c1ohgvp2a2&e=.csv\r\n\r\n";

COLORREF CStockPrices::m_rgbBackground = RGB(37,37,38);

/*
* Stock prices:
*
* http://download.finance.yahoo.com/d/quotes.csv?s=[symbol]&f=sl1d1t1c1ohgv&e=.csv
*
* format parameters:
* 
* a 	Ask 
* a2 	Average Daily Volume
* a5 	Ask Size
* b 	Bid
* b2 	Ask (Real-time)
* b3 	Bid (Real-time)
* b4 	Book Value
* b6 	Bid Size
* c 	Change & Percent Change
* c1 	Change
* c3 	Commission
* c6 	Change (Real-time)
* c8 	After Hours Change (Real-time)
* d 	Dividend/Share
* d1 	Last Trade Date
* d2 	Trade Date
* e 	Earnings/Share
* e1 	Error Indication (returned for symbol changed / invalid)
* e7 	EPS Estimate Current Year
* e8 	EPS Estimate Next Year
* e9 	EPS Estimate Next Quarter
* f6 	Float Shares 
* g 	Day's Low 
* h 	Day's High
* j 	52-week Low 
* k 	52-week High
* g1 	Holdings Gain Percent
* g3 	Annualized Gain
* g4 	Holdings Gain
* g5 	Holdings Gain Percent (Real-time)
* g6 	Holdings Gain (Real-time) 
* i 	More Info
* i5 	Order Book (Real-time)
* j1 	Market Capitalization
* j3 	Market Cap (Real-time)
* j4 	EBITDA
* j5 	Change From 52-week Low 
* j6 	Percent Change From 52-week Low
* k1 	Last Trade (Real-time) With Time
* k2 	Change Percent (Real-time)
* k3 	Last Trade Size
* k4 	Change From 52-week High
* k5 	Percebt Change From 52-week High 
* l 	Last Trade (With Time)
* l1 	Last Trade (Price Only)
* l2 	High Limit
* l3 	Low Limit
* m 	Day's Range
* m2 	Day's Range (Real-time)
* m3 	50-day Moving Average
* m4 	200-day Moving Average
* m5 	Change From 200-day Moving Average
* m6 	Percent Change From 200-day Moving Average 
* m7 	Change From 50-day Moving Average
* m8 	Percent Change From 50-day Moving Average
* n 	Name
* n4 	Notes
* o 	Open
* p 	Previous Close
* p1 	Price Paid
* p2 	Change in Percent 
* p5 	Price/Sales
* p6 	Price/Book
* q 	Ex-Dividend Date
* r 	P/E Ratio 
* r1 	Dividend Pay Date
* r2 	P/E Ratio (Real-time)
* r5 	PEG Ratio 
* r6 	Price/EPS Estimate Current Year
* r7 	Price/EPS Estimate Next Year
* s 	Symbol
* s1 	Shares Owned
* s7 	Short Ratio 
* t1 	Last Trade Time
* t6 	Trade Links
* t7 	Ticker Trend 
* t8 	1 yr Target Price
* v 	Volume
* v1 	Holdings Value
* v7 	Holdings Value (Real-time)
* w 	52-week Range
* w1 	Day's Value Change
* w4 	Day's Value Change (Real-time)
* x 	Stock Exchange
* y 	Dividend Yield 		
*/

/*
*
* Charts:
* 
* Small chart:
* 1 day: http://ichart.yahoo.com/t?s=MSFT
* 5 days: http://ichart.yahoo.com/v?s=MSFT
* 1 year: http://ichart.finance.yahoo.com/c/bb/m/msft
* 
* Big chart:
* 1 day: http://ichart.finance.yahoo.com/b?s=MSFT
* 5 days: http://ichart.finance.yahoo.com/w?s=MSFT
* 3 months: http://chart.finance.yahoo.com/c/3m/msft
* 6 months: http://chart.finance.yahoo.com/c/6m/msft
* 1 year: http://chart.finance.yahoo.com/c/1y/msft
* 2 years: http://chart.finance.yahoo.com/c/2y/msft
* 5 years: http://chart.finance.yahoo.com/c/5y/msft
* max: http://chart.finance.yahoo.com/c/my/msft
*/

CStockPrices::CStockPrices(LPCTSTR lpszCode)
{
	strcpy_s(m_szCode, lpszCode);
	Clear();

	m_hFontSymbol = NULL;
	m_hFontPrice = NULL;
	m_hBitmap = NULL;

	m_nDrawPosition = 0;

	m_bHighlight = FALSE;
	m_hWnd = NULL;
}

CStockPrices::~CStockPrices(void)
{
}

void CStockPrices::Clear()
{
	memset(m_szDescription, 0, sizeof(m_szDescription));
	memset(m_szChangePercent, 0, sizeof(m_szChangePercent));
	strcpy_s(m_szDescription, "<Unknown>");
	strcpy_s(m_szChangePercent, "-");
	m_dblLastPrice = 0.0f;
	m_dblChange = 0.0f;
	m_dblVolume = 0.0f;
	m_dblVolumeAverage = 0.0f;
	m_dblOpenPrice = 0.0f;
	m_dblDayHigh = 0.0f;
	m_dblDayLow = 0.0f;
}

int CStockPrices::GetNextSubString(LPTSTR& str, TCHAR* buf)
{
	int iLen = strlen(str);	// length of input string
	int iCount = 0;					// length of output string

	for (int i=0; i<iLen; i++) {
		if (*str == ',') {	// EOL
			str++;
			break;
		}
		if (*str == '\"') {	// omit quotes
			str++;
			continue;
		}

		buf[iCount++] = *str++;
	}
	buf[iCount] = '\0';

	return iCount;
}

void CStockPrices::ParseString(LPTSTR lpszString)
{
	TCHAR buf[BUFSIZ];
	TCHAR* str = lpszString;

	if (GetNextSubString(str, buf) > 0) {
		// company description
		strcpy_s(m_szDescription, buf);
	}
	if (GetNextSubString(str, buf) > 0) {
		// last price
		m_dblLastPrice = atof(buf);
	}
	if (GetNextSubString(str, buf) > 0) {
		// last trade date
	}
	if (GetNextSubString(str, buf) > 0) {
		// last trade time
	}
	if (GetNextSubString(str, buf) > 0) {
		// change
		if (buf[0] == '+')
			m_dblChange = atof(buf+1);
		else if (buf[0] == '-')
			m_dblChange = -atof(buf+1);
	}
	if (GetNextSubString(str, buf) > 0) {
		// OpenPrice
		m_dblOpenPrice = atof(buf);
	}
	if (GetNextSubString(str, buf) > 0) {
		// DayHigh
		m_dblDayHigh = atof(buf);
	}
	if (GetNextSubString(str, buf) > 0) {
		// DayLow
		m_dblDayLow = atof(buf);
	}
	if (GetNextSubString(str, buf) > 0) {
		// Volume
		m_dblVolume = atof(buf);
	}
	if (GetNextSubString(str, buf) > 0) {
		// change percent
		strcpy_s(m_szChangePercent, buf);
	}
	if (GetNextSubString(str, buf) > 0) {
		// Volume
		m_dblVolumeAverage = atof(buf);
	}
}

BOOL CStockPrices::UpdatePrice()
{
	char buff[512];
	struct hostent *hp;
	unsigned int addr;
	struct sockaddr_in server;

	// init attribs
	Clear();


	// connect to the server
	SOCKET conn;
	conn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(conn == INVALID_SOCKET)
		return FALSE;

	if(inet_addr(YAHOO_SERVER_NAME) == INADDR_NONE)
	{
		hp = gethostbyname(YAHOO_SERVER_NAME);
	}
	else
	{
		addr = inet_addr(YAHOO_SERVER_NAME);
		hp = gethostbyaddr((char*)&addr, sizeof(addr), AF_INET);
	}
	if(hp == NULL)
	{
		closesocket(conn);
		return FALSE;
	}

	server.sin_addr.s_addr = *((unsigned long*)hp->h_addr);
	server.sin_family = AF_INET;
	server.sin_port = htons(80);
	if(connect(conn, (struct sockaddr*)&server, sizeof(server)))
	{
		closesocket(conn);
		return FALSE;	
	}
	
	// get stock quotes (in .CSV format)
	sprintf_s(buff, CSV_PARAMS, m_szCode);
	send(conn, buff, strlen(buff), 0);

	memset(buff, 0, sizeof(buff));
	int len = recv(conn, buff, 512, 0);
	if (len > 0)
		ParseString(buff);

	closesocket(conn);

	return TRUE;
}

void CStockPrices::CalculateLayout()
{
	HDC hdc = GetDC(m_hWnd);

	RECT rcClient;
	RECT rcSymbol, rcPrice, rcChange;
	TCHAR szSymbol[12], szPrice[64], szChange[64];
	HFONT hFontOld;

	GetClientRect(m_hWnd, &rcClient);
	
	m_Rect.top = 0;
	m_Rect.bottom = rcClient.bottom;
	m_Rect.left = 0;
	m_Rect.right = 0;

	rcSymbol.top = m_Rect.top;
	rcSymbol.bottom = m_Rect.bottom - 2;
	rcSymbol.left = 2;
	rcSymbol.right = 0;

	memset(szSymbol, 0, sizeof(szSymbol));
	FormatSymbol(szSymbol);

	// draw the symbol
	hFontOld = (HFONT)SelectObject(hdc, m_hFontSymbol);
	DrawText(hdc, szSymbol, strlen(szSymbol), &rcSymbol, DT_LEFT|DT_CALCRECT);
	SelectObject(hdc, hFontOld);

	rcSymbol.top = -2;
	rcSymbol.bottom = m_Rect.bottom - 2;
	m_Rect.right += rcSymbol.right - rcSymbol.left;
	m_Rect.right += 4;
	

	// calculate the price
	sprintf_s(szPrice, "%.3lf", m_dblLastPrice);
	
	rcPrice = m_Rect;
	rcPrice.left = m_Rect.right + 5;
	rcPrice.right = m_Rect.right;

	hFontOld = (HFONT)SelectObject(hdc, m_hFontPrice);
	DrawText(hdc, szPrice, strlen(szPrice), &rcPrice, DT_LEFT|DT_CALCRECT);
	SelectObject(hdc, hFontOld);
	
	rcChange.top = m_Rect.top;
	rcPrice.bottom = (m_Rect.bottom - m_Rect.top) / 2;

	// show change
	sprintf_s(szChange, "%s", m_szChangePercent);
	
	rcChange = m_Rect;
	rcChange.left = rcPrice.left;
	rcChange.right = rcPrice.left;

	hFontOld = (HFONT)SelectObject(hdc, m_hFontPrice);
	DrawText(hdc, szChange, strlen(szChange), &rcChange, DT_LEFT|DT_CALCRECT);
	SelectObject(hdc, hFontOld);

	int nMaxWidth = max(rcChange.right, rcPrice.right);
	rcChange.right = rcPrice.right = nMaxWidth;

	m_Rect.right = rcPrice.right;
	m_Rect.right += 10;
	
	rcChange.top = (m_Rect.bottom - m_Rect.top) / 2;
	rcChange.bottom = m_Rect.bottom;

	// create the cache bitmap and draw
	if (m_hBitmap != NULL) {
		DeleteObject(m_hBitmap);
	}

	m_hBitmap = CreateCompatibleBitmap(hdc, m_Rect.right - m_Rect.left, m_Rect.bottom - m_Rect.top);

	HDC hdcTemp = CreateCompatibleDC(hdc);
	HBITMAP hBitmapOld = (HBITMAP)SelectObject(hdcTemp, m_hBitmap);

	HBRUSH hbr = CreateSolidBrush(m_rgbBackground);
	FillRect(hdcTemp, &m_Rect, hbr);
	
	SetBkMode(hdcTemp, TRANSPARENT);
	if (IsIndex())
		SetTextColor(hdcTemp, RGB(0xa0,0xa0,0xa0));
	else
		SetTextColor(hdcTemp, RGB(0xff,0xff,0xff));
	hFontOld = (HFONT)SelectObject(hdcTemp, m_hFontSymbol);
	DrawText(hdcTemp, szSymbol, strlen(szSymbol), &rcSymbol, DT_CENTER|DT_VCENTER);
	SelectObject(hdc, hFontOld);
	
	if (m_dblChange < 0.0) {
		SetTextColor(hdcTemp, RGB(0xff, 0x00, 0x00));
	} else {
		SetTextColor(hdcTemp, RGB(0x00, 0xff, 0x00));
	}

	hFontOld = (HFONT)SelectObject(hdcTemp, m_hFontPrice);
	DrawText(hdcTemp, szPrice, strlen(szPrice), &rcPrice, DT_RIGHT|DT_VCENTER);
	DrawText(hdcTemp, szChange, strlen(szChange), &rcChange, DT_RIGHT|DT_VCENTER);
	SelectObject(hdc, hFontOld);
	
	DeleteObject(hbr);
	SelectObject(hdcTemp, hBitmapOld);
	DeleteDC(hdcTemp);
}

void CStockPrices::SetDrawPos(int nDrawPosition)
{
	m_nDrawPosition = nDrawPosition;
}

RECT CStockPrices::GetRect() const
{
	RECT rc = m_Rect;
	rc.left += m_nDrawPosition;
	rc.right += m_nDrawPosition;
	return rc;
}

void CStockPrices::OnDraw(HDC hdc)
{
	if (m_hBitmap == NULL)
		return;

	// dont draw if we're off the screen..
	if (m_nDrawPosition < -GetWidth())
		return;

	RECT rc = GetRect();

	HDC hdcTemp = CreateCompatibleDC(hdc);
	SelectObject(hdcTemp, m_hBitmap);
	BitBlt(hdc, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, hdcTemp, 0, 0, SRCCOPY);
	DeleteDC(hdcTemp);

	if (m_bHighlight) {
		HPEN hPen = CreatePen(PS_SOLID, 1, RGB(192,192,192));
		HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
		POINT* pts = new POINT[4];
		pts[0].x = rc.left+1; pts[0].y = rc.top;
		pts[1].x = rc.left+1; pts[1].y = rc.bottom-1;
		pts[2].x = rc.right-1; pts[2].y = rc.bottom-1;
		pts[3].x = rc.right-1; pts[3].y = rc.top;
		Polyline(hdc, pts, 4);
		SelectObject(hdc, hOldPen);
		DeleteObject(hPen);
	}
}

void CStockPrices::FormatSymbol(LPTSTR lpBuf)
{
	int nLen = strlen(m_szCode);
	int nChars = 0;
	for (int nChar=0; nChar < nLen; nChar++) {
		if (m_szCode[nChar] == '\0')
			break;

		// skip ".AX" suffix
		if (m_szCode[nChar] == '.') {
			if (nLen >= (nChar + 2) && (m_szCode[nChar+1] == 'A' && m_szCode[nChar+2] == 'X'))
				break;
		}

		// skip indicies prefix
		if (m_szCode[nChar] == '^')
			continue;

		lpBuf[nChars++] = m_szCode[nChar];
	}
	lpBuf[nChars] = '\0';
}
