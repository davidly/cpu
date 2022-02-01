#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <stdio.h>
#include <math.h>

#include "djlres.hxx"

#pragma comment(lib, "advapi32.lib")

#define REGISTRY_APP_NAME L"SOFTWARE\\davidlycpu"
#define REGISTRY_WINDOW_POSITION L"WindowPosition"

LRESULT CALLBACK WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

HFONT fontText;
COLORREF crRainbow[ 360 ];
DWORD coreCount = 1;

static void HSVToRGB( int h, int s, int v, int &r, int &g, int &b )
{
    // Assume h 0..359, s 0..255, v 0..255.
    const int sixtyDegrees = 60; // 60 out of 360, and 43 out of 256 (43, 85, 171 are color changes)
    const int divby = 255; // use 256 when performance is critical and accuracy is not.

    int hi = ( h / sixtyDegrees ); 
    int f = ( 255 * ( h % sixtyDegrees ) ) / sixtyDegrees; 
    int p = ( v * ( 255 - s ) ) / divby;

    int t = 0, q = 0;
    if ( hi & 0x1 )
        q = ( v * ( 255 - ( ( f * s ) / divby ) ) ) / divby;
    else
        t = ( v * ( 255 - ( ( ( 255 - f ) * s ) / divby ) ) ) / divby;

    if ( 0 == hi )
    {
        r = v; g = t; b = p;
    }
    else if ( 1 == hi )
    {
        r = q; g = v; b = p;
    }
    else if ( 2 == hi )
    {
        r = p; g = v; b = t;
    }
    else if ( 3 == hi )
    {
        r = p; g = q; b = v;
    }
    else if ( 4 == hi )
    {
        r = t; g = p; b = v;
    }
    else if ( 5 == hi )
    {
        r = v; g = p; b = q;
    }
    else
    {
        r = 0; g = 0; b = 0;
    }
} //HSVToRGB

