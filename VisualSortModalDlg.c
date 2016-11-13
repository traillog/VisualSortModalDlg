//
//  VisualSortModalDlg.c
//  Sorting Demo Modal Dialogs
//

#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <process.h>

#include "resource.h"

#define     SIZE_BUF    128
#define     NUMBTNS     2

// Child windows IDs
#define     ID_SORTWND              0
#define     ID_STRPAUBTN            1
#define     ID_RSTBTN               2

// Buttons size
#define     BTNS_H                  40
#define     BTNS_W                  120

// Status IDs
#define     STATUS_READY            0
#define     STATUS_INICOUNTING      1
#define     STATUS_PAUSED           2
#define     STATUS_RESUMECOUNTING   3

// Proprietary messages
#define     WM_ADDR_SET             ( WM_USER + 0 )
#define     WM_SIZE_SET             ( WM_USER + 1 )
#define     WM_RST_SET              ( WM_USER + 2 )
#define     WM_SORT_DONE            ( WM_USER + 3 )

// Thread config parameters
typedef struct paramsTag
{
     HANDLE hEvent;
     HWND   hMainWnd;
     HWND   hSortWnd;
     BOOL   bContinue;
     int    iStatus;
     int*   pElemsSet;
     int    setSize;
     int    sortMth;
} PARAMS, *PPARAMS;

// Settings parameters
typedef struct tagSETTINGS_DATA
{
    int setElemsCount;
    int setSortMethod;
} SETTINGS_DATA;

// Wnd procs
LRESULT CALLBACK WndProcMain     (HWND, UINT, WPARAM, LPARAM) ;
LRESULT CALLBACK WndProcSort     (HWND, UINT, WPARAM, LPARAM) ;
BOOL    CALLBACK SettingsDlgProc (HWND, UINT, WPARAM, LPARAM) ;
BOOL    CALLBACK AboutDlgProc    (HWND, UINT, WPARAM, LPARAM) ;

// Auxiliary functions
int AskConfirmation( HWND hwnd );
void fillSet( int* elemsSet, int setSize );
void shuffleSet( int* elemsSet, int setSize );
void setUpMappingMode( HDC hdc, int cX, int cY, int setSize );
void drawSet( HDC hdc, HPEN itemPen, HBRUSH itemBrush,
    int* elemsSet, int setSize );
void drawItem( HDC hdc, HPEN itemPen, HBRUSH itemBrush,
    int pos, int val, int setSize );
void deleteItem( HDC hdc, int pos, int val, int setSize );

// Worker thread
void Thread( PVOID pvoid );

// Sorting functions
void swapBars( HWND hSortWnd, HPEN itemPen, HBRUSH itemBrush,
    int* elemsSet, int setSize, int i, int j );
void swapItems( int* elemsSet, int i, int j );
void selectionSort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* elemsSet, int setSize );
void quicksort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int setSize, int l, int h );
int partition( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int setSize, int l, int h );

