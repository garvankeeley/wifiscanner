// wifis.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "wifis.h"
#include "chrome_wifi.cpp"

#define MAX_LOADSTRING 100

int foo();

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

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

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_WIFIS, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WIFIS));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

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
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WIFIS));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_WIFIS);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

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

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

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
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	OutputDebugString(L"hi");

	foo();
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

#include "wlanapi.h"

int foo() {

  WindowsNdisApi* w = WindowsNdisApi::Create();
  std::vector<AccessPoint> apData;
  w->GetAccessPointData(apData);


  ///////////////////////////////////////////////////////////
	HANDLE hClient;
	PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
	PWLAN_INTERFACE_INFO pIfConnInfo = NULL;
	PWLAN_CONNECTION_ATTRIBUTES pConnectInfo = NULL;

	PWLAN_BSS_LIST pBssList=NULL;
	PWLAN_BSS_ENTRY  pBssEntry=NULL;
	WLAN_OPCODE_VALUE_TYPE opCode = wlan_opcode_value_type_invalid;

	DWORD dwResult = 0;
	DWORD dwMaxClient = 2;         
	DWORD dwCurVersion = 0;
	DWORD connectInfoSize = sizeof(WLAN_CONNECTION_ATTRIBUTES);

	int i;

	// Initialise the Handle
	dwResult = WlanOpenHandle(dwMaxClient, NULL, &dwCurVersion, &hClient);
	if (dwResult != ERROR_SUCCESS) 
	{    
		return 0;
	}

	// Get the Interface List
	dwResult = WlanEnumInterfaces(hClient, NULL, &pIfList);
	if (dwResult != ERROR_SUCCESS) 
	{    
		return 0;
	}

	//Loop through the List to find the connected Interface
	PWLAN_INTERFACE_INFO pIfInfo = NULL;
	for (i = 0; i < (int) pIfList->dwNumberOfItems; i++) 
	{
		pIfInfo = (WLAN_INTERFACE_INFO *) & pIfList->InterfaceInfo[i];    
		if (pIfInfo->isState == wlan_interface_state_connected) 
		{
			pIfConnInfo = pIfInfo;
			break;
		}
	}

	if ( pIfConnInfo == NULL )
		return 0;

	// Query the Interface
	dwResult = WlanQueryInterface(hClient,&pIfConnInfo->InterfaceGuid,wlan_intf_opcode_current_connection,NULL,&connectInfoSize,(PVOID *) &pConnectInfo,&opCode);
	if (dwResult != ERROR_SUCCESS) 
	{    
		return 0;
	}

	// Scan the connected SSID
	dwResult = WlanScan(hClient, &pIfConnInfo->InterfaceGuid,
		                NULL/*&pConnectInfo->wlanAssociationAttributes.dot11Ssid*/,
						NULL, NULL);
	if (dwResult != ERROR_SUCCESS) 
	{    
		return 0;
	}

	// Get the BSS Entry
	dwResult = WlanGetNetworkBssList(hClient, &pIfConnInfo->InterfaceGuid,
		NULL /*&pConnectInfo->wlanAssociationAttributes.dot11Ssid */,
		dot11_BSS_type_any,
		TRUE, NULL, &pBssList);

	if (dwResult != ERROR_SUCCESS) 
	{    
		return 0;
	}

	// Get the RSSI value
	pBssEntry=&pBssList->wlanBssEntries[0];
	return pBssEntry->lRssi;
}