int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow )
{
    SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );

    SYSTEM_INFO sysInfo;
    GetSystemInfo( &sysInfo );
    coreCount = sysInfo.dwNumberOfProcessors;

    for ( int c = 0; c < _countof( crRainbow ); c++ )
    {
        int r, g, b;
        HSVToRGB( c, 0x70, 0xc0, r, g, b );
        crRainbow[ c ] = r | ( g << 8 ) | ( b << 16 );
    }

    RECT rectDesk;
    GetWindowRect( GetDesktopWindow(), &rectDesk );

    int fontHeight = (int) round( (double) rectDesk.bottom * 0.0208333 );
    int windowWidth = (int) round( (double) fontHeight * 6.666667 );

    fontText = CreateFont( fontHeight, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_OUTLINE_PRECIS,
                           CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, VARIABLE_PITCH, L"Tahoma" ); //TEXT("Arial") );

    int posLeft = rectDesk.right - windowWidth;
    int posTop = 0;

    // Any command-line argument will override the registry setting with the default window position

    if ( 0 == pCmdLine[ 0 ] )
    {
        WCHAR awcPos[ 100 ];
        BOOL fFound = CDJLRegistry::readStringFromRegistry( HKEY_CURRENT_USER, REGISTRY_APP_NAME, REGISTRY_WINDOW_POSITION, awcPos, sizeof( awcPos ) );
        if ( fFound )
            swscanf( awcPos, L"%d %d", &posLeft, &posTop );
    }

    const WCHAR CLASS_NAME[] = L"CPU-davidly-Class";
    WNDCLASS wc = { };
    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hIcon         = LoadIcon( hInstance, MAKEINTRESOURCE( 100 ) ) ;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass( &wc );

    HWND hwnd = CreateWindowEx( WS_EX_TOOLWINDOW, CLASS_NAME, L"CPU", WS_POPUP, posLeft, posTop, windowWidth, fontHeight, NULL, NULL, hInstance, NULL );
    if ( NULL == hwnd )
        return 0;

    ShowWindow( hwnd, nCmdShow );

    SetTimer( hwnd, 0, 1000, NULL );

    SetProcessWorkingSetSize( GetCurrentProcess(), ~0, ~0 );

    MSG msg = { };
    while ( GetMessage( &msg, NULL, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    return 0;
} //wWinMain

static float CalculateCPULoad( unsigned long long idleTicks, unsigned long long totalTicks )
{
    static unsigned long long _previousTotalTicks = 0;
    static unsigned long long _previousIdleTicks = 0;

    unsigned long long totalTicksSinceLastTime = totalTicks - _previousTotalTicks;
    unsigned long long idleTicksSinceLastTime  = idleTicks - _previousIdleTicks;

    float ret = 1.0f - ( ( totalTicksSinceLastTime > 0 ) ? ( (float) idleTicksSinceLastTime ) / totalTicksSinceLastTime : 0 );

    _previousTotalTicks = totalTicks;
    _previousIdleTicks  = idleTicks;

    return ret;
} //CalculateCPULoad

static unsigned long long FileTimeToInt64( const FILETIME & ft )
{
    return ( ( (unsigned long long)( ft.dwHighDateTime ) ) << 32 ) | ((unsigned long long) ft.dwLowDateTime );
} //FileTimeToInt64

// Returns 1.0f for "CPU fully pinned", 0.0f for "CPU idle", or somewhere in between
// You'll need to call this at regular intervals, since it measures the load between
// the previous call and the current one.  Returns 0 on error.

float GetCPULoad()
{
    FILETIME idleTime, kernelTime, userTime;

    if ( GetSystemTimes( &idleTime, &kernelTime, &userTime ) )
        return CalculateCPULoad( FileTimeToInt64( idleTime ), FileTimeToInt64( kernelTime ) + FileTimeToInt64( userTime ) );

    return 0.0;
} //GetCPULoad

LRESULT CALLBACK WindowProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
    switch ( uMsg )
    {
        case WM_DESTROY:
        {
            RECT rectPos;
            GetWindowRect( hwnd, &rectPos );

            WCHAR awcPos[ 100 ];
            int len = swprintf_s( awcPos, _countof( awcPos ), L"%d %d", rectPos.left, rectPos.top );
            CDJLRegistry::writeStringToRegistry( HKEY_CURRENT_USER, REGISTRY_APP_NAME, REGISTRY_WINDOW_POSITION, awcPos );

            PostQuitMessage( 0 );
            return 0;
        }

        case WM_TIMER:
        {
            InvalidateRect( hwnd, NULL, TRUE );
            return 0;
        }

        case WM_CHAR:
        {
            if ( 'q' == wParam || 0x1b == wParam ) // q or ESC
                DestroyWindow( hwnd );
            return 0;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint( hwnd, &ps );
    
            float load = GetCPULoad();
            COLORREF cr = crRainbow[ (int) round( load * ( _countof( crRainbow ) - 1 ) ) ];

            // 16 for lots of cores some day. 14 WCHARs needed including null terminator: XXX.X% = YY.Y0

            WCHAR awcCPU[ 16 ];
            int len = swprintf_s( awcCPU, _countof( awcCPU ), L"%.*f%% = %.*f", 1, 100.0 * load, 1, load * coreCount );

            if ( -1 != len )
            {
                HFONT fontOld = (HFONT) SelectObject( hdc, fontText );
                COLORREF crOld = SetBkColor( hdc, cr );
                UINT taOld = SetTextAlign( hdc, TA_CENTER );
    
                RECT rect;
                GetClientRect( hwnd, &rect );
    
                ExtTextOut( hdc, rect.right / 2, 0, ETO_OPAQUE, &rect, awcCPU, len, NULL );

                SetTextAlign( hdc, taOld );
                SetBkColor( hdc, crOld );
                SelectObject( hdc, fontOld );
            }

            EndPaint( hwnd, &ps );
            return 0;
        }

        case WM_NCHITTEST:
        {
            // Turn the whole window into what Windows thinks is the title bar so the user can drag the window around

            LRESULT hittest = DefWindowProc( hwnd, uMsg, wParam, lParam );
            if ( HTCLIENT == hittest )
                return HTCAPTION;

            return lParam;
        }
    }

    return DefWindowProc( hwnd, uMsg, wParam, lParam );
} //WindowProc