// String literals
TCHAR szAppName[] = TEXT( "VSortModal" );
TCHAR* elemsTxt[] = { TEXT( "100" ), TEXT( "1000" ), TEXT( "10000" ) };
TCHAR* methsTxt[] = { TEXT( "Selection sort" ), TEXT( "Quicksort" ) };

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    PSTR szCmdLine, int iCmdShow )
{
    MSG          msg ;
    HWND         hwnd ;
    WNDCLASS     wndclass ;

    wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
    wndclass.lpfnWndProc   = WndProcMain ;
    wndclass.cbClsExtra    = 0 ;
    wndclass.cbWndExtra    = 0 ;
    wndclass.hInstance     = hInstance ;
    wndclass.hIcon         = LoadIcon( hInstance, szAppName );
    wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wndclass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
    wndclass.lpszMenuName  = szAppName ;
    wndclass.lpszClassName = szAppName ;

    if ( !RegisterClass( &wndclass ) )
    {
        MessageBox( NULL, TEXT( "This program requires Windows NT!" ),
            TEXT( "Sorting Demo Modal Dialogs" ), MB_ICONERROR );
        return 0 ;
    }

    hwnd = CreateWindow(
        szAppName,
        TEXT( "Sorting Demo Modal Dialogs" ),
        WS_OVERLAPPEDWINDOW,
        GetSystemMetrics (SM_CXSCREEN) / 2 - 300,
        GetSystemMetrics (SM_CYSCREEN) / 2 - 250,
        600,
        500,
        NULL, NULL, hInstance, NULL );

    ShowWindow( hwnd, iCmdShow );
    UpdateWindow( hwnd ); 

    while ( GetMessage( &msg, NULL, 0, 0 ) )
    {
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

    return msg.wParam ;
}

LRESULT CALLBACK WndProcMain( HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam )
{
    HDC hdc;
    PAINTSTRUCT ps;
    WNDCLASS wndclass;
    static PARAMS params;
    static int oldElemsSizeOption;
    static int oldSortMethodOption;
    static HINSTANCE hInstance ;
    static int cxClient, cyClient;

    // Handles to child windows
    // Window to display elements to sort, and buttons
    static HWND hSortWnd, hStrBtn, hRstBtn;

    // Labels shown on the main window
    static TCHAR szBufferElems[ SIZE_BUF ] = { 0 };
    static TCHAR szBufferMethod[ SIZE_BUF ] = { 0 };

    // Settings structure
    // creation and initialization
    // used to store the settings adjusted
    // in the settings dialog
    static SETTINGS_DATA sd = { IDC_ELEM_100, IDC_SORT_BUBBLE };

    // Predifined possible the itemsSet
    static int itemsSetSizes[ 3 ] = { 100, 1000, 10000 };

    // Pointer to elements to be sorted
    static int* itemsSet = NULL;

    switch (message)
    {
    case WM_CREATE:
        // Create elements set
        // using initial default size
        itemsSet = ( int* )calloc(
            itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ], sizeof( int ) );

        // Config and register class to display elements to sort
        hInstance = ( ( LPCREATESTRUCT )lParam )->hInstance;

        wndclass.style         = CS_HREDRAW | CS_VREDRAW;
        wndclass.cbClsExtra    = 0;
        wndclass.cbWndExtra    = 0;
        wndclass.hInstance     = hInstance;
        wndclass.hIcon         = NULL;
        wndclass.hCursor       = LoadCursor( NULL, IDC_ARROW );
        wndclass.hbrBackground = ( HBRUSH )GetStockObject( WHITE_BRUSH );
        wndclass.lpszMenuName  = NULL;
        wndclass.lpfnWndProc   = WndProcSort;
        wndclass.lpszClassName = TEXT( "SortWnd" );

        RegisterClass( &wndclass );

        // Create child visualization wnd
        hSortWnd = CreateWindow(
            TEXT( "SortWnd" ), NULL,
            WS_CHILD | WS_VISIBLE,
            0, 0, 0, 0, 
            hwnd, ( HMENU )ID_SORTWND,
            hInstance, NULL);

        // Create 'Start' button
        hStrBtn = CreateWindow(
            TEXT( "button" ), TEXT( "Start" ),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ( HMENU )ID_STRPAUBTN,
            hInstance, NULL );
        
        // Create 'Reset' button
        hRstBtn = CreateWindow(
            TEXT( "button" ), TEXT( "Shuffle" ),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            0, 0, 0, 0,
            hwnd, ( HMENU )ID_RSTBTN,
            hInstance, NULL );

        // Seed the rand function
        // used to randomise elements to be sorted
        srand( ( unsigned int )time( NULL ) );

        // Initialize set
        fillSet( itemsSet,
            itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ] );
        
        // Suffle set
        shuffleSet( itemsSet,
            itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ] );

        // Pass elements set address to visualization wnd
        SendMessage( hSortWnd, WM_ADDR_SET, 0, ( LPARAM )itemsSet );

        // Pass elements set size to visualization wnd
        SendMessage( hSortWnd, WM_SIZE_SET, 0,
            ( LPARAM )( itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ] ) );

        // Set up worker thread's condfig params
        params.hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
        params.hMainWnd = hwnd;
        params.hSortWnd = hSortWnd;
        params.bContinue = FALSE;
        params.iStatus = STATUS_READY;
        params.pElemsSet = itemsSet;
        params.setSize = itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ];
        params.sortMth = sd.setSortMethod;

        // Start worker thread (paused)
        _beginthread( Thread, 0, &params );

        return 0;

    case WM_SIZE:
        // Get the size of the client area
        cxClient = LOWORD( lParam );
        cyClient = HIWORD( lParam );

        // Position 'visualization' window
        MoveWindow(
            hSortWnd,
            16, 66,
            cxClient - 32, cyClient - 146,
            TRUE );

        // Position 'Start' button
        MoveWindow(
            hStrBtn,
            cxClient / 2 - BTNS_W - 10, cyClient - 60,
            BTNS_W, BTNS_H,
            TRUE );

        // Position 'Reset' button
        MoveWindow(
            hRstBtn,
            cxClient / 2 + 10, cyClient - 60,
            BTNS_W, BTNS_H,
            TRUE );

        return 0;

    case WM_GETMINMAXINFO:
        // Limit the minimum size of the main window
        // as it is being resized with the mouse
        ( ( MINMAXINFO* )( lParam ) )->ptMinTrackSize.x = 308;
        ( ( MINMAXINFO* )( lParam ) )->ptMinTrackSize.y = 312;
        return 0;

    case WM_COMMAND:
        switch ( LOWORD( wParam ) )
        {
        case IDM_APP_SETTINGS:
            // Save current elems set size
            oldElemsSizeOption = sd.setElemsCount;

            // Save current sorting method type
            oldSortMethodOption = sd.setSortMethod;

            // Call the settings dialog
            if ( DialogBoxParam (hInstance, TEXT ("SETTINGSBOX"),
                hwnd, SettingsDlgProc, (LPARAM)&sd) )
            {
                // Dlg closed using OK, so update application config

                // Validate changes in elems set size
                if ( oldElemsSizeOption != sd.setElemsCount )
                {
                    // Destroy current set
                    free( itemsSet );

                    // Create a new set
                    itemsSet = ( int* )calloc(
                        itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ],
                        sizeof( int ) );

                    // Ini set
                    fillSet( itemsSet,
                        itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ] );

                    // Shuffle set
                    shuffleSet( itemsSet,
                        itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ] );

                    // Pass elements set address to visualization wnd
                    SendMessage( hSortWnd, WM_ADDR_SET, 0, ( LPARAM )itemsSet );

                    // Pass elements set size to visualization wnd
                    SendMessage( hSortWnd, WM_SIZE_SET, 0,
                        ( LPARAM )( itemsSetSizes[
                            sd.setElemsCount - IDC_ELEM_100 ] ) );

                    // Uppdate working thread config
                    params.pElemsSet = itemsSet;
                    params.setSize =
                        itemsSetSizes[ sd.setElemsCount - IDC_ELEM_100 ];
                }

                if ( oldSortMethodOption != sd.setSortMethod )
                {
                    // Uppdate working thread config
                    params.sortMth = sd.setSortMethod;
                }

                // Repaint client area
                InvalidateRect (hwnd, NULL, TRUE) ;
            }
            return 0;

        case IDM_APP_ABOUT:
            DialogBox( hInstance, TEXT( "AboutBox" ), hwnd, AboutDlgProc );
            return 0;

        case IDM_APP_EXIT:
            SendMessage( hwnd, WM_CLOSE, 0, 0 );
            return 0;
        }

        if ( LOWORD( wParam ) == ID_STRPAUBTN &&
             HIWORD( wParam ) == BN_CLICKED )
        {
            // Start/Pause button pressed
            if ( params.iStatus == STATUS_READY )
            {
                // From READY to INICOUNTING
                params.iStatus = STATUS_INICOUNTING;
                params.bContinue = TRUE;

                SetEvent( params.hEvent );

                EnableWindow( hRstBtn, FALSE );
                SetWindowText( hStrBtn, TEXT( "Pause" ) );
            }
            else if ( params.iStatus == STATUS_INICOUNTING )
            {
                // From INICOUNTING to PAUSED
                params.iStatus = STATUS_PAUSED;
                params.bContinue = FALSE;

                EnableWindow( hRstBtn, TRUE );
                SetWindowText( hStrBtn, TEXT( "Resume" ) );
            }
            else if ( params.iStatus == STATUS_PAUSED )
            {
                // From PAUSED to RESUMECOUNTING
                params.iStatus = STATUS_RESUMECOUNTING;
                params.bContinue = TRUE;

                SetEvent( params.hEvent );

                EnableWindow( hRstBtn, FALSE );
                SetWindowText( hStrBtn, TEXT( "Pause" ) );
            }
            else if ( params.iStatus == STATUS_RESUMECOUNTING )
            {
                // From RESUMECOUNTING to PAUSE
                params.iStatus = STATUS_PAUSED;
                params.bContinue = FALSE;

                EnableWindow( hRstBtn, TRUE );
                SetWindowText( hStrBtn, TEXT( "Resume" ) );
            }
        }

        if ( LOWORD( wParam ) == ID_RSTBTN &&      
             HIWORD( wParam ) == BN_CLICKED )
        {
            // Reset button pressed
            if ( params.iStatus == STATUS_PAUSED ||
                 params.iStatus == STATUS_READY )
            {
                // From PAUSED to READY
                params.iStatus = STATUS_READY;
                SetWindowText( hStrBtn, TEXT( "Start" ) );

                SendMessage( params.hSortWnd, WM_RST_SET, 0, 0 );
            }
        }

        return 0 ;

    case WM_PAINT:
        hdc = BeginPaint( hwnd, &ps );

        // Set up text labels
        swprintf( szBufferElems, SIZE_BUF, TEXT( "Elements : %s" ),
            elemsTxt[ sd.setElemsCount - IDC_ELEM_100 ] );

        swprintf( szBufferMethod, SIZE_BUF, TEXT( "Method   : %s" ),
            methsTxt[ sd.setSortMethod - IDC_SORT_BUBBLE ] );

        // Output text labels
        SelectObject( hdc, GetStockObject( SYSTEM_FIXED_FONT ) );

        SetTextAlign( hdc, TA_TOP | TA_LEFT );

        TextOut( hdc, 16, 16, szBufferElems,
            wcsnlen( szBufferElems, SIZE_BUF ) );

        TextOut( hdc, 16, 38, szBufferMethod,
            wcsnlen( szBufferMethod, SIZE_BUF ) );

        EndPaint( hwnd, &ps );
        return 0 ;

    case WM_CLOSE:
        if ( IDYES == AskConfirmation( hwnd ) )
        {
            DestroyWindow( hwnd );
        }
        return 0 ;

    case WM_SORT_DONE :
        // Sort done
        params.iStatus = STATUS_READY;
        params.bContinue = FALSE;
        EnableWindow( hRstBtn, TRUE );
        SetWindowText( hStrBtn, TEXT( "Start" ) );
        return 0;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        return 0;
    }

    return DefWindowProc (hwnd, message, wParam, lParam) ;
}

