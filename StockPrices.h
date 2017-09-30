#pragma once

class CStockPrices
{
public:
	CStockPrices(LPCTSTR lpszCode);
	~CStockPrices(void);

public:
	TCHAR m_szCode[12];
	TCHAR m_szDescription[128];
	double m_dblLastPrice;
	double m_dblChange;
	TCHAR m_szChangePercent[128];
	double m_dblVolume;
	double m_dblVolumeAverage;
	double m_dblOpenPrice;
	double m_dblDayHigh;
	double m_dblDayLow;

	HFONT m_hFontSymbol;
	HFONT m_hFontPrice;

	HWND m_hWnd;

	static COLORREF m_rgbBackground;

	BOOL m_bHighlight;

private:
	static LPCTSTR YAHOO_SERVER_NAME;
	static LPCTSTR CSV_PARAMS;

public:
	void FormatSymbol(LPTSTR lpBuf);

	BOOL IsIndex() const {
		return (m_szCode[0] == '^') ? TRUE : FALSE;
	}

	RECT GetRect() const;

public:
	// Clears all attribute values.
	void Clear();

	// Initialize winsock for use. Call this once before calling Get().
	BOOL Initialise();

	// Gets all details for the given stock code and assigns to member attributes.
	// All existing attribute values are cleared.
	BOOL UpdatePrice();

	void CalculateLayout();

	void OnDraw(HDC hdc);

	int GetDrawPos() const { return m_nDrawPosition; }
	void SetDrawPos(int nDrawPosition);

	int GetWidth() const { return m_Rect.right - m_Rect.left; }


// Implementation
protected:
	RECT m_Rect;
	int m_nDrawPosition;
	HBITMAP m_hBitmap;

	void ParseString(LPTSTR lpszCode);
	int GetNextSubString(LPTSTR& str, TCHAR* buf);
};