LRESULT CALLBACK WndProcSort( HWND hwnd, UINT message,
    WPARAM wParam, LPARAM lParam )
{
    HDC hdc;
    PAINTSTRUCT ps;
    static int  cxClient, cyClient;
    static HPEN redPen;
    static HBRUSH redBrush;
    static int* ptrItemsSet;
    static int sizeItemsSet;

    switch ( message )
    {
    case WM_CREATE :
        redPen = CreatePen( PS_SOLID, 0, RGB( 0xFF, 0x00, 0x00 ) );
        redBrush = CreateSolidBrush( RGB( 0xFF, 0x00, 0x00 ) );
        return 0;

    case WM_SIZE :
        cxClient = LOWORD( lParam );
        cyClient = HIWORD( lParam );
        return 0;

    case WM_ADDR_SET :
        // Store set address
        ptrItemsSet = ( int* )lParam;
        return 0;

    case WM_SIZE_SET :
        // Store set address
        sizeItemsSet = ( int )lParam;
        return 0;

    case WM_PAINT :
        // Redraw set
        hdc = BeginPaint( hwnd, &ps );

        setUpMappingMode( hdc, cxClient, cyClient, sizeItemsSet );
        drawSet( hdc, redPen, redBrush, ptrItemsSet, sizeItemsSet );

        EndPaint( hwnd, &ps );
        return 0 ;

    case WM_RST_SET :
        // Initialize set
        fillSet( ptrItemsSet, sizeItemsSet );
        shuffleSet( ptrItemsSet, sizeItemsSet );

        // Redraw set
        InvalidateRect( hwnd, NULL, TRUE );
        return 0;

    case WM_DESTROY :
        // Clean up
        DeleteObject( redPen );
        DeleteObject( redBrush );
        return 0;
    }

    return DefWindowProc( hwnd, message, wParam, lParam );
}

BOOL CALLBACK AboutDlgProc (HWND hDlg, UINT message, 
    WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG :
        return TRUE ;

    case WM_COMMAND :
        switch (LOWORD (wParam))
        {
        case IDOK :
        case IDCANCEL :
            EndDialog (hDlg, 0) ;
            return TRUE ;
        }
        break ;
    }

    return FALSE ;
}

BOOL CALLBACK SettingsDlgProc (HWND hDlg, UINT message, 
    WPARAM wParam, LPARAM lParam)
{
    static SETTINGS_DATA sd, *psd;

    switch (message)
    {
    case WM_INITDIALOG :
        psd = (SETTINGS_DATA *)lParam ;
        sd = *psd ;

        CheckRadioButton (hDlg, IDC_ELEM_100, IDC_ELEM_10000,
            sd.setElemsCount) ;
        CheckRadioButton (hDlg, IDC_SORT_BUBBLE, IDC_SORT_QUICK,
            sd.setSortMethod) ;

        SetFocus (GetDlgItem (hDlg, sd.setElemsCount)) ;

        return TRUE ;

    case WM_COMMAND :
        switch (LOWORD (wParam))
        {
        case IDOK:
            *psd = sd;
            EndDialog (hDlg, TRUE) ;
            return TRUE ;

        case IDCANCEL:
            EndDialog (hDlg, FALSE) ;
            return TRUE ;

        case IDC_ELEM_100:
        case IDC_ELEM_1000:
        case IDC_ELEM_10000:
            sd.setElemsCount = LOWORD (wParam) ;
            CheckRadioButton (hDlg, IDC_ELEM_100, IDC_ELEM_10000,
                LOWORD (wParam)) ;
            return TRUE ;

        case IDC_SORT_BUBBLE:
        case IDC_SORT_QUICK:
            sd.setSortMethod = LOWORD (wParam) ;
            CheckRadioButton (hDlg, IDC_SORT_BUBBLE, IDC_SORT_QUICK,
                LOWORD (wParam)) ;
            return TRUE ;
        }
        break ;
    }

    return FALSE ;
}

void Thread( PVOID pvoid )
{
    volatile PPARAMS pparams;
    static HPEN redPen;
    static HBRUSH redBrush;

    pparams = ( PPARAMS )pvoid;

    redPen = CreatePen( PS_SOLID, 0, RGB( 0xFF, 0x00, 0x00 ) );
    redBrush = CreateSolidBrush( RGB( 0xFF, 0x00, 0x00 ) );

    while ( TRUE )
    {
        WaitForSingleObject( pparams->hEvent, INFINITE );

        // Sort set, uncomment desired method

        switch ( pparams->sortMth )
        {
        case IDC_SORT_BUBBLE :
            selectionSort( pparams->hSortWnd, &( pparams->bContinue ),
                pparams->iStatus, redPen, redBrush,
                pparams->pElemsSet, pparams->setSize );
            break;

        case IDC_SORT_QUICK :
            quicksort( pparams->hSortWnd, &( pparams->bContinue ),
                pparams->iStatus, redPen, redBrush,
                pparams->pElemsSet, pparams->setSize,
                0, pparams->setSize - 1 );
            break;

        default :
            break;
        }

        // Report sorting is done
        if ( pparams->bContinue == TRUE )
            SendMessage( pparams->hMainWnd, WM_SORT_DONE, 0, 0 );
    }

    // Clean up
    DeleteObject( redPen );
    DeleteObject( redBrush );
}

int AskConfirmation( HWND hwnd )
{
    return MessageBox ( hwnd, TEXT( "Really want to close the program?" ),
        TEXT( "Quit Sorting Demo..." ), MB_YESNO | MB_ICONQUESTION );
}

void fillSet( int* elemsSet, int setSize )
{
    int i = 0;

    for ( i = 0; i < setSize; i++ )
    {
        elemsSet[ i ] = i + 1;
    }
}

void shuffleSet( int* elemsSet, int setSize )
{
    int i = 0;      // current item
    int j = 0;      // random chosen item
    int tmp = 0;    // tmp value

    // Iterate over all items
    for ( i = 0; i < setSize; i++ )
    {
        // Choose one item randomly
        j = rand() % setSize;
        
        // Swap current and chosen items
        tmp = elemsSet[ i ];
        elemsSet[ i ] = elemsSet[ j ];
        elemsSet[ j ] = tmp;
    }
}

void setUpMappingMode( HDC hdc, int cX, int cY, int setSize )
{
    // Set up mapping mode
    SetMapMode( hdc, MM_ANISOTROPIC );

    // Set up extents
    SetWindowExtEx( hdc, setSize, setSize, NULL );
    SetViewportExtEx( hdc, cX - 1, cY - 1, NULL );

    // Set up 'viewport' origin
    SetViewportOrgEx( hdc, 0, 0, NULL);
}

void drawSet( HDC hdc, HPEN itemPen, HBRUSH itemBrush,
    int* elemsSet, int setSize )
{
    int i = 0;

    for ( i = 0; i < setSize; i++ )
    {
        drawItem( hdc, itemPen, itemBrush, i, elemsSet[ i ], setSize );
    }
}

void drawItem( HDC hdc, HPEN itemPen, HBRUSH itemBrush,
    int pos, int val, int setSize )
{
    // Validate args
    pos = max( 0, min( pos, setSize - 1 ) );
    val = max( 1, min( val, setSize ) );

    // Select pen and brush
    SelectObject( hdc, itemPen );
    SelectObject( hdc, itemBrush );
    
    // Draw item
    Rectangle( hdc, 0, pos + 1, val, pos );
}

void deleteItem( HDC hdc, int pos, int val, int setSize )
{
    // Validate args
    pos = max( 0, min( pos, setSize - 1 ) );
    val = max( 1, min( val, setSize ) );

    // Select pen and brush
    SelectObject( hdc, GetStockObject( WHITE_PEN ) );
    SelectObject( hdc, GetStockObject( WHITE_BRUSH ) );
    
    // Draw item
    Rectangle( hdc, 0, pos + 1, val, pos );
}

void swapBars( HWND hSortWnd, HPEN itemPen, HBRUSH itemBrush,
    int* elemsSet, int setSize, int i, int j )
{
    HDC hdc;
    RECT rcClientSortWnd;
    static int cxSortWnd, cySortWnd;

    // Get sort win client area size
    GetClientRect( hSortWnd, &rcClientSortWnd );
    cxSortWnd = rcClientSortWnd.right - rcClientSortWnd.left;
    cySortWnd = rcClientSortWnd.bottom - rcClientSortWnd.top;

    hdc = GetDC( hSortWnd );

    setUpMappingMode( hdc, cxSortWnd, cySortWnd, setSize );

    deleteItem( hdc, i, elemsSet[ i ], setSize );

    deleteItem( hdc, j, elemsSet[ j ], setSize );

    drawItem( hdc, itemPen, itemBrush, i, elemsSet[ j ], setSize );

    drawItem( hdc, itemPen, itemBrush, j, elemsSet[ i ], setSize );

    ReleaseDC( hSortWnd, hdc );
}

void swapItems( int* elemsSet, int i, int j )
{
    int tmp = 0;

    tmp = elemsSet[ i ];
    elemsSet[ i ] = elemsSet[ j ];
    elemsSet[ j ] = tmp;
}

void selectionSort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* elemsSet, int setSize )
{
    int i, j;   // Set indices
    int sml;    // Smallest item found in current pass
    static int lastI;

    if ( iStatus == STATUS_INICOUNTING )
        lastI = 0;

    // Loop over array size - 1 items
    for ( i = lastI; i < setSize - 1 && (*pbContinue); i++ )
    {
        sml = i;    // Initialize smallest elem found

        // Loop over remaining array
        for ( j = i + 1; j < setSize && (*pbContinue); j++ )
        {
            if ( elemsSet[ j ] < elemsSet[ sml ] )
            {
                sml = j;
            }
        }
        
        // Swap smallest and current analysed item on graphic
        swapBars( hSortWnd, itemPen, itemBrush, elemsSet, setSize, i, sml );

        // Swap smallest and current analysed item on array
        swapItems( elemsSet, i, sml );

        Sleep( 25 );
    }

    if ( (*pbContinue) == FALSE )
        lastI = i;
}

void quicksort( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int setSize, int l, int h )
{
    int p = 0;

    if ( l < h )
    {
        p = partition( hSortWnd, pbContinue, iStatus, itemPen, itemBrush,
                set, setSize, l, h );

        quicksort( hSortWnd, pbContinue, iStatus, itemPen, itemBrush,
            set, setSize, l, p - 1 );
        
        quicksort( hSortWnd, pbContinue, iStatus, itemPen, itemBrush,
            set, setSize, p + 1, h );
    }
}

int partition( HWND hSortWnd, BOOL* pbContinue, int iStatus,
    HPEN itemPen, HBRUSH itemBrush, int* set, int setSize, int l, int h )
{
    int pivot = set[ h ];
    int i = l;
    int j = l;

    for ( j = l; j < h; j++ )
    {
        if ( set[ j ] <= pivot )
        {
            swapBars( hSortWnd, itemPen, itemBrush, set, setSize, i, j );
            swapItems( set, i, j );
            i++;

            Sleep( 25 );
        }
    }

    swapBars( hSortWnd, itemPen, itemBrush, set, setSize, i, h );
    swapItems( set, i, h );

    Sleep( 25 );

    return i;
}
