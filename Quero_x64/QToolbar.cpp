/*
   Quero Toolbar
   http://www.quero.at/
   Copyright 2013 Viktor Krammer

   This file is part of Quero Toolbar.

   Quero Toolbar is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Quero Toolbar is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Quero Toolbar.  If not, see <http://www.gnu.org/licenses/>.
*/

// QToolbar.cpp : Implementation of CQToolbar

#include "stdafx.h"
#include "resource.h"
#include "Quero.h"
#include "QToolbar.h"
#include "QueroBand.h"

#include <shlwapi.h>
#include <urlhist.h>
#include <Usp10.h>
#include <Lmcons.h>
#include <Sddl.h>

// Windows Vista specifc include files, variables and functions
#ifdef COMPILE_FOR_WINDOWS_VISTA
// Aero global variables and functions
#include <dwmapi.h>
// Menu icon
#include "MenuIcon.h"
#endif

#include "..\QueroBroker\QueroBroker_i.c"

#define LOGOGAP 2

const int g_NavOpenMap[4]={0,navOpenInNewWindow,navOpenInNewTab,navOpenBackgroundTab};

#define MapNewWinTabToNavOpen(newWinTab) g_NavOpenMap[newWinTab]

/////////////////////////////////////////////////////////////////////////////
// CQToolbar

// Static members
HICON CQToolbar::g_Icons[NICONS]={0,0,0,0,0,0,0};
bool CQToolbar::g_bIconsLoaded=false;
VersionInfo CQToolbar::QueroVersion;

CQToolbar::CQToolbar() : m_pBrowser(NULL) , m_pBand(NULL) , m_IconAnimation(this), pQueroBroker(NULL)
#ifdef COMPILE_FOR_WINDOWS_VISTA
,m_ReBar(this)
#endif
{
	HKEY hKeyLocal;
	DWORD size;
	TCHAR QName[64+UNLEN];
	TCHAR UserName[UNLEN+1];
	HANDLE hQSharedDataMutex;
	bool bCloseQueroSharedDataMutex;

	QueroInstanceId=UNASSIGNED_INSTANCE_ID;
	IsNewProcess=false;
	hPopupMenu=NULL;
	bToolbarCreated=false;

	currentURL[0]=_T('\0');
	currentAsciiURL[0]=_T('\0');
	HostStartIndex=0;
	HostEndIndex=0;
	DomainStartIndex=0;
	DomainStartIndexAscii=0;
	CoreDomainStartIndex=0;
	CoreDomainEndIndex=0;
	CoreDomainStartExtent=0;
	CoreDomainEndExtent=0;
	HostStartIndexAscii=0;
	HostEndIndexAscii=0;
	IsIPv6=false;
	SpecialCharsInURL=SPECIALCHARS_NON;
	nEmbedIcons=0;
	beforeURL[0]=_T('\0');
	BeforeHostStartIndex=0;
	BeforeHostEndIndex=0;
	BeforeDomainStartIndex=0;
	LastQueryURL=NULL;
	bstrCurrentDocumentTitle=NULL;
	
	Certificate_Organization_Extent=0;

	URLNavigationTime.dwHighDateTime=0;
	URLNavigationTime.dwLowDateTime=0;
	m_AutoComplete=NULL;
	ClearLastFoundText();
	Find_Occurence=0;
	LastProgress=0;
	PreviewIDN=false;
	nHighlightedWords=0;
	Highlight=false;
	IsActive=g_IE_MajorVersion<7;
	bFirstRun=false;
	ZoomFactor=100;

	HistoryIndex=0;
	LTimeHistory=0;
	HistoryEntrySelected=false;
	BlockedContentIndex=0;
	ContentBlocked=false;
	bTemporarilyUnblock=false;
	CurrentProfileId=-1;
	CurrentEngineIndex=0;
	nengines=0;
	nseparators=0;
	currentIcon=NULL;
	currentType=TYPE_UNKNOWN;
	currentIconOffset=0;
	currentFavIcon=NULL;
	Searching=false;
	SecureLockIcon_IE=false;
	SecureLockIcon_Quero=false;
	NavigationFailed=false;
	NavigationPending=false;
	PhraseNotFound=false;
	InternalLink=false;
	ImFeelingLucky=false;
	chooseProfile=false;
	LastHighlightedItemIndex=INDEX_UNDEFINED;
	Times_LastHighlightedItem_Identical=0;
	LastPopUpTime.dwHighDateTime=0;
	bAllowOnePopUp=false;

	hToolbarBckgrndMemDC=NULL;
	hToolbarBckgrndBitmap=NULL;
	ToolbarBackgroundState=g_ToolbarBackgroundState-1;

	LastHistoryEntry.Query=NULL;

	// Create Quero shared data mutex: one per process

	StringCbPrintf(QName,sizeof QName,L"QSharedDataMutex%X",GetCurrentProcessId());
	hQSharedDataMutex=CreateMutex(NULL,FALSE,QName);
	bCloseQueroSharedDataMutex=true;

	// Initialize shared data structures

	if(WaitForSingleObject(hQSharedDataMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		// Increment Quero instance count
		g_QueroInstanceCount++;

		// IsNewProcess?
		if(g_QueroInstanceCount==1)
		{
			// Create global strucures

			IsNewProcess=true;

			// Save shared data mutex to global handle

			if(g_hQSharedDataMutex==NULL)
			{
				g_hQSharedDataMutex=hQSharedDataMutex;
				bCloseQueroSharedDataMutex=false;
			}

			// Set the time when Quero is started (used to avoid highlighting old search terms)

			CoFileTimeNow(&g_QueroStartTime);

			// Initialize Thread Local Storage

			ZeroMemory(&QThreadLocalStg,sizeof QThreadLocalStg);

			// Create Mutexes

			size=sizeof UserName;
			if(GetUserName(UserName,&size)==0) UserName[0]=0;
			StringCbPrintf(QName,sizeof QName,L"QSharedMemoryMutex_%s",UserName);

			g_hQSharedListMutex=CreateMutex(NULL,FALSE,NULL);

			SECURITY_ATTRIBUTES *pSecAttr=NULL;
			// Set security attributes of shared memory and mutex to IL Low
			#ifdef COMPILE_FOR_WINDOWS_VISTA
			SECURITY_ATTRIBUTES secAttr;
			PSECURITY_DESCRIPTOR pSD;
			PACL pSacl;
			BOOL bSaclPresent;
			BOOL bSaclDefaulted;
			BYTE secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
			secAttr.nLength = sizeof(secAttr);
			secAttr.bInheritHandle = FALSE;
			secAttr.lpSecurityDescriptor = &secDesc;
			pSD=NULL;
			if(	InitializeSecurityDescriptor(secAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION) &&
				SetSecurityDescriptorDacl(secAttr.lpSecurityDescriptor, TRUE, NULL, FALSE) &&
				ConvertStringSecurityDescriptorToSecurityDescriptor(L"S:(ML;;NW;;;LW)",SDDL_REVISION_1,&pSD,NULL) &&
				GetSecurityDescriptorSacl(pSD,&bSaclPresent,&pSacl,&bSaclDefaulted) &&
				SetSecurityDescriptorSacl(secAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE) ) pSecAttr=&secAttr;
			#endif

			g_hQSharedMemoryMutex=CreateMutex(pSecAttr,FALSE,QName);

			// Create Shared Memory and Mutex

			if(g_hQSharedMemoryMutex) // In Windows Vista access to global mutex and shared memory could be denied if another higher priveleged IE process is running and created the shared memory
			{
				StringCbPrintf(QName,sizeof QName,L"QSharedMemory_%s",UserName);
				g_hQSharedMemoryFileMapping=CreateFileMapping(INVALID_HANDLE_VALUE,pSecAttr,PAGE_READWRITE|SEC_COMMIT,0,sizeof g_QSharedMemory,QName);

				if(g_hQSharedMemoryFileMapping)
				{
					g_QSharedMemory=(QSharedMemory*)MapViewOfFile(g_hQSharedMemoryFileMapping,FILE_MAP_ALL_ACCESS,0,0,0);

					if(g_QSharedMemory && GetLastError()!=ERROR_ALREADY_EXISTS)
					{
						g_QSharedMemory->LTimeHistory=1;
						g_QSharedMemory->LTimeURLs=1;
						g_QSharedMemory->LTimeWhiteList=1;
					}
				}
			}
			else g_hQSharedMemoryMutex=CreateMutex(NULL,FALSE,NULL);
			
			#ifdef COMPILE_FOR_WINDOWS_VISTA
			if(pSD) LocalFree(pSD);
			#endif

			// Query Windows version
			OSVERSIONINFO VersionInfo;

			VersionInfo.dwOSVersionInfoSize=sizeof VersionInfo;
			GetVersionEx(&VersionInfo);
			g_WindowsVersion=((VersionInfo.dwMajorVersion)<<8)+((VersionInfo.dwMinorVersion)&0xFF);

			// Retrieve IE version

			TCHAR IEVersion[255];
			DWORD size;

			size=sizeof IEVersion;

			if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,L"Software\\Microsoft\\Internet Explorer\\",0,KEY_READ,&hKeyLocal)==ERROR_SUCCESS)
			{
				if(RegQueryValueEx(hKeyLocal,L"svcVersion",0,NULL,(LPBYTE)&IEVersion,&size)==ERROR_SUCCESS)
				{
					g_IE_MajorVersion=StrToInt(IEVersion);
				}
				else if(RegQueryValueEx(hKeyLocal,L"Version",0,NULL,(LPBYTE)&IEVersion,&size)==ERROR_SUCCESS)
				{
					g_IE_MajorVersion=StrToInt(IEVersion);
				}
				RegCloseKey(hKeyLocal);
			}
		}

		// Load Quero settings and update version
		SyncSettings();

		// Set visible buttons
		m_ButtonBar.SetVisibleButtons(g_Buttons);

		ReleaseMutex(hQSharedDataMutex);
	}

	if(bCloseQueroSharedDataMutex) CloseHandle(hQSharedDataMutex);

	// Set colors
	hHighlightBrush=NULL;
	SetColorScheme(g_FontColor,false);
	
	// Set the font and calculate the toolbar height and dimensions
	InitFontAndHeight();

	// Create the background brush
	hDefaultBackground=CreateSolidBrush(Colors[COLOR_Background]);

	// Set toolbar pointers
	m_ComboQuero.SetToolbar(this);
	m_ComboEngine.SetToolbar(this);
	m_ButtonBar.SetToolbar(this);
}

HKEY CQToolbar::OpenQueroKey(HKEY hKeyRoot,TCHAR *pSubKey,bool bCreateKey)
{
	HKEY hKeyQuero;
	LONG result;
	TCHAR *pKey;
	TCHAR Key[MAX_PATH];

	#ifdef COMPILE_FOR_WINDOWS_VISTA
		if(hKeyRoot!=HKEY_USERS)
		{
			if(IsWindows8OrLater())
			{
				// Store Quero Registry data under InternetRegistry on Windows 8 and later for Enhanced Protected Mode compatibility
				pKey=L"Software\\Microsoft\\Internet Explorer\\InternetRegistry\\Software\\Quero Toolbar";
			}
			else
			{
				// Store Quero Registry data under AppDataLow on Windows 7 and Windows Vista
				pKey=L"Software\\AppDataLow\\Software\\Quero Toolbar";
			}
		}
		else // hKeyRoot == HKEY_USERS
		{
			pKey=L".DEFAULT\\Software\\Quero Toolbar";
		}
	#else
		if(hKeyRoot!=HKEY_USERS)
		{
			pKey=L"Software\\Quero Toolbar";
		}
		else
		{
			pKey=L".DEFAULT\\Software\\Quero Toolbar";
		}
	#endif

	if(pSubKey)
	{
		StringCbPrintf(Key,sizeof Key,L"%s\\%s",pKey,pSubKey);
		pKey=Key;
	}

	if(bCreateKey)
	{
		result=RegCreateKeyEx(hKeyRoot, pKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyQuero, NULL);
	}
	else
	{
		result=RegOpenKeyEx(hKeyRoot, pKey, 0, KEY_READ, &hKeyQuero);
	}		

	if(result!=ERROR_SUCCESS) hKeyQuero=NULL;

	return hKeyQuero;
}

void CQToolbar::SyncSettings()
{
	UINT i;
	LONG result;
	HKEY hKey;
	DWORD dword,type,size;	
	DWORD EnumIndex;
	DWORD cValueName;
	TCHAR ValueName[32];
	TCHAR data[MAX_PATH];
	UINT VersionFlags;
	bool bVersionLoaded;

	VersionFlags=0;

	// Load the settings from the registry

	hKey=OpenQueroKey(HKEY_CURRENT_USER,NULL,true);
	if(hKey==NULL) hKey=OpenQueroKey(HKEY_CURRENT_USER,NULL,false); // Try opening the registry in read-only mode

	if(hKey)
	{
		bVersionLoaded=false;
		EnumIndex=0;
		do
		{
			cValueName=32;
			size=sizeof data;

			result=RegEnumValue(hKey,EnumIndex,ValueName,&cValueName,NULL,&type,(LPBYTE)data,&size);

			if(result==ERROR_SUCCESS)
			{
				if(type==REG_DWORD) dword=*(DWORD*)data;
				else dword=0;

				i=0;
				while(i<SETTINGS_VALUES_COUNT && StrCmpI(ValueName,SETTINGS_VALUES[i])) i++;

				switch(i)
				{
				case SETTINGS_VALUES_HIGHLIGHT:
					Highlight=(dword!=0);
					break;

				case SETTINGS_VALUES_ZOOMFACTOR:
					if(dword>=ZOOMFACTOR_MIN && dword<=ZOOMFACTOR_MAX) ZoomFactor=dword;
					break;

				case SETTINGS_VALUES_SHOWURL:
					g_ShowURL=(dword!=0);
					break;

				case SETTINGS_VALUES_BLOCKPOPUPS:
					g_BlockPopUps=dword;
					break;

				case SETTINGS_VALUES_FONTSIZE:
					if((int)dword>=-10 && (int)dword<=10) g_FontSize=(int)dword;
					break;

				case SETTINGS_VALUES_FONTCOLOR:
					if(dword<N_COLOR_SCHEMES) g_FontColor=dword;
					break;

				case SETTINGS_VALUES_IDNSUPPORT:
					g_IDNSupport=(dword!=0);
					break;

				case SETTINGS_VALUES_BUTTONS:
					g_Buttons=dword;
					break;

				case SETTINGS_VALUES_WARNINGS:
					g_Warnings=dword;
					break;

				case SETTINGS_VALUES_OPTIONS1:
					g_Options=dword;
					break;

				case SETTINGS_VALUES_OPTIONS2:
					g_Options2=dword;
					break;

				case SETTINGS_VALUES_RESTRICTIONS:
					g_Restrictions=dword;
					break;

				case SETTINGS_VALUES_VERSION:
					if(IsNewProcess && type==REG_BINARY && size==sizeof VersionInfo)
					{
						CopyMemory(&QueroVersion,data,sizeof VersionInfo);
						bVersionLoaded=true;
					}
					break;

				case SETTINGS_VALUES_KEYS:
					g_Keys=dword;
					break;
				}

				EnumIndex++;
			}
		} while(result==ERROR_SUCCESS);

		// Update version
		if(IsNewProcess)
		{
			VersionFlags=UpdateVersion(hKey,&QueroVersion,bVersionLoaded,false);
			bFirstRun=(VersionFlags&VERSION_FLAG_FIRSTRUN);
		}

		RegCloseKey(hKey);
	} // End hKey

	if(VersionFlags&VERSION_FLAG_SYNC_SETTINGS)
	{
		SyncSettings();
	}
}

bool CQToolbar::SaveSettingsValue(UINT ValueId,DWORD dwValue)
{
	return SaveSettingsValueEx(ValueId,REG_DWORD,(LPBYTE)&dwValue,sizeof dwValue);
}

bool CQToolbar::SaveSettingsValueEx(UINT ValueId,DWORD dwType,LPBYTE pData,DWORD cbSize)
{
	bool result=false;
	HKEY hKeyQuero;

	if(ValueId<SETTINGS_VALUES_COUNT)
	{
		hKeyQuero=OpenQueroKey(HKEY_CURRENT_USER,NULL,true);
		if(hKeyQuero)
		{
			if(RegSetValueEx(hKeyQuero,SETTINGS_VALUES[ValueId],0,dwType,pData,cbSize)==ERROR_SUCCESS)
				result=true;
			RegCloseKey(hKeyQuero);
		}
	}

	return result;
}

bool CQToolbar::DeleteSettingsValue(UINT ValueId)
{
	bool result=false;
	HKEY hKeyQuero;

	if(ValueId<SETTINGS_VALUES_COUNT)
	{
		hKeyQuero=OpenQueroKey(HKEY_CURRENT_USER,NULL,true);
		if(hKeyQuero)
		{
			if(RegDeleteValue(hKeyQuero,SETTINGS_VALUES[ValueId])==ERROR_SUCCESS)
				result=true;
			RegCloseKey(hKeyQuero);
		}
	}

	return result;
}

void CQToolbar::InitFontAndHeight()
{
	LOGFONT f;
	TEXTMETRIC tm;
	HDC hDC;
	HGDIOBJ hOldFont;

	// Get screen DC
	hDC=::GetDC(NULL);

	// Init scaling
	if(hDC)
	{
		LogPixelsX=GetDeviceCaps(hDC,LOGPIXELSX);
		LogPixelsY=GetDeviceCaps(hDC,LOGPIXELSY);
	}
	else
	{
		LogPixelsX=96;
		LogPixelsY=96;
	}

	// Get the icon font
	SystemParametersInfo(SPI_GETICONTITLELOGFONT,sizeof(LOGFONT),&f,0);

	//QDEBUG_PRINTF(L"font size",L"%d %d -> %d",f.lfHeight,g_FontSize,CalculatePxHeight(f.lfHeight-g_FontSize));
	
	// Minimum height of toolbar is 22 pixels
	f.lfHeight=CalculateFontHeight(16);
	
	hFont=CreateFontIndirect(&f);

	f.lfWeight=FW_SEMIBOLD;

	hFontBold=CreateFontIndirect(&f);
	
	// The height of the combo box item is derived from the height of the applied font
	if(hDC)
	{
		hOldFont=SelectObject(hDC,hFont);
		GetTextMetrics(hDC,&tm);
		SelectObject(hDC,hOldFont);

		ItemHeight=tm.tmHeight;
		//QDEBUG_PRINTF(L"textmetric",L"%d %d %d",tm.tmHeight,tm.tmInternalLeading,tm.tmExternalLeading);
	}
	else ItemHeight=CalculatePxHeight(f.lfHeight);

	// Make item height even
	if(ItemHeight%2) ItemHeight--;

	// Enforce minimum item height of 18 and set the text padding
	if(ItemHeight<18)
	{
		ItemHeight=18;
		Padding_Top=2;
	}
	else Padding_Top=2;

	// Set margin between items
	Margin_Items=2;

	// DPI Scaling
	if(LogPixelsX!=96 && (g_Options2&OPTION2_DPI_Scaling))
	{
		g_Scaled_NavButtonSize=DPI_SCALEX(g_Unscaled_NavButtonSize);
		g_Scaled_ButtonSize=DPI_SCALEX(g_Unscaled_ButtonSize);
		if(LogPixelsX<144) g_Scaled_IconSize=g_Unscaled_IconSize;
		else g_Scaled_IconSize=DPI_SCALEX(g_Unscaled_IconSize);
		g_Scaled_QueroLogoX=DPI_SCALEX(QUEROLOGOX);
		g_Scaled_QueroLogoY=DPI_SCALEY(QUEROLOGOY);
		g_Scaled_PaddingY=g_Unscaled_PaddingY;
	}
	else
	{
		g_Scaled_NavButtonSize=g_Unscaled_NavButtonSize;
		g_Scaled_ButtonSize=g_Unscaled_ButtonSize;
		g_Scaled_IconSize=g_Unscaled_IconSize;
		g_Scaled_QueroLogoX=QUEROLOGOX;
		g_Scaled_QueroLogoY=QUEROLOGOY;
		g_Scaled_PaddingY=g_Unscaled_PaddingY;
	}

	// Release DC
	if(hDC) ::ReleaseDC(NULL,hDC);
}

CQToolbar::~CQToolbar()
{
	int LastInstanceId;
	int i;

	// Ensure that search animations are stopped
	m_IconAnimation.AbortThread();

	if(WaitForSingleObject(g_hQSharedDataMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		// Remove Quero instance pointers from global array

		if(QueroInstanceId!=UNASSIGNED_INSTANCE_ID) RemoveQueroInstance(QueroInstanceId);

		// Garbage collect the global instances array

		LastInstanceId=UNASSIGNED_INSTANCE_ID;

		for(i=0;i<=g_MaxUsedInstanceId;i++)
			if(QThreadLocalStg[i].hIEWnd!=NULL)
			{
				if(::IsWindow(QThreadLocalStg[i].hIEWnd)) LastInstanceId=i;
				else
				{
					RemoveQueroInstance(i);
					if(g_QueroInstanceCount) g_QueroInstanceCount--;
				}
			}

		if(LastInstanceId!=g_MaxUsedInstanceId) g_MaxUsedInstanceId=LastInstanceId;

		// Decreae Quero instance count
		if(g_QueroInstanceCount) g_QueroInstanceCount--;

		// Last instance closed?
		if(g_QueroInstanceCount==0)
		{
			#ifndef COMPILE_FOR_WINDOWS_VISTA
			// Free default IE frame window icon
			if(g_IE_Icon)
			{
				DestroyIcon(g_IE_Icon);
				g_IE_Icon=NULL;
			}
			#endif

			// Free icons
			for(i=0;i<NICONS;i++)
			{
				if(g_Icons[i])
				{
					DestroyIcon(g_Icons[i]);
					g_Icons[i]=NULL;
				}
			}

			// Free ToolbarIcons
			m_ButtonBar.m_ToolbarIcons.Destroy();

			// Set Icons loaded to false
			g_bIconsLoaded = false;

			// Free Custom Icons
			if(g_QueroTheme_FileName) SysFreeString(g_QueroTheme_FileName);
			if(g_QueroTheme_DLL) FreeLibrary(g_QueroTheme_DLL);

			
			// Free global History and Whitelist
			FreeHistory(g_History,&g_HistoryIndex);
			FreeURLs();
			g_LTimeHistory=0;
			g_LTimeURLs=0;
			g_LTimeWhiteList=0;

			// Close Shared Memory handles
			if(g_QSharedMemory)
			{
				UnmapViewOfFile((LPVOID)g_QSharedMemory);
				g_QSharedMemory=NULL;
				CloseHandle(g_hQSharedMemoryFileMapping);
			}
			if(g_hQSharedMemoryMutex) CloseHandle(g_hQSharedMemoryMutex);
			if(g_hQSharedListMutex) CloseHandle(g_hQSharedListMutex);
		}
		
		ReleaseMutex(g_hQSharedDataMutex);
	}

	if(m_AutoComplete)
	{
		m_AutoComplete->Unbind();
		m_AutoComplete->Release();
	}
	if(hFont) DeleteObject(hFont);
	if(hFontBold) DeleteObject(hFontBold);
	if(hDefaultBackground) DeleteObject(hDefaultBackground);
	if(hHighlightBrush) DeleteObject(hHighlightBrush);
	if(hToolbarBckgrndMemDC) DeleteDC(hToolbarBckgrndMemDC);
	if(hToolbarBckgrndBitmap) DeleteObject(hToolbarBckgrndBitmap);

	// Free LastQueryURL
	FreeLastQueryURL();

	// Free document title
	FreeCurrentDocumentTitle();

	// Free local history
	FreeHistory(History,&HistoryIndex);
	FreeLastHistoryEntry();

	// Free FavIcon
	if(currentFavIcon) DestroyIcon(currentFavIcon);

	// Destroy window
	if(IsWindow()) DestroyWindow();

	// Release the Web browser
	SetBrowser(NULL);

	// Release the Quero Broker
	if(pQueroBroker) pQueroBroker->Release();

}

void CQToolbar::RemoveQueroInstance(int id)
{
	HWND hIEWnd;
	int i;

	hIEWnd=QThreadLocalStg[id].hIEWnd;

	QThreadLocalStg[id].hIEWnd=NULL;
	QThreadLocalStg[id].pToolbar=NULL;
	QThreadLocalStg[id].ThreadId=0;
	QThreadLocalStg[id].ThreadId_IEWnd=0;

	// Unhook the IE tab keyboard hook
	if(QThreadLocalStg[id].hKeyHookTab) UnhookWindowsHookEx(QThreadLocalStg[id].hKeyHookTab);

	// Unsubclass ReBar control
	#ifdef COMPILE_FOR_WINDOWS_VISTA
		// Workaround: set bForce=TRUE to prevent crash when Google Toolbar is loaded, which also subclasses the ReBar
		// ATL 8 SubclassWindow uses unsafe SetWindowLongPtr instead of SetWindowSubclass
		if(m_ReBar.m_hWnd) m_ReBar.UnsubclassWindow(TRUE);
	#endif
	
	if(QThreadLocalStg[id].bNewWindow)
	{
		bool bWindowClosed;

		bWindowClosed=true;

		for(i=0;i<=g_MaxUsedInstanceId;i++)
			if(QThreadLocalStg[i].hIEWnd==hIEWnd)
			{
				if(bWindowClosed)
				{
					bWindowClosed=false;
					QThreadLocalStg[i].bNewWindow=true;
					break;
				}
			}

		// Unsubclass IE window
		if(bWindowClosed)
		{
			#ifndef COMPILE_FOR_WINDOWS_VISTA
			if(ORIG_IEFrame_WndProc) ::SetWindowLongPtr(hIEWnd,GWLP_WNDPROC,(LONG_PTR)ORIG_IEFrame_WndProc);
			#endif

			if(pQueroBroker) pQueroBroker->Unhook_IEFrame(HandleToLong(hIEWnd));
		}
	}
}

int CQToolbar::CalculatePxHeight(int fheight)
{
	return MulDiv(fheight-1,LogPixelsY,-72);
}

int CQToolbar::CalculateFontHeight(int pxheight)
{
	return MulDiv(pxheight,-72,LogPixelsY)+1;
}

/*
LRESULT CQToolbar::OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	return 0;
}
*/

void CQToolbar::CreateDeferred()
{
	int i;
	DWORD style;

	// Load profile list, try to update if empty
	m_Profiles.LoadProfileList();
	if(m_Profiles.ProfileCount==0)
	{
		m_Profiles.UpdateUserProfiles();
		m_Profiles.LoadProfileList();
	}
	CurrentProfileId=m_Profiles.DefaultProfileId;

	// Load main history and initialize LastHistoryEntry
	SyncHistory(true);
	CopyLastHistoryEntry(NULL,true);

	// Create our ComboQuero window and set the font.
	RECT rect = {0,0,0,0};

	style=WS_CHILD|WS_CLIPCHILDREN|CBS_DROPDOWNLIST|WS_VSCROLL|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
	if(g_Options2&OPTION2_ShowSearchBox) style|=WS_VISIBLE;
	m_ComboQuero.Create(m_hWnd, rect, NULL, style, WS_EX_CLIENTEDGE, IDC_COMBOQUERO);
	m_ComboQuero.GetEditCtrl()->SetFont(hFont);

	// Set a WindowProc Wrapper function to handle the WM_NCCALCSIZE in the edit control of the combobox
	//pEditQuero=&m_ComboQuero;
	//oldEditCtlWindowProc=(WNDPROC)::GetWindowLong(m_ComboQuero.m_hWndEdit,GWL_WNDPROC);
	//::SetWindowLong(m_ComboQuero.m_hWndEdit,GWL_WNDPROC,LONG(EditCtlWindowProc));

	m_ComboQuero.SendMessage(CB_SETDROPPEDWIDTH,250,0);
	m_ComboQuero.SendMessage(0x1701,12,0); // For WinXP CB_SETMINVISIBLE
	::SendMessage(m_ComboQuero.m_hWndEdit,EM_LIMITTEXT,MAXURLLENGTH-1,0);

	// Bind the edit control to AutoComplete
	m_AutoComplete=new CAutoComplete();
	if(m_AutoComplete)
	{
		m_AutoComplete->SetToolbar(this);
		m_AutoComplete->Bind(m_ComboQuero.m_hWndEdit,ACO_AUTOSUGGEST|ACO_FILTERPREFIXES|ACO_USETAB);
	}

	// Create the Search Engine's combo box
	style=WS_CHILD|CBS_DROPDOWNLIST|WS_VSCROLL|CBS_OWNERDRAWVARIABLE|CBS_HASSTRINGS|CBS_NOINTEGRALHEIGHT;
	if(g_Options&OPTION_ShowSearchEngineComboBox) style|=WS_VISIBLE;
	m_ComboEngine.Create(m_hWnd, rect, NULL, style, WS_EX_CLIENTEDGE, IDC_COMBOENGINE);
	m_ComboEngine.SetFont(hFont);

	// Initialize the search engines list
	int CurrentEngineId;
	if(IsNewProcess)
	{
		 // Select the default search engine
		CurrentEngineId=-1;
	}
	else
	{
		// Select the last used profile and search engine
		HistoryEntry *lastSearch;

		lastSearch=GetLastHistoryEntry();
		if(lastSearch && IsMoreRecent_Than(lastSearch->Timestamp,g_QueroStartTime))
		{
			CurrentProfileId=lastSearch->ProfileId;
			CurrentEngineId=lastSearch->EngineId;
		}
		else CurrentEngineId=-1;
	}
	SelectProfile(CurrentProfileId,CurrentEngineId);
		
	// Set the combobox width
	IdealEngineWidth=MeasureEngineWidth();
	m_ComboEngine.SendMessage(CB_SETDROPPEDWIDTH,IdealEngineWidth-5,0);
	//m_ComboEngine.SendMessage(0x1701,25,0); // For WinXP CB_SETMINVISIBLE

	// Add a reference of the current toolbar to the IE HWND Local Storage
	if(WaitForSingleObject(g_hQSharedDataMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		// Load Icons
		if(g_bIconsLoaded==false)
		{
			const WORD IconResIds[NICONS]={IDI_SEARCH,IDI_SEARCH_NOTFOUND,IDI_URL,IDI_BIN,IDI_IDN,IDI_123,IDI_LOCK};
			
			i=0;
			while(i<NICONS)
			{
				g_Icons[i]=CToolbarIcons::LoadThemeIcon(IconResIds[i],g_Scaled_IconSize,g_Scaled_IconSize);
				i++;
			}

			m_ButtonBar.LoadToolbarIcons();

			g_bIconsLoaded=true;
		}

		DWORD DEFAULT_TOOLBAR_STYLE = 
			/*Window styles:*/ WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS |
			/*Toolbar styles:*/ TBSTYLE_TOOLTIPS | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_LIST |
					/* TBSTYLE_WRAPABLE | */
			/*Common Control styles:*/ CCS_TOP | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE ;

		// Create the Navigation Bar

		// Create the Button Bar
		m_ButtonBar.Create(m_hWnd, rect, NULL, DEFAULT_TOOLBAR_STYLE);

		// Create the Logo Toolbar
		
		// Subclass combo box drop-down windows
		m_ComboQuero.SubclassListWnd();
		m_ComboEngine.SubclassListWnd();

		// Add a reference of the current toolbar to the IE HWND Local Storage
		// and connect to the Quero Broker
		for(i=0;i<MAX_QUERO_INSTANCES;i++)
		{
			if(QThreadLocalStg[i].pToolbar==NULL)
			{
				if(i>g_MaxUsedInstanceId) g_MaxUsedInstanceId=i;
				QueroInstanceId=i;

				QThreadLocalStg[i].pToolbar=this;
				if(m_pBrowser) m_pBrowser->get_HWND((SHANDLE_PTR*)(&QThreadLocalStg[i].hIEWnd));
				else QThreadLocalStg[i].hIEWnd=NULL;
				
				QThreadLocalStg[i].ThreadId=GetCurrentThreadId();

				// Install the keyboard hook for the current thread
				QThreadLocalStg[i].hKeyHookTab=SetWindowsHookEx(WH_KEYBOARD,KeyboardHookTab,NULL,QThreadLocalStg[i].ThreadId);

				// Is new IE window?
				bool IsNewWindow=true;

				i=0;
				while(i<=g_MaxUsedInstanceId)
				{
					if(QThreadLocalStg[i].hIEWnd==QThreadLocalStg[QueroInstanceId].hIEWnd && i!=QueroInstanceId)
					{
						IsNewWindow=false;
						break;
					}
					i++;
				}

				if(IsNewWindow)
				{
					// Retrieve the thread id of the IEFrame window (IE7+)
					QThreadLocalStg[QueroInstanceId].ThreadId_IEWnd=::GetWindowThreadProcessId(QThreadLocalStg[QueroInstanceId].hIEWnd,NULL);

					// Hook the IE frame window
					if(g_IE_MajorVersion>=7)
					{
						if(pQueroBroker) pQueroBroker->Hook_IEFrame(HandleToLong(QThreadLocalStg[QueroInstanceId].hIEWnd),HandleToLong(m_hWnd),g_Options,g_Options2,g_IE_MajorVersion);
					}
					#ifndef COMPILE_FOR_WINDOWS_VISTA
					else
					{
						// Subclass IE Frame window (all IE windows of the same process share the same window procedure address)
						if(ORIG_IEFrame_WndProc==NULL) ORIG_IEFrame_WndProc=(WNDPROC)::GetWindowLongPtr(QThreadLocalStg[QueroInstanceId].hIEWnd,GWLP_WNDPROC);
						::SetWindowLongPtr(QThreadLocalStg[QueroInstanceId].hIEWnd,GWLP_WNDPROC,(LONG_PTR)IEFrame_WndProc);

						// Auto Maximize
						if(g_Options2&OPTION2_AutoMaximize) PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_MAXIMIZE);
					}
					#endif

					// Mark this instance as the first instance of a new IE window
					QThreadLocalStg[QueroInstanceId].bNewWindow=true;
				}
				else
				{
					QThreadLocalStg[QueroInstanceId].ThreadId_IEWnd=QThreadLocalStg[i].ThreadId_IEWnd;
					QThreadLocalStg[QueroInstanceId].bNewWindow=false;
				}

				break;
			}
		}
		ReleaseMutex(g_hQSharedDataMutex);
	}

	// Initialize the Quero edit control
	if(g_ShowURL==false)
	{
		if(IsNewProcess || (g_Options2&OPTION2_ShowSearchTermsWhileSearching)==0)
		{
			m_ComboQuero.SetTextPrompt();
		}
		else
		{
			HistoryEntry *lastSearch=GetLastHistoryEntry();
			if(lastSearch && lastSearch->Type==TYPE_SEARCH && IsMoreRecent_Than(lastSearch->Timestamp,g_QueroStartTime))
			{
				m_ComboQuero.SetText(lastSearch->Query,TYPE_SEARCH,NULL,false);
			}
			else
			{
				m_ComboQuero.SetTextPrompt();
			}
		}
	}

	// Subclass the ReBar control
	#ifdef COMPILE_FOR_WINDOWS_VISTA
	HWND hwnd_ReBar;
	hwnd_ReBar=GetParent();
	if(hwnd_ReBar) m_ReBar.SubclassWindow(hwnd_ReBar);
	#endif

	// Set bToolbarCreated
	bToolbarCreated=true;
}

int CQToolbar::GetToolbarHeight()
{
	int height;

	if((g_Options2&OPTION2_ShowSearchBox) || (g_Options&OPTION_ShowSearchEngineComboBox) ||
	 m_ButtonBar.HasVisibleButtons())	
		height=ItemHeight+2+GetToolbarPadding()*2;
	else
		height=0;

	if(IsBelowWindows8() && g_IE_MajorVersion>=9 && (g_Options2&OPTION2_EnableAeroTheme)) height+=2;

	//QDEBUG_PRINTF(L"toolbar height",L"%d",height);

	return height;
}

// Get padding between search box and toolbar edges
int CQToolbar::GetToolbarPadding()
{
	int padding;

	int buttonheight=0;

	buttonheight=g_Scaled_NavButtonSize;
	if(m_ButtonBar.HasVisibleButtons() && g_Scaled_ButtonSize>buttonheight) buttonheight=g_Scaled_ButtonSize;

	if(buttonheight>20)
	{
		padding=(buttonheight-g_Scaled_ButtonSize+1)/(g_Scaled_PaddingY==PADDINGY_UNKNOWN?3:g_Scaled_PaddingY); // Minimum search box height is 22px
	}
	else if(g_Scaled_PaddingY==PADDINGY_UNKNOWN)
	{
		padding=g_IE_MajorVersion>=7?2:0;
	}
	else padding=g_Scaled_PaddingY;

	if(IsWindows8OrLater()) padding--;
	else if(g_IE_MajorVersion>=9 && (g_Options2&OPTION2_EnableAeroTheme)) padding-=5;

	if(padding<0) padding=0;
		
	//QDEBUG_PRINTF(L"padding",L"%d",padding);

	return padding;
}

LRESULT CQToolbar::OnWindowPosChanging(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	// Remove toolbar close button in IE8
	if(g_IE_MajorVersion>=8 && (g_Options2&OPTION2_HideToolbarCloseButton))
	{
		WINDOWPOS *pWindowPos=(WINDOWPOS*)lParam;
		HWND hwnd_ReBar;
		RECT rect_borders;
		LRESULT index;
		int expand_left;

		#ifdef COMPILE_FOR_WINDOWS_VISTA
			hwnd_ReBar=m_ReBar.m_hWnd;
		#else
			hwnd_ReBar=GetParent();
		#endif

		// Check whether toolbars are locked or unlocked
		index=::SendMessage(hwnd_ReBar,RB_IDTOINDEX,(WPARAM)m_pBand->GetBandID(),0);
		if(index!=-1)
		{
			::SendMessage(hwnd_ReBar,RB_GETBANDBORDERS,index,(LPARAM)&rect_borders);
			// QDEBUG_PRINTF(L"rect_borders.left",L"%d,%f %d %d %d",rect_borders.left,ScaleX,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CXSIZE),GetSystemMetrics(SM_CXSMSIZE));
			// IE7 borders Vista: 4 locked, 13 unlocked
			// IE7 borders Vista Classic: 0 locked, 9 unlocked
			// IE7 borders XP: 0 locked, 10 unlocked
			// IE7 borders XP Classic: 0 locked, 9 unlocked
			// IE8 borders XP: 26 locked, 31-32 unlocked
			// IE8 XP scale 1.25: 36 locked, 42 unlocked
			// IE8 borders Vista: 30 locked, 35 unlocked
			if(rect_borders.left>=26)
			{
				if(LogPixelsX==96)
				{
					if(rect_borders.left<=30) expand_left=26;
					else expand_left=22;
				}
				else
				{
					expand_left=(rect_borders.left)-4;
				}
				if(pWindowPos->x >= expand_left)
				{
					pWindowPos->x-=expand_left;
					pWindowPos->cx+=expand_left;
				}
			}
		}
	} // End IE8 && OPTION2_HideToolbarCloseButton

	return 0;
}

LRESULT CQToolbar::OnSize(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	RECT wndRect;
	LONG end;
	LONG newEngineWidth,buttonwidth;
	LONG toolbarwidth;
	LONG toolbarheight;
	LONG delta;

	// Update Toolbar Background Bitmap
	//InterlockedIncrement(&g_ToolbarBackgroundState);

	// Get the size of the toolbar
	GetClientRect(&wndRect);
	toolbarwidth=wndRect.right;
	toolbarheight=wndRect.bottom;

	if(toolbarheight && bToolbarCreated)
	{
		// Calculate rightmost edge of search and engine combo box
		if(g_Options2&OPTION2_ShowSearchBox)
		{
			if(m_ButtonBar.HasVisibleButtons()) wndRect.right -= m_ButtonBar.GetSize().cx;
		}
		else
		{
			wndRect.right=wndRect.left;

			if(g_Options&OPTION_ShowSearchEngineComboBox)
			{
				wndRect.right+=LOGOGAP+IdealEngineWidth;
			}
		}

		// Put a small gap between the combo box and the button bar
		if(m_ButtonBar.HasVisibleButtons()) end=wndRect.right;
		else end=wndRect.right;

		// Add padding, if navigation buttons are present
		wndRect.top=0;

		// Calculate the dimensions of the Quero combo box
		wndRect.left=0;
		if((g_Options2&OPTION2_ShowSearchBox) || (g_Options&OPTION_ShowSearchEngineComboBox)) wndRect.left=0;
		if(g_Options2&OPTION2_ShowSearchBox)
		{	
			if(g_Options&OPTION_ShowSearchEngineComboBox)
			{
				wndRect.right = end - IdealEngineWidth;
				delta=toolbarwidth - GetToolbarMinWidth() - 2*IdealEngineWidth;
				// Shrink Quero combo box and Engine combo box evenly
				if(delta < 0) wndRect.right-=delta/2;

				// Enforce minimum engine combo box width
				newEngineWidth=end-wndRect.right;
				buttonwidth=(m_ComboEngine.m_rcButton.right-m_ComboEngine.m_rcButton.left)+10;
				if(newEngineWidth < buttonwidth) wndRect.right+= newEngineWidth-buttonwidth;
			}
			else wndRect.right=end;
		}
		else wndRect.right=wndRect.left;
		wndRect.bottom -= 3;
		// Set the dimensions of the Quero combo box
		m_ComboQuero.MoveWindow(&wndRect,TRUE);

		// Set the dimensions of the embedded edit control
		UpdateEmbedButtons(true,true);

		 // Set the position of the engine's combo box
		if(g_Options2&OPTION2_ShowSearchBox)
		{
			if(g_Options&OPTION_ShowSearchEngineComboBox)
			{
				wndRect.left=wndRect.right+4; // Add gap between search box and engine's box
				wndRect.right=end;
			}
			else wndRect.left=wndRect.right=end-IdealEngineWidth;
		}
		else if(g_Options&OPTION_ShowSearchEngineComboBox)
		{
			wndRect.right=end;
		}
		m_ComboEngine.MoveWindow(&wndRect,TRUE);
		::SendMessage(m_ComboEngine.m_hWnd, CB_SETITEMHEIGHT, -1, 18);
		// Update the dimensions of the Engine combo box
		m_ComboEngine.UpdateComboBoxInfo();
		
		// Set the height of the drop-down list
		CurrentEngineWidth=wndRect.right-wndRect.left;
		SizeComboEngineList();

		//::SendMessage(m_hWnd, TB_SETROWS, MAKEWPARAM(1, FALSE), (LPARAM)&btnRect);
		//::SendMessage(m_hWnd, TB_AUTOSIZE, 0,0);
	}

	return 0;
}

void CQToolbar::SizeComboEngineList()
{
	WORD height;
	WORD n;

	if(chooseProfile)
	{
		n=m_Profiles.ProfileCount?m_Profiles.ProfileCount:1;
		height=(ItemHeight+Margin_Items)*n+2;
	}
	else
	{
		n=nengines?nengines:1;
		height=(ItemHeight+Margin_Items)*(n+1)+(SEPARATOR_HEIGHT*nseparators)+4;
	}
	::MoveWindow(m_ComboEngine.m_hWndList,0,0,CurrentEngineWidth,height,FALSE);
}

LRESULT CQToolbar::OnMove(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	// Update Toolbar Background Bitmap
	InterlockedIncrement(&g_ToolbarBackgroundState);

	return 0;
}

HDC CQToolbar::GetToolbarBckgrndMemDC()
{
	RECT rect;
	HDC hDC;

	if(ToolbarBackgroundState!=g_ToolbarBackgroundState)
	{
		// Update Toolbar Background Bitmap	
		hDC=GetDC();
		if(hDC)
		{
			if(hToolbarBckgrndMemDC==NULL) hToolbarBckgrndMemDC=CreateCompatibleDC(hDC);
			if(hToolbarBckgrndMemDC)
			{
				if(hToolbarBckgrndBitmap)
				{
					DeleteObject(hToolbarBckgrndBitmap);
					hToolbarBckgrndBitmap=NULL;
				}

				GetClientRect(&rect);
				hToolbarBckgrndBitmap=CreateCompatibleBitmap(hDC,rect.right,rect.bottom);
				SelectObject(hToolbarBckgrndMemDC,hToolbarBckgrndBitmap);

				if(hToolbarBckgrndBitmap)
				{
					ToolbarBackgroundState=g_ToolbarBackgroundState;
					::SendMessage(m_hWnd,IsBelowWindowsXP()?WM_ERASEBKGND:WM_PRINTCLIENT,(WPARAM)hToolbarBckgrndMemDC,PRF_ERASEBKGND); // WM_PAINT
				}
			} // End hToolbarBckgrndMemDC
			ReleaseDC(hDC);
		} // End GetDC
	} // End background changed

	return hToolbarBckgrndMemDC;
}

void CQToolbar::SetBrowser(IWebBrowser2* pBrowser)
{
   m_pBrowser = pBrowser;
}

void CQToolbar::SelectEngine(int EngineIndex,bool bForceUpdate,bool bSetCurSel,bool bRedraw)
{
	if(EngineIndex>=0 && (UINT)EngineIndex<nengines)
	{
		if(CurrentEngineIndex!=EngineIndex || bForceUpdate)
		{
			CurrentEngineIndex=EngineIndex;
			if(chooseProfile==false && bSetCurSel) ::SendMessage(m_ComboEngine.m_hWnd,CB_SETCURSEL,EngineIndex+1,0);
			if(bRedraw && (g_Options2&OPTION2_ShowSearchBox) && (g_Options&OPTION_ShowSearchEngineComboBox)==0) m_ComboQuero.RedrawWindow(); // Update embedded button
		}
	}
}

void CQToolbar::SelectNextPrevEngine(bool bNext)
{
	int NewEngineIndex;

	NewEngineIndex=CurrentEngineIndex;
	if(bNext) NewEngineIndex++;
	else NewEngineIndex--;

	if(NewEngineIndex>=(int)nengines) NewEngineIndex=0;
	else if(NewEngineIndex<0) NewEngineIndex=nengines-1;

	SelectEngine(NewEngineIndex);

	if((g_Options2&OPTION2_ShowSearchBox) && (g_Options&OPTION_ShowSearchEngineComboBox)==0)
	{
		m_ComboQuero.ShowToolTip(0,true);
		SetTimer(ID_COMBOQUERO_TOOLTIP_TIMER,TOOLTIP_TIMEOUT);
	}
}

LRESULT CQToolbar::OnNotify(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT result;

	switch(lpnm->code)
	{
	case NM_CUSTOMDRAW:
		// Draw background of embedded toolbars: lpnmTBCustomDraw->nmcd.dwDrawStage==CDDS_PREPAINT (or CDDS_PREERASE if TBSTATE_CUSTOMERASE enabled)
		HDC hMemDC;
		RECT rect;

		#ifdef COMPILE_FOR_WINDOWS_VISTA
		if(m_ReBar.bIsQueroBackgroundTransparent)
		{
			FillRect(lpnmTBCustomDraw->nmcd.hdc,&lpnmTBCustomDraw->nmcd.rc,(HBRUSH)GetStockObject(BLACK_BRUSH));
			result=0;
			break;
		}
		#endif

		hMemDC=GetToolbarBckgrndMemDC();
		if(hMemDC)
		{
			::GetClientRect(lpnm->hwndFrom,&rect);
			::MapWindowPoints(lpnm->hwndFrom,m_hWnd,(LPPOINT)&rect,2);

			BitBlt(lpnmTBCustomDraw->nmcd.hdc,0,0,lpnmTBCustomDraw->nmcd.rc.right,lpnmTBCustomDraw->nmcd.rc.bottom,hMemDC,rect.left,rect.top,SRCCOPY);
		}

		result=CDRF_DODEFAULT;
		break;

	case TTN_NEEDTEXT:
		BSTR bstrQuery;
		UINT HintId;

		HintId=0;
		switch(lpdi->hdr.idFrom)
		{
		case IDM_QUERO:
			if(m_ComboQuero.GetText(bstrQuery))
			{
				trim(bstrQuery);

				if(bstrQuery[0] && !m_ComboQuero.bIsEmptySearch) // Search box not empty
				{
					StringCbPrintf(Tooltip,sizeof Tooltip,GetString(currentType==TYPE_ADDRESS?IDS_HINT_GOTOURL:IDS_HINT_QUERY),bstrQuery);
				}
				else // Go to Engine's Homepage
				{
					BSTR pName=GetEngineName();
					StringCbPrintf(Tooltip,sizeof Tooltip,GetString(IDS_HINT_GOTOENGINE),pName?pName:L"");
				}

				lpdi->lpszText=Tooltip;
			
				SysFreeString(bstrQuery);
			}
			break;
		
		case IDM_REFRESH:
			HintId=IDS_HINT_REFRESH;
			break;
		case IDM_STOP:
			HintId=IDS_HINT_STOP;
			break;
		case IDM_HOME:
			HintId=IDS_HINT_HOME;
			break;
		default:
			lpdi->lpszText=NULL;
		}
		if(HintId) lpdi->lpszText=GetString(HintId);
		lpdi->hinst=NULL;

		result=0;
		break;
	// End TTN_NEEDTEXT
	
	default:
		bHandled=FALSE;
		result=-1;
	}

	return result;
}

void CQToolbar::SetAutoMaximize(bool bAutoMaximize)
{
	if(bAutoMaximize) g_Options2|=OPTION2_AutoMaximize;
	else g_Options2&=~OPTION2_AutoMaximize;

	// Save highlight setting in registry
	SaveSettingsValue(SETTINGS_VALUES_OPTIONS2,g_Options2);

	if(pQueroBroker) pQueroBroker->SetOptions(g_Options,g_Options2,UPDATE_AUTO_MAXIMIZE|UPDATE_SYNC_SETTINGS);
	else ::ShowWindow(GetIEFrameWindow(),SW_MAXIMIZE);
}

LRESULT CQToolbar::OnShowResizeWindow(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HWND hwnd,parenthwnd;
	RECT rect;
	int max_width,max_height;
	int current_width,current_height;

	current_width=0;
	current_height=0;

	hwnd=GetIEFrameWindow();
	parenthwnd=::GetParent(hwnd); // Workaround: Live Toolbar puts a wrapper window around the IEFrame window when tabbed browsing is active
	if(parenthwnd) hwnd=parenthwnd;
	::GetWindowRect(hwnd,&rect);
	current_width=rect.right-rect.left;
	current_height=rect.bottom-rect.top;

#ifdef COMPILE_FOR_WIN9X
	max_width=GetSystemMetrics(SM_CXVIRTUALSCREEN);
	max_height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
#else
	max_width=GetSystemMetrics(SM_CXSCREEN);
	max_height=GetSystemMetrics(SM_CYSCREEN);
#endif

	return 0;
}

void CQToolbar::ResizeWindow(int x,int y,int width,int height,UINT flags)
{
	HWND hwnd,parenthwnd;
	UINT swpflags;

	hwnd=GetIEFrameWindow();
	parenthwnd=::GetParent(hwnd); // Workaround: Live Toolbar puts a wrapper window around the IEFrame window when tabbed browsing is active
	if(parenthwnd) hwnd=parenthwnd;

	if(flags&RESIZEWINDOW_FULLSCREEN)
	{
#ifdef COMPILE_FOR_WIN9X
		RECT workarea;

		SystemParametersInfo(SPI_GETWORKAREA,0,&workarea,0);

		x=workarea.left;
		y=workarea.top;
		width=workarea.right-workarea.left;
		height=workarea.bottom-workarea.top;
#else
		HMONITOR hMonitor;
		MONITORINFO MonitorInfo;

		hMonitor=MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST);
		MonitorInfo.cbSize=sizeof MONITORINFO;
		if(GetMonitorInfo(hMonitor,&MonitorInfo)==TRUE)
		{
			x=MonitorInfo.rcWork.left;
			y=MonitorInfo.rcWork.top;
			width=MonitorInfo.rcWork.right-x;
			height=MonitorInfo.rcWork.bottom-y;
		}
#endif
	}

	if(flags&RESIZEWINDOW_MOVE) swpflags=SWP_NOOWNERZORDER|SWP_NOZORDER;
	else swpflags=SWP_NOOWNERZORDER|SWP_NOZORDER|SWP_NOMOVE;

	if(pQueroBroker)
	{
		pQueroBroker->SetWindowPos(HandleToLong(hwnd),NULL,x,y,width,height,swpflags);
	}
	else
	{
		::ShowWindow(hwnd,SW_RESTORE);
		::SetWindowPos(hwnd,NULL,x,y,width,height,swpflags);
	}
}

void CQToolbar::UpdateQueroInstances(UINT update)
{
	int i;

	if(WaitForSingleObject(g_hQSharedDataMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		for(i=0;i<=g_MaxUsedInstanceId;i++)
			if(QThreadLocalStg[i].hIEWnd!=NULL)
			{
				QThreadLocalStg[i].pToolbar->UpdateQueroInstance(update);
			}
		
		ReleaseMutex(g_hQSharedDataMutex);
	}
}

void CQToolbar::UpdateQueroInstance(UINT update)
{
	BOOL bHandled;

	if(update&UPDATE_SYNC_SETTINGS)
	{
		SyncSettings();
	}

	if(update&UPDATE_BUTTONS)
	{
		m_ButtonBar.ShowButtons(g_Buttons);
	}

	if(update&UPDATE_FONTCOLOR)
	{
		SetColorScheme(g_FontColor,true);
	}

	if(update&(UPDATE_BUTTONS|UPDATE_FONTSIZE|UPDATE_LAYOUT)) OnSysColorChange(WM_SYSCOLORCHANGE,0,0,bHandled);

	if(update&UPDATE_LAYOUT)
	{
		::PostMessage(GetParent(),WM_SIZE,0,0); // Reposition toolbar (hide/show toolbar close button)
		::PostMessage(m_hWnd,WM_SIZE,0,0);
		::ShowWindow(m_ComboQuero.m_hWnd,(g_Options2&OPTION2_ShowSearchBox)?SW_SHOW:SW_HIDE);
		::ShowWindow(m_ComboEngine.m_hWnd,(g_Options&OPTION_ShowSearchEngineComboBox)?SW_SHOW:SW_HIDE);
		RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ERASE);
		m_ComboQuero.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
	}

	if(update&UPDATE_BUTTONS)
	{
		RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ERASE);
		m_ButtonBar.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_ERASE);
	}

	if(update&UPDATE_SHOWURL)
	{
		if(g_ShowURL && currentURL[0])
		{
			m_ComboQuero.SetTextCurrentURL();
		}
		else 
		{
			m_ComboQuero.SetTextPrompt();
		}
		
		m_pBand->FocusChange(FALSE);

		UpdateEmbedButtons(false,true);
		m_ComboQuero.Redraw(true);
	}

	if(update&UPDATE_FREELASTHISTORYENTRY) FreeLastHistoryEntry();

	if(update&UPDATE_QUERO_CONTEXT_MENU)
	{
		m_pBand->InstallContextMenu((g_Options2&OPTION2_EnableQueroContextMenu)!=0);
	}

	#ifdef COMPILE_FOR_WINDOWS_VISTA
	if(update&UPDATE_AEROTHEME)
	{
		if(IsActive) m_ReBar.OnEnableAeroThemeChanged();
	}
	#endif
}

LRESULT CQToolbar::OnCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled=FALSE;

	if(HIWORD(wParam)==0)
	// Toolbar Command
	{
		if(m_pBrowser)
		{
			switch(LOWORD(wParam))
			{
			case IDM_REFRESH:
				m_pBrowser->Refresh();
				bHandled=TRUE;
				break;
			case IDM_STOP:
				m_pBrowser->Stop();
				bHandled=TRUE;
				break;
			case IDM_HOME:
				m_pBrowser->GoHome();
				bHandled=TRUE;
				break;
			case IDM_QUERO:
				m_pBand->FocusChange(FALSE);
				Quero();
				bHandled=TRUE;
				break;
			}
		}
	}
	else if((HWND)lParam==m_ComboQuero.m_hWnd)
	// Message target Search Box
	{
		if(HIWORD(wParam)==CBN_SELCHANGE)
		{
			if(!HistoryIndex) m_ComboQuero.SetText(L"",TYPE_SEARCH,NULL,false);
			else if(!HistoryEntrySelected)
			{
				UINT i=(UINT)::SendMessage(m_ComboQuero.m_hWnd,CB_GETCURSEL,0,0);	

				if(i!=CB_ERR && i<HistoryIndex)
				{
					m_ComboQuero.SetText(History[HistoryIndex-i-1].Query,History[HistoryIndex-i-1].Type,NULL,false);
					::SendMessage(m_ComboQuero.m_hWndEdit,EM_SETSEL,0,-1);
				}
				
/*
					m_pBand->FocusChange(FALSE);
					if(!newWindow)
					{
						m_ComboQuero.ignoreChange=true;
						::SetWindowText(m_ComboQuero.m_hWndEdit,History[HistoryIndex-i-1].Query);
					}
					SetCurrentType(History[HistoryIndex-i-1].Type);
					if(History[HistoryIndex-i-1].Flags & FLAG_BROWSEBYNAME)
					{
						ImFeelingLucky=true;
					}
					else if(currentType==TYPE_SEARCH && !History[HistoryIndex-i-1].Flags&FLAG_BROWSEBYNAME)
					{
						// Set the search engine
						if(History[HistoryIndex-i-1].Profile!=profile) SelectProfile(History[HistoryIndex-i-1].Profile);
						SelectEngine(EngineId2Index(History[HistoryIndex-i-1].Engine));
					}
					Quero(History[HistoryIndex-i-1].Query,true,false,false,newWindow);
					*/
			}
			bHandled=TRUE;
		}
		else if(HIWORD(wParam)==CBN_SELENDOK)
		{
			if(!HistoryIndex) m_ComboQuero.SetText(L"",TYPE_SEARCH,NULL,false);
			else 
			{
				UINT i=(UINT)::SendMessage(m_ComboQuero.m_hWnd,CB_GETCURSEL,0,0);
				if(i!=CB_ERR && i<HistoryIndex)
				{
					UINT ShortcutOptions;

					ShortcutOptions=SHORTCUT_OPTION_SEARCHBOX;
					if(g_MiddleClick) ShortcutOptions|=SHORTCUT_OPTION_MIDDLECLICK;
					else if(GetKeyState(VK_RETURN)<0) ShortcutOptions|=SHORTCUT_OPTION_ENTERKEY;

					UINT newWinTab=GetNewWinTabKeyState(ShortcutOptions);
					HistoryEntry *entry;

					entry=History+HistoryIndex-i-1;
					if(newWinTab==OPEN_SameWindow)
					{
						m_ComboQuero.SetText(entry->Query,entry->Type,NULL,false);
						if(entry->Type==TYPE_SEARCH && !(entry->Flags&FLAG_BROWSEBYNAME))
						{
							// Select the search engine
							if(entry->ProfileId!=CurrentProfileId) SelectProfile(entry->ProfileId,entry->EngineId);
							else SelectEngine(m_Profiles.EngineIdToIndex(entry->EngineId));
						}
					}
					if((g_Options&OPTION_NavigateOnHistorySelection) || newWinTab)
					{
						m_pBand->FocusChange(FALSE);
						if(entry->Type!=TYPE_SEARCH)
						{
							entry->ProfileId=CurrentProfileId;
							entry->EngineId=GetEngineId();
						}
						if(entry->Flags&FLAG_BROWSEBYNAME)
						{
							if(entry->ProfileId==CurrentProfileId) entry->EngineId=GetEngineId();
						}
						HistoryEntrySelected=true;

						BYTE options;

						options=(entry->Flags&FLAG_BROWSEBYNAME)?QUERO_IMFEELINGLUCKY:0;
						if((ShortcutOptions&SHORTCUT_OPTION_ENTERKEY) && GetKeyState(VK_CONTROL)<0)
							options^=QUERO_IMFEELINGLUCKY;

						Quero(entry->Query,entry->Type,options,newWinTab,entry->EngineId,entry->ProfileId);
					}
				}
			}
			bHandled=TRUE;
		}
		else if(HIWORD(wParam)==CBN_DROPDOWN)
		{
			if(SyncLocalHistory())
			{
				m_ComboQuero.SendMessage(CB_RESETCONTENT,0,0);
				if(!HistoryIndex) m_ComboQuero.SendMessage(CB_ADDSTRING,0,(LPARAM)GetString(IDS_EMPTY));
				else
				{
					UINT i;

					i=HistoryIndex;
					while(i>0)
					{
						i--;
						m_ComboQuero.SendMessage(CB_ADDSTRING,0,(LPARAM)(History[i].Query?History[i].Query:L""));
					}
				}
			}

			::SendMessage(m_ComboQuero.m_hWndEdit,EM_SETSEL,0,-1);
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			HistoryEntrySelected=false;

			if(GetFocus()!=m_ComboQuero.m_hWndEdit) m_ComboQuero.SetFocus();
		}
	}
	else if((HWND)lParam==m_ComboEngine.m_hWnd)
	// Message target SE drop-down list
	{
		if(HIWORD(wParam)==CBN_DROPDOWN)
		{
			m_ComboEngine.bHighlightSelection=true;
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			#ifndef COMPILE_FOR_WIN9X
			SetTimer(ID_COMBOENGINE_HOVER_TIMER,HOVER_INTERVAL);
			#endif
		}
		else if(HIWORD(wParam)==CBN_SELENDOK)
		{
			int SelectionIndex;
			
			SelectionIndex=(int)::SendMessage(m_ComboEngine.m_hWnd,CB_GETCURSEL,0,0);

			if(chooseProfile)
			{
				chooseProfile=false;
				m_ComboEngine.bHighlightSelection=false;
				SelectProfile(m_Profiles.IndexToProfileId(SelectionIndex));
				m_ComboEngine.PostMessage(CB_SHOWDROPDOWN,TRUE,0);
			}
			else
			{
				// Profile selected
				if(SelectionIndex==0)
				{
					int i;
					int ProfileId;
					TCHAR *pProfileName;

					m_ComboEngine.bHighlightSelection=false;
									
					if(m_ComboEngine.SendMessage(CB_GETDROPPEDSTATE,0,0)) m_ComboEngine.SendMessage(CB_SHOWDROPDOWN,FALSE,0);

					chooseProfile=true;

					m_ComboEngine.SendMessage(CB_RESETCONTENT,0,0);
					
					i=0;
					ProfileId=m_Profiles.First();
					while(ProfileId!=-1)
					{
						pProfileName=m_Profiles.GetProfileName(ProfileId);
						m_ComboEngine.SendMessage(CB_ADDSTRING,0,(LPARAM)(pProfileName?pProfileName:L""));
						m_ComboEngine.SendMessage(CB_SETITEMDATA,i,(LPARAM)ProfileId);
						ProfileId=m_Profiles.Next();
						i++;
					}

					if(i==0) m_ComboEngine.SendMessage(CB_ADDSTRING,0,(LPARAM)GetString(IDS_EMPTY));

					// Set the height of the drop-down list
					SizeComboEngineList();

					// Select the current profile entry
					if(i)
					{
						i=m_Profiles.ProfileIdToIndex(CurrentProfileId);
						if(i==-1) i=0;
						m_ComboEngine.PostMessage(CB_SETCURSEL,i,0);
					}

					// Show the profile list
					m_ComboEngine.PostMessage(CB_SHOWDROPDOWN,TRUE,0);
				}
				// SE selected
				else if(SelectionIndex>0 && (UINT)SelectionIndex<=nengines)
				{
					m_ComboEngine.bHighlightSelection=true;
					SelectEngine(SelectionIndex-1,false,false,true);
			
					// PostMessage to workaround issue when new IE window is opened that CBN_SELENDOK is sent twice
					UINT ShortcutOptions;
					ShortcutOptions=SHORTCUT_OPTION_SEARCHBOX;
					if(g_MiddleClick) ShortcutOptions|=SHORTCUT_OPTION_MIDDLECLICK;
					else if(GetKeyState(VK_RETURN)<0) ShortcutOptions|=SHORTCUT_OPTION_ENTERKEY;
					PostMessage(WM_QUERO,(m_ComboQuero.bIsEmptySearch || currentType==TYPE_ADDRESS)?QUERO_GO2HP|QUERO_CONTEXT_SEARCHENGINE_BOX:QUERO_CONTEXT_SEARCHENGINE_BOX,(LPARAM)ShortcutOptions); // wParam=Quero Options
				}
			}
			bHandled=TRUE;
		}
		else if(HIWORD(wParam)==CBN_SELENDCANCEL)
		{
			if(chooseProfile)
			{
				chooseProfile=false;
				SelectProfile(CurrentProfileId,GetEngineId());
			}
			else if(m_ComboEngine.bHighlightSelection && nengines) ::PostMessage(m_ComboEngine.m_hWnd,CB_SETCURSEL,CurrentEngineIndex+1,0);
			bHandled=TRUE;
		}
	}
	
	return 0;
}

UINT CQToolbar::GetNewWinTabKeyState(UINT ShortcutOptions,UINT DefaultNewWinTab)
{
	bool bCtrlKeyDown=::GetKeyState(VK_CONTROL)<0;
	bool bAltKeyDown=::GetKeyState(VK_MENU)<0;
	bool bShiftKeyDown=::GetKeyState(VK_SHIFT)<0;

	bool bToolbarContext=(ShortcutOptions&(SHORTCUT_OPTION_QUERO_TOOLBAR|SHORTCUT_OPTION_SEARCHBOX))!=0;
	bool bMiddleClick=(ShortcutOptions&SHORTCUT_OPTION_MIDDLECLICK)!=0;
	bool bEnterKey=(ShortcutOptions&SHORTCUT_OPTION_ENTERKEY)!=0;

	UINT newWinTab;

	if(ShortcutOptions&SHORTCUT_OPTION_IGNORE_MODIFIERS)
	{
		newWinTab=OPEN_SameWindow;
	}
	else
	{
		if(bToolbarContext)
		{
			// Context Quero Toolbar or search box
			newWinTab=0;
			if(bShiftKeyDown) newWinTab++;
			if(bMiddleClick || (bEnterKey && bAltKeyDown) || (!bEnterKey && bCtrlKeyDown)) newWinTab+=2;

			// Prevent that the menu bar appears in IE7 if the user opens a search with Alt+Click
			if(bAltKeyDown) g_IgnoreAltKeyUpOnce=true;
		}
		else
		{
			// Context right-click menu
			if(bMiddleClick || bCtrlKeyDown)
			{
				newWinTab=bShiftKeyDown?OPEN_NewTab:OPEN_BackgroundTab;
			}
			else newWinTab=bShiftKeyDown?OPEN_NewWindow:OPEN_SameWindow;
		}
	}

	if(DefaultNewWinTab==OPEN_UNDEFINED)
	{
		if(bToolbarContext) DefaultNewWinTab=OPTION_NewWinTab_SearchBox(g_Options);
		else DefaultNewWinTab=OPTION_NewWinTab_ContextMenu(g_Options);

		// Open in same window if current page is an about URL or navigation failed
		if(DefaultNewWinTab!=OPEN_SameWindow && (NavigationFailed || !StrCmpN(currentURL,L"about:",6)))
			DefaultNewWinTab=OPEN_SameWindow;
	}

	IE6_MapNewTabToNewWin(DefaultNewWinTab);
	IE6_MapNewTabToNewWin(newWinTab);
	
	if(newWinTab==OPEN_SameWindow) newWinTab=DefaultNewWinTab;
	else if(DefaultNewWinTab==newWinTab) newWinTab=OPEN_SameWindow;

	// Map new tab to new window if tabbed browsing is disabled
	if(newWinTab>OPEN_NewWindow && pQueroBroker)
	{
		if(pQueroBroker->IsTabbedBrowsing(HandleToLong(GetIEFrameWindow()))==S_FALSE)
			newWinTab=OPEN_NewWindow;
	}

	return newWinTab;
}

LRESULT CQToolbar::OnGo(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	Quero(NULL,TYPE_UNKNOWN,(BYTE)wParam,GetNewWinTabKeyState((UINT)lParam|SHORTCUT_OPTION_QUERO_TOOLBAR)); // wParam=Quero Options; lParam=Shortcut Options
	return 0;
}

void CQToolbar::Quero(TCHAR *pQuery, BYTE type, BYTE options, UINT newWinTab, int differentEngineId, int differentProfileId)
{
	SearchEngine* pEngine;
	SearchEngine differentEngine;
	int engineid,profileid;
	bool go2hp;
	bool IsNewQuery;

	// Reset LastQueryURL
	IsNewQuery=false;
	FreeLastQueryURL();

	if(newWinTab==OPEN_UNDEFINED) newWinTab=GetNewWinTabKeyState(SHORTCUT_OPTION_SEARCHBOX);

	if(m_pBrowser)
	{
		VARIANT vEmpty,vFlags,vPostData,vHeaders;
		SAFEARRAY sArray;
		BSTR bstrQuery;
		TCHAR *pNewQuery;
		BSTR bstrURL;
		char *pPostData;
		size_t len;
		TCHAR QueryWithAddress[MAXURLLENGTH];

		// Terminate find on page operation
		if(newWinTab==OPEN_SameWindow) SetPhraseNotFound(false);

		// Init variables
		bstrQuery=NULL;
		pNewQuery=NULL;
		bstrURL=NULL;
		pPostData=NULL;

		VariantInit(&vEmpty);
		VariantInit(&vFlags);
		VariantInit(&vPostData);
		VariantInit(&vHeaders);

		m_Profiles.InitEngine(&differentEngine);

		// Determine search engine
		if(differentEngineId!=-1)
		{
			engineid=differentEngineId;
			profileid=(differentProfileId==-1)?CurrentProfileId:differentProfileId;
			if(m_Profiles.GetEngine(profileid,engineid,&differentEngine)) pEngine=&differentEngine;
			else pEngine=NULL;
		}
		else
		{
			engineid=GetEngineId();
			profileid=CurrentProfileId;
			pEngine=GetCurrentEngine();
		}

		// Set type to search for URL based services if query is initiated through the searchengine combo box
		if(pEngine && pEngine->iRequiresAddress && pEngine->bRequiresKeywords==false && (options&QUERO_CONTEXT_SEARCHENGINE_BOX) && type==TYPE_UNKNOWN && m_ComboQuero.bIsEmptySearch==false)
		{
			type=TYPE_SEARCH;
			if(newWinTab==OPEN_SameWindow && currentType!=TYPE_SEARCH)
			{
				m_ComboQuero.bCurrentURLDisplayed=false;
				SetCurrentType(TYPE_SEARCH,NULL);
			}
			go2hp=false;
		}
		else
		{
			go2hp=(options&QUERO_GO2HP)!=0;
		}

		// Determine whether to search or to navigate
		if(!go2hp)
		{
			// Get query string
			if(pQuery) pNewQuery=pQuery;
			else if(m_ComboQuero.GetText(bstrQuery)) pNewQuery=bstrQuery;
			else pNewQuery=NULL;

			if(pNewQuery)
			{
				trim(pNewQuery);
				StrCchLen(pNewQuery,MAXURLLENGTH,len);

				if(len>=MAXURLLENGTH)
				{
					MessageBox(GetString(IDS_ERR_QUERYTOOLONG),_T("Quero Toolbar"),MB_OK|MB_ICONEXCLAMATION);
					pNewQuery[MAXURLLENGTH]=0;
				}

				// Determine type
				if(type==TYPE_UNKNOWN)
				{
						if(len && m_ComboQuero.bIsEmptySearch==false) type=(g_Options2&OPTION2_AddressNavigation)?currentType:TYPE_SEARCH;
						else go2hp=true;
				}

				// Address
				if(type==TYPE_ADDRESS)
				{
					// Prefix "http://" if scheme is omitted and address is an IPv6 address or port number is present
					bstrURL=SysAllocString(pNewQuery);
					type=TYPE_ADDRESS;
					pEngine=NULL;
				}
			} // End pNewQuery
		} // End !go2hp
		
		// Go to search engine's homepage or save to history
		if(go2hp)
		{
			if(pEngine)
			{
				if(pEngine->LinkURL) bstrURL=SysAllocString(pEngine->LinkURL);
				pEngine=NULL;
			}
		}
		else if(pNewQuery) AddToURLHistory(pNewQuery);

		// Build search query
		if(pEngine && pNewQuery)
		{
			m_Profiles.PrepareNavigation(pNewQuery,pEngine,&bstrURL,&pPostData);
			if(pPostData)
			{
				vHeaders.vt=VT_BSTR;
				vHeaders.bstrVal=L"Content-type: application/x-www-form-urlencoded";
				sArray.cDims=1;
				sArray.fFeatures=FADF_FIXEDSIZE;
				sArray.cbElements=1;
				sArray.cLocks=0;
				sArray.pvData=pPostData;
				StrCchLenA(pPostData,MAXURLLENGTH,len);
				sArray.rgsabound[0].cElements=(ULONG)len;
				sArray.rgsabound[0].lLbound=0;
				vPostData.vt=VT_ARRAY|VT_UI1;
				vPostData.parray=&sArray;
			}
		}

		// Navigate
		if(bstrURL)
		{
			if(bstrURL)
			{
				// Have the Web browser navigate to the site URL requested depending on user input
				if(newWinTab)
				{
					vFlags.vt=VT_I4;
					vFlags.intVal=MapNewWinTabToNavOpen(newWinTab);
					if(newWinTab==OPEN_NewWindow) bAllowOnePopUp=true;
					
					//vTarget.vt=VT_BSTR;			// If Target="_BLANK" is specified, IE does not always open a new window.
					//vTarget.bstrVal=L"_BLANK";
				}
				if(options&QUERO_REDIRECT)
				{
					if(vPostData.vt&VT_ARRAY)
					{
						StrCchLenA(pPostData,MAXURLLENGTH,len);
					}
					PostMessage(WM_QUERO_REDIRECTBROWSER);
				}
				else
				{
					m_pBrowser->Navigate(bstrURL,&vFlags,&vEmpty,&vPostData,&vHeaders);
					// Dispaly current URL if new window/tab is opened
					if(newWinTab && g_ShowURL && currentURL[0]) m_ComboQuero.SetTextCurrentURL();
				}

				// Set text
				if(pQuery && (options&QUERO_SETTEXT) && newWinTab==OPEN_SameWindow) m_ComboQuero.SetText(pQuery,type,NULL,false);

				// Save LastQueryURL or free bstrURL
				if(IsNewQuery) LastQueryURL=bstrURL;
				else SysFreeString(bstrURL);
			}
		}

		 // Free memory
		m_Profiles.FreeEngine(&differentEngine);
		if(bstrQuery) SysFreeString(bstrQuery);
		if(pPostData) delete[] pPostData;

	} // End m_pBrowser
}

UINT CQToolbar::SplitIntoWords(TCHAR *str,TCHAR Words[MAXWORDS][MAXWORDLENGTH],BYTE options,UINT MaxWords)
{
	UINT n;

	n=0;

	return n;
}

LRESULT CQToolbar::OnSysColorChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	// Update the fonts and and item height
	if(hFont) DeleteObject(hFont);
	if(hFontBold) DeleteObject(hFontBold);

	InitFontAndHeight();

	// Update the font
	m_ComboQuero.GetEditCtrl()->SetFont(hFont);
	m_ComboEngine.SetFont(hFont);

	// Update the controls on the toolbar
	m_ComboQuero.OnHeightChange(ItemHeight);
	m_ComboEngine.OnHeightChange(ItemHeight);

/*	TCHAR buffer[100];
	_stprintf(buffer,L"%d",height);
	MessageBox(buffer);*/

	// Set the toolbar's height
	HWND hWndReBar = m_pBand->GetParentWindow();
	if(hWndReBar)
	{
		int nCount = (int)::SendMessage(hWndReBar, RB_GETBANDCOUNT, 0, 0L);
		for(int i = 0; i < nCount; i++)
		{
			REBARBANDINFO rbbi = { sizeof(REBARBANDINFO), RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE};
			BOOL bRet = (BOOL)::SendMessage(hWndReBar, RB_GETBANDINFO, i, (LPARAM)&rbbi);
			if(bRet && rbbi.hwndChild == m_hWnd)
			{
				rbbi.cxMinChild = GetToolbarMinWidth();
				if(rbbi.cx < rbbi.cxMinChild) rbbi.cx = rbbi.cxMinChild;
				rbbi.cyMinChild = rbbi.cyChild = GetToolbarHeight();
     			::SendMessage(hWndReBar, RB_SETBANDINFO, i, (LPARAM)&rbbi);
				break;           
			}
		}
	}

	// Update the embedded toolbars heights
	m_ButtonBar.OnHeightChange(ItemHeight);

	// Set the combobox width
	IdealEngineWidth=MeasureEngineWidth();
	OnSize(0,0,0,bHandled);
	m_ComboEngine.SendMessage(CB_SETDROPPEDWIDTH,IdealEngineWidth-5,0);
	SelectProfile(CurrentProfileId,GetEngineId());

	if(m_AutoComplete)
	{
		m_AutoComplete->Unbind();
		m_AutoComplete->Bind(m_ComboQuero.m_hWndEdit,ACO_AUTOSUGGEST|ACO_FILTERPREFIXES|ACO_USETAB);
	}

	// Recalculate domain extents

	MeasureDomainExtents(currentURL,CoreDomainStartIndex,CoreDomainEndIndex,&CoreDomainStartExtent,&CoreDomainEndExtent);
	//QDEBUG_PRINTF(L"MeasureDomainExtents",L"%d,%d :: %d,%d",CoreDomainStartIndex,CoreDomainEndIndex,CoreDomainStartExtent,CoreDomainEndExtent);

	m_ComboQuero.bURLChanged=true;
	m_ComboQuero.GetEditCtrl()->RedrawWindow();

	RedrawWindow();

	bHandled=TRUE;
	return 0;
}

LRESULT CQToolbar::OnMeasureItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if(wParam==IDC_COMBOENGINE)
	{
		if(lpmis->itemID!=-1)
		{
			lpmis->itemHeight=ItemHeight+Margin_Items;

			if(chooseProfile)
			{
				if(lpmis->itemID==(m_Profiles.ProfileCount-1)) lpmis->itemHeight--;
			}
			else
			{
				if(lpmis->itemID==0) lpmis->itemHeight+=SEPARATOR_HEIGHT;
				else
				{
					if(lpmis->itemID-1<nengines)
						if(m_Profiles.CurrentProfile.Engines[lpmis->itemID-1].HasSeparator)
							lpmis->itemHeight+=SEPARATOR_HEIGHT;
				}
			}
		}
		else lpmis->itemHeight=ItemHeight;
	}
	else // ComboQuero
	{
		lpmis->itemHeight=(lpmis->itemID==-1)?ItemHeight:ItemHeight+Margin_Items;
	}

	return TRUE;
}

LRESULT CQToolbar::OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HDC memDC;

	// Prepare buffered drawing

	#ifdef COMPILE_FOR_WINDOWS_VISTA
		CBufferedPaint BufferedPaint;

		if(BufferedPaint.Begin(lpDIS->hDC,&lpDIS->rcItem,BPBF_TOPDOWNDIB,NULL,&memDC)==NULL)
			memDC=NULL;
	#else
		RECT OrigRect;
		HBITMAP hMemBmp,hOldBmp;
		LONG BmpWidth,BmpHeight;

		OrigRect=lpDIS->rcItem;
		BmpWidth=OrigRect.right-OrigRect.left;
		BmpHeight=OrigRect.bottom-OrigRect.top;
		lpDIS->rcItem.top=0;
		lpDIS->rcItem.left=0;
		lpDIS->rcItem.right=BmpWidth;
		lpDIS->rcItem.bottom=BmpHeight;
		memDC=CreateCompatibleDC(lpDIS->hDC);
		hMemBmp=CreateCompatibleBitmap(lpDIS->hDC,BmpWidth,BmpHeight);
		hOldBmp=(HBITMAP)SelectObject(memDC,hMemBmp);
	#endif

	if(memDC)
	{		
		// Clean Bitmap
		
		FillRect(memDC,&lpDIS->rcItem,hDefaultBackground);

		// Start Drawing

		SetBkMode(memDC,TRANSPARENT);

		if(lpDIS->CtlID==IDC_COMBOENGINE)
		{
			if((lpDIS->itemState&(ODS_COMBOBOXEDIT|ODS_SELECTED))==ODS_SELECTED)
			{
				if(LastHighlightedItemIndex!=lpDIS->itemID)
				{
					LastHighlightedItemIndex=lpDIS->itemID;
					Times_LastHighlightedItem_Identical=0;
				}
			}
			DrawItemComboEngine(memDC,lpDIS);
		}
		else
		{
			DrawItemComboQuero(memDC,lpDIS);
		}

		// Draw buffer to screen

		#ifdef COMPILE_FOR_WINDOWS_VISTA
			BufferedPaint.MakeOpaque();
			BufferedPaint.End();
		#else
			BitBlt(lpDIS->hDC,OrigRect.left,OrigRect.top,BmpWidth,BmpHeight,memDC,0,0,SRCCOPY);
			SelectObject(memDC, hOldBmp);
			DeleteObject(hMemBmp);
			DeleteDC(memDC);
		#endif
	}

	return TRUE;
}

void CQToolbar::DrawItemComboEngine(HDC hDC,DRAWITEMSTRUCT *pItem)
{
	bool bDrawEngine=true;
	bool IsEmpty;
	bool bold=false;
	RECT rect;
	HICON hIcon;
	UINT EngineIdx;
	BSTR pName;
	size_t len;
	TCHAR buffer[3];

	if(pItem->itemState & ODS_COMBOBOXEDIT) // Drawing takes place in the selection field
	{
		if(pItem->itemID==0 || chooseProfile)
		{
			rect=pItem->rcItem;
			rect.left+=6;
			rect.top+=Padding_Top;
			SelectObject(hDC,hFont);
			SetTextColor(hDC,Colors[COLOR_Link]);
			pName=m_Profiles.GetProfileName((int)pItem->itemData);
			if(pName==NULL) pName=GetString(IDS_EMPTY);
			DrawText(hDC,pName,-1,&rect,DT_TOP|DT_SINGLELINE|DT_NOPREFIX);

			bDrawEngine=false;
		}
		else if(pItem->itemID==-1)
		{
			EngineIdx=CurrentEngineIndex;
			if(chooseProfile) bDrawEngine=false;
		}
		else
		{
			EngineIdx=pItem->itemID-1;
		}
	}
	// Drawing takes place in the drop-down list
	else if(pItem->itemID==0 || chooseProfile) // Draw Profile
	{
		bool selected=false;

		rect=pItem->rcItem;

		if(!chooseProfile) rect.bottom-=Margin_Items+SEPARATOR_HEIGHT-2;
		if(pItem->itemState & ODS_SELECTED)
		{
			selected=true;
			SetTextColor(hDC,Colors[COLOR_HighlightText]);
			SelectObject(hDC,hFontBold);
		}
		else
		{
			SetTextColor(hDC,Colors[COLOR_Link]);
			SelectObject(hDC,hFont);
		}

		rect.top+=Padding_Top;
		rect.left+=5;
		if(chooseProfile) rect.top--;

		SIZE size_name,size_id;

		int ProfileIdx=m_Profiles.ProfileIdToIndex((int)pItem->itemData);
		if(ProfileIdx!=-1)
		{
			ProfileIdx++;
			if(ProfileIdx>=10)
			{
				buffer[0]=L'0'+ProfileIdx/10;
				buffer[1]=L'0'+ProfileIdx%10;
				buffer[2]=L'\0';
			}
			else
			{
				buffer[0]=L'0'+ProfileIdx;
				buffer[1]=L'\0';
			}
		}
		else buffer[0]=L'\0';

		GetTextExtentPoint32(hDC,L"00",2,&size_id);

		pName=m_Profiles.GetProfileName((int)pItem->itemData);
		if(pName) len=SysStringLen(pName);
		else
		{
			pName=GetString(IDS_EMPTY);
			StrCchLen(pName,MAX_STRING_LENGTH,len);
		}
		rect.right-=size_id.cx+15;
		rect.top+=2;
		DrawText(hDC,pName,(int)len,&rect,DT_TOP|DT_SINGLELINE|DT_NOPREFIX);
		rect.right+=size_id.cx+15;
		GetTextExtentPoint32(hDC,pName,(int)len,&size_name);

		rect.left=rect.right-size_id.cx-14;

		DrawText(hDC,buffer,-1,&rect,DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_CENTER);

		bDrawEngine=false;
	}
	else
	{
		EngineIdx=pItem->itemID-1;

		// Draw separator line

		if(EngineIdx<nengines && m_Profiles.CurrentProfile.Engines[EngineIdx].HasSeparator)
		{
			/*
			HDC bmpDC;
			bmpDC=CreateCompatibleDC(hDC);
			SelectObject(bmpDC,hImgSeparator);
			BitBlt(hDC,pItem->rcItem.left+25,pItem->rcItem.top+height+(Margin/2)+2,(((pItem->rcItem.right-pItem->rcItem.left-3)/5)*5)-25,2,bmpDC,2,0,SRCCOPY);
			DeleteDC(bmpDC);
			pItem->rcItem.bottom-=SEPARATOR_HEIGHT;*/

			/*
			RECT line;
			HBRUSH hBrush;

			line=pItem->rcItem;
			line.top+=ItemHeight+Margin_Items+2;
			line.bottom-=2;
			line.left+=30;

			hBrush=CreateSolidBrush(Colors[COLOR_Separator]);
			if(hBrush)
			{
				::FillRect(hDC,&line,hBrush);
				DeleteObject(hBrush);
			}
			*/
		
			pItem->rcItem.bottom-=SEPARATOR_HEIGHT;
		}
		pItem->rcItem.bottom-=Margin_Items;
	}

	// Draw Engine

	if(bDrawEngine)
	{
		// Get the engine name
		len=0;
		if(EngineIdx<nengines)
		{
			pName=m_Profiles.CurrentProfile.Engines[EngineIdx].Name;
			if(pName) len=SysStringLen(pName);
		}
		if(len==0)
		{
			pName=GetString(IDS_EMPTY);
			StrCchLen(pName,MAX_STRING_LENGTH,len);
			IsEmpty=true;
		}
		else IsEmpty=false;

		// Draw engine icon

		rect=pItem->rcItem;

		if((pItem->itemState & ODS_COMBOBOXEDIT) == 0) rect.left+=4;
		else rect.left+=2;

		if(IsEmpty==false)
		{
			rect.right=rect.left+g_Scaled_IconSize;
			
			if(EngineIdx<nengines)
			{
				hIcon=m_Profiles.GetEngineIcon(&m_Profiles.CurrentProfile.Engines[EngineIdx],g_Icons[ICON_SEARCH]);
			}
			else hIcon=NULL;
			
			DrawItemIcon(hDC,&rect,hIcon,hDefaultBackground,-2,false);
			pItem->rcItem.left=rect.left+g_Scaled_IconSize+4;
		}
		else pItem->rcItem.left=rect.left+8;

		/*if(!(pItem->itemState & ODS_COMBOBOXEDIT))
		{
			RECT r;

			r=pItem->rcItem;
			r.left+=3;
			r.right=r.left+20;
			r.bottom=BmpHeight;

			::FillRect(hDC,&r,hHighlightBrush);

			DrawItemIcon(hDC,&rect,hIcon,hHighlightBrush,-1);
		}
		else DrawItemIcon(hDC,&rect,hIcon,hDefaultBackground,-2);
		pItem->rcItem.left=rect.left+23;*/

		if(pItem->itemState & ODS_SELECTED)
		{
			if((pItem->itemState & ODS_COMBOBOXEDIT))
			{
				if(m_ComboEngine.bHighlightSelection)
				{
					SIZE sz;

					SetTextColor(hDC,GetSysColor(COLOR_HIGHLIGHTTEXT));
					SelectObject(hDC,hFont);

					if(pName)
					{
						GetTextExtentPoint32(hDC,pName,(int)len,&sz);
						rect=pItem->rcItem;
						rect.left+=2;
						if(rect.left+sz.cx<rect.right) rect.right=rect.left+sz.cx;
						rect.top+=Padding_Top;
						rect.bottom--;
						::FillRect(hDC,&rect,GetSysColorBrush(COLOR_HIGHLIGHT));
					}
				}
				else SetTextColor(hDC,Colors[COLOR_Link]);
			}
			else
			{
				SetTextColor(hDC,Colors[COLOR_HighlightText]);
				bold=true;
			}
		}
		else // Not selected
		{
			SetTextColor(hDC,IsEmpty?Colors[COLOR_Link]:(COLORREF)pItem->itemData);
		}

		// Draw engine name

		pItem->rcItem.top+=Padding_Top;
		pItem->rcItem.left+=2;

		if(len)
		{
			UINT format;

			if(bold) SelectObject(hDC,hFontBold);
			else SelectObject(hDC,hFont);

			format=DT_TOP|DT_SINGLELINE|DT_NOPREFIX;
			DrawText(hDC,pName,(int)len,&pItem->rcItem,format);
		}
	} // End DrawEngine
}

void CQToolbar::DrawItemComboQuero(HDC hDC,DRAWITEMSTRUCT *pItem)
{
	RECT rect;
	HICON hIcon;
	size_t len;
	TCHAR buffer[REGKEYLENGTH];

	if(pItem->itemState & ODS_COMBOBOXEDIT) // Drawing takes place in the selection field
	{
		int i;

		rect=pItem->rcItem;
		rect.right=rect.left+g_Scaled_IconSize;

		// Draw state icon

		DrawItemIcon(hDC,&rect,PreviewIDN?g_Icons[ICON_URL]:currentIcon,hDefaultBackground,currentIconOffset,FALSE);

		// Draw inbox icons

		rect=pItem->rcItem;
		rect.right-=EMBEDICONS_MARGIN;

		i=0;
		while(i<nEmbedIcons)
		{
			switch(EmbedButtons[i])
			{
			case EMBEDBUTTON_SEARCHENGINE:
				/*
				if(nengines)
				{
					hIcon=m_Profiles.GetEngineIcon(GetCurrentEngine(),g_Icons[ICON_SEARCH]);
				}				
				else hIcon=g_Icons[ICON_SEARCH];
				*/
				hIcon=g_Icons[ICON_SEARCH];
				break;
			case EMBEDBUTTON_LOCK:
				hIcon=g_Icons[ICON_LOCK];
				break;
			case EMBEDBUTTON_CONTENTBLOCKED:
				hIcon=g_Icons[ICON_CONTENTBLOCKED];
				break;
			case EMBEDBUTTON_IDN:
				hIcon=g_Icons[(SpecialCharsInURL==SPECIALCHARS_IDN)?ICON_IDN:ICON_123];
				break;
			default:
				hIcon=g_Icons[ICON_SEARCH];
			}
			
			rect.left=rect.right-EMBEDICONS_ICONWIDTH; // Space between first embedded icon and drop down button: 3 pixel
			DrawIconEx(hDC,rect.left,rect.top+(rect.bottom-rect.top-g_Scaled_IconSize+1)/2,hIcon,g_Scaled_IconSize,g_Scaled_IconSize,0,hDefaultBackground,DI_IMAGE|DI_MASK);
			rect.right=rect.left-EMBEDICONS_SPACING;

			i++;
		}
	}
	else // Drawing takes place in the drop-down list
	{
		HistoryEntry *pHistEntry;
		SearchEngine SE;
		long description_width;
		RECT description_rect;
		TCHAR *descripton_text;
		bool bDrawDescription=false;
		BYTE type=TYPE_UNKNOWN;

		m_Profiles.InitEngine(&SE);

		rect=pItem->rcItem;

		rect.left+=2;
		rect.right=rect.left+g_Scaled_IconSize;

		if(pItem->itemID<HistoryIndex)
		{
			pHistEntry=History+HistoryIndex-pItem->itemID-1;			
			type=pHistEntry->Type;
			switch(type)
			{
			case TYPE_ADDRESS:
				if(pHistEntry->hIcon)
				{
					hIcon=pHistEntry->hIcon;
				}
				break;
			default:
				hIcon=NULL;
			}
		}
		else hIcon=NULL;

		DrawItemIcon(hDC,&rect,hIcon,hDefaultBackground,0,false);

		if(Margin_Items) pItem->rcItem.top+=Margin_Items>>1;

		if(bDrawDescription) // Draw search engine name or Web page title (not implemented)
		{
			description_width=(pItem->rcItem.right-pItem->rcItem.left)*DESCRIPTION_RELATIVE_WIDTH;

			if(description_width < DESCRIPTION_MIN_WIDTH) description_width=DESCRIPTION_MIN_WIDTH;

			description_rect=pItem->rcItem;
			description_rect.top+=Padding_Top;

			description_rect.left=description_rect.right-description_width;

			if(pItem->itemState & ODS_SELECTED)	SelectObject(hDC,hFontBold);
			else SelectObject(hDC,hFont);
			SetTextColor(hDC,Colors[COLOR_Description]);
			DrawText(hDC,descripton_text,-1,&description_rect,DT_TOP|DT_SINGLELINE|DT_NOPREFIX);

			pItem->rcItem.right-=description_width+DESCRIPTION_SPACING;
		}

		m_Profiles.FreeEngine(&SE);

		// Draw text after icon

		if(pItem->itemState & ODS_SELECTED)
		{
			SetTextColor(hDC,Colors[COLOR_HighlightText]);
			SelectObject(hDC,hFontBold);
		}
		else
		{	if(type==TYPE_ADDRESS) SetTextColor(hDC,Colors[COLOR_Link]);
			else SetTextColor(hDC,Colors[COLOR_Search]);
			SelectObject(hDC,hFont);
		}

		pItem->rcItem.left+=QEDITCTRL_LEFTMARGIN+m_ComboQuero.GetEditCtrlMargin(); // Add left margin of edit control

		pItem->rcItem.top+=Padding_Top;
		pItem->rcItem.left+=2;

		len=m_ComboQuero.SendMessage(CB_GETLBTEXTLEN,pItem->itemID,0);
		if(len!=CB_ERR && len<(REGKEYLENGTH-1))
		{
			m_ComboQuero.SendMessage(CB_GETLBTEXT,pItem->itemID,(LPARAM)buffer);
		}
		else len=0;

		if(len)
		{
			UINT format;

			format=DT_TOP|DT_SINGLELINE|DT_NOPREFIX;
			if(bDrawDescription) format|=DT_END_ELLIPSIS;
			DrawText(hDC,buffer,(int)len,&pItem->rcItem,format);
		}
	}
}

void CQToolbar::UpdateEmbedButtons(bool bForceResizeEditCtrl,bool bRedraw)
{
	RECT rcEditCtrl;
	TOOLINFO ti;
	int old_nEmbedIcons;
	int i;

	// debug
	//secureProtocol=true;
	//SpecialCharsInURL=SPECIALCHARS_IDN;
	//ContentBlocked=true;

	if(bToolbarCreated)
	{
		EmbedButtonCondition EmbedButtonConditions[]={
			{EMBEDBUTTON_SEARCHENGINE,(g_Options&OPTION_ShowSearchEngineComboBox)==0},
			{EMBEDBUTTON_LOCK,SecureLockIcon_Quero && g_ShowURL},
			{EMBEDBUTTON_CONTENTBLOCKED,ContentBlocked || bTemporarilyUnblock},
			{EMBEDBUTTON_IDN,(SpecialCharsInURL>=SPECIALCHARS_IDN || (SpecialCharsInURL==SPECIALCHARS_ASCII && (g_Options&OPTION_DigitAsciiIndicator)!=0)) && g_ShowURL}
		};

		old_nEmbedIcons=nEmbedIcons;

		nEmbedIcons=0;
		i=0;
		while(i<MAXEMBEDBUTTONS)
		{
			if(EmbedButtonConditions[i].Condition)
			{
				EmbedButtons[nEmbedIcons]=EmbedButtonConditions[i].Id;
				nEmbedIcons++;
			}
			i++;
		}

		// Redraw embedded buttons first before shrinking edit box
		if(bRedraw && nEmbedIcons>old_nEmbedIcons)
			m_ComboQuero.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW);

		if(bForceResizeEditCtrl || nEmbedIcons!=old_nEmbedIcons)
		{
			// Update the item dimensions
			m_ComboQuero.UpdateComboBoxInfo();
			rcEditCtrl=m_ComboQuero.m_rcItem;
			ti.rect=m_ComboQuero.m_rcItem;

			// Set the dimensions of the embedded edit control
			rcEditCtrl.top+=Padding_Top;
			rcEditCtrl.left+=QEDITCTRL_LEFTMARGIN;
			if(nEmbedIcons) rcEditCtrl.right-=GetEmbedButtonsTotalWidth();
			if(SecureLockIcon_Quero && Certificate_Organization_Extent)
			{
				if((rcEditCtrl.right-rcEditCtrl.left-Certificate_Organization_Extent)>=MIN_EDIT_BOX_WIDTH)
				{
					rcEditCtrl.left+=Certificate_Organization_Extent+m_ComboQuero.GetEditCtrlMargin()+SPACING_CERTIFICATE_ORGANIZATION_EDITCTRL;
				}
			}
			m_ComboQuero.EditCtrlWidth=rcEditCtrl.right-rcEditCtrl.left;
			m_ComboQuero.GetEditCtrl()->MoveWindow(&rcEditCtrl,bRedraw?TRUE:FALSE);

			// Ensure that the core domain is still visible
			m_ComboQuero.PositionText(true,false,bRedraw);

			// Update the tooltip rectangles of the embedded buttons
			if(m_ComboQuero.hToolTipControl)
			{
				ti.cbSize=sizeof(TOOLINFO);
				ti.hwnd=m_ComboQuero.m_hWnd;

				ti.rect.left=ti.rect.right-EMBEDICONS_MARGIN+2;
				ti.rect.right++;

				i=0;
				while(i<MAXEMBEDBUTTONS)
				{
					ti.uId=i++;
					ti.rect.left-=EMBEDICONS_SLOTWIDTH;
					if(i==nEmbedIcons) ti.rect.left-=2;
					::SendMessage(m_ComboQuero.hToolTipControl,TTM_NEWTOOLRECT,0,(LPARAM)&ti);
					ti.rect.right=ti.rect.left;
				}
			}
		}

		// Redraw
		if(bRedraw && nEmbedIcons<=old_nEmbedIcons)
			m_ComboQuero.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW);

	} // bToolbarCreated
}

long CQToolbar::GetEmbedButtonsTotalWidth()
{
	return EMBEDICONS_MARGIN+nEmbedIcons*EMBEDICONS_SLOTWIDTH;
}

void CQToolbar::DrawItemIcon(HDC hDC,RECT *pRect,HICON hIcon,HBRUSH hBrush,int Offset,bool bDrawLoadingAnimation)
{
	int topx,topy;
	RECT rectErease;
	BLENDFUNCTION bf;
	HDC hDC_RotatingDisk;

	topx=pRect->left+3+Offset;
	topy=pRect->top+(pRect->bottom-pRect->top-g_Scaled_IconSize+1)/2;

	if(bDrawLoadingAnimation)
	{
		::FillRect(hDC,pRect,hBrush);
		hDC_RotatingDisk=m_IconAnimation.GetDC_RotatingDisk(hDC);
		if(hDC_RotatingDisk)
		{
			bf.BlendOp = AC_SRC_OVER;
			bf.BlendFlags = 0;
			bf.SourceConstantAlpha = 0xFF;
			bf.AlphaFormat = AC_SRC_ALPHA;
			AlphaBlend(hDC,topx,topy,ROTATING_DISK_ANIMATION_CX,ROTATING_DISK_ANIMATION_CY,hDC_RotatingDisk,ROTATING_DISK_ANIMATION_CX*m_IconAnimation.GetRotatingDiskAnimationStep(),0,ROTATING_DISK_ANIMATION_CX,ROTATING_DISK_ANIMATION_CY,bf);
		}
	}
	else if(hIcon)
	{
		rectErease=*pRect;

		rectErease.right=topx;
		::FillRect(hDC,&rectErease,hBrush);

		rectErease.left=topx+g_Scaled_IconSize;
		rectErease.right=pRect->left+QEDITCTRL_LEFTMARGIN;
		::FillRect(hDC,&rectErease,hBrush);

		DrawIconEx(hDC,topx,topy,hIcon,g_Scaled_IconSize,g_Scaled_IconSize,0,hBrush,DI_IMAGE|DI_MASK);
	}
	else ::FillRect(hDC,pRect,hBrush);
}

void CQToolbar::SetCurrentType(BYTE type,HICON hFavIcon,bool bRedraw)
{
	if(currentType!=type || (type==TYPE_ADDRESS && currentIcon!=hFavIcon))
	{
		currentType=type;

		if(type!=TYPE_SEARCH)
		{
			m_IconAnimation.Stop(true);
			// Remove tooltip
			if(m_ComboQuero.Hover==HOVER_SEARCHICON && m_ComboQuero.hToolTipControl) ::SendMessage(m_ComboQuero.hToolTipControl,TTM_POP,0,0);
		}

		switch(type)
		{
		case TYPE_SEARCH:
			currentIcon=g_Icons[PhraseNotFound?ICON_SEARCH_NOTFOUND:ICON_SEARCH];
			break;
		case TYPE_ADDRESS:
			if(hFavIcon) currentIcon=hFavIcon;
			else currentIcon=g_Icons[ICON_URL];
			break;
		default:
			currentIcon=NULL;
		}

		if(bRedraw)
		{
			m_ComboQuero.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW);
			m_ComboQuero.GetEditCtrl()->RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW);
		}
	}
}

void CQToolbar::ToggleCurrentType()
{
	if(g_Options2&OPTION2_AddressNavigation)
	{
		m_ComboQuero.bCurrentURLDisplayed=false;
		SetCurrentType((currentType==TYPE_SEARCH)?TYPE_ADDRESS:TYPE_SEARCH,NULL,true);
	}
}

void CQToolbar::SetSearchIcon(int IconId)
{
	HICON hIcon;
	
	hIcon=g_Icons[IconId];

	if(currentIcon!=hIcon)
	{
		currentIcon=hIcon;
		m_ComboQuero.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW);
	}
}

void CQToolbar::SetIconOffset(int offset)
{
	if(currentIconOffset!=offset)
	{
		currentIconOffset=offset;
		DrawComboQueroIcon();
	} // End currentIconOffset!=offset
}

void CQToolbar::DrawComboQueroIcon()
{
	HDC hDC;
	RECT rect;

	if(m_ComboQuero.IsWindow())
	{
		hDC=m_ComboQuero.GetDC();
		if(hDC)
		{
			rect=m_ComboQuero.m_rcItem;
			rect.right=rect.left+g_Scaled_IconSize+5;

			#ifdef COMPILE_FOR_WINDOWS_VISTA
				CBufferedPaint BufferedPaint;
				HDC hBufferedDC;
				if(BufferedPaint.Begin(hDC,&rect,BPBF_TOPDOWNDIB,NULL,&hBufferedDC))
				{
					FillRect(hBufferedDC,&rect,hDefaultBackground);
					DrawItemIcon(hBufferedDC,&rect,currentIcon,hDefaultBackground,currentIconOffset,FALSE);
					BufferedPaint.MakeOpaque();
					BufferedPaint.End();
				}
			#else
				DrawItemIcon(hDC,&rect,currentIcon,hDefaultBackground,currentIconOffset,IsLoadingAnimation());
			#endif

			m_ComboQuero.ReleaseDC(hDC);
		} // End GetDC
	} // End IsWindow
}

LRESULT CQToolbar::OnSetFavIcon(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	SetFavIcon((HICON)lParam);

	if(currentType==TYPE_ADDRESS)
	{
		currentIcon=currentFavIcon;
		m_ComboQuero.RedrawWindow(NULL,NULL,RDW_INVALIDATE|RDW_NOERASE);
	}

	// Assign the favicon to the IE window
	if(g_Options&OPTION_ShowFavIconsInTaskbar) SetIcon_IEFrame((HICON)lParam);

	bHandled=TRUE;
	return 0;
}

void CQToolbar::SetIcon_IEFrame(HICON hIcon)
{
	HWND hwnd_IEFrame;
	
	//QDEBUG_PRINTF(L"SetIcon_IEFrame",L"instance %d active %d",QueroInstanceId,IsActive);

	if(IsActive)
	{
		hwnd_IEFrame=GetIEFrameWindow();

		if(pQueroBroker)
		{
			pQueroBroker->SetIcon(HandleToLong(hwnd_IEFrame),HandleToLong(hIcon));
		}
		#ifndef COMPILE_FOR_WINDOWS_VISTA
		else
		{
			HICON hIconCopy;
			HICON hPrevIcon;

			if(WaitForSingleObject(g_hQSharedDataMutex,DOWNLOAD_MUTEX_TIMEOUT)==WAIT_OBJECT_0)
			{
				if(hIcon==NULL) hIcon=g_IE_Icon;
				hIconCopy=CopyIcon(hIcon);
				if(hIconCopy)
				{			
					hPrevIcon=(HICON)::SendMessage(hwnd_IEFrame,WM_SETICON,ICON_SMALL_FAVICON,(LPARAM)hIconCopy);				
					if(g_IE_Icon==NULL) g_IE_Icon=hPrevIcon;
					else if(hPrevIcon) DestroyIcon(hPrevIcon);
				}				
				ReleaseMutex(g_hQSharedDataMutex);
			}
		} // End send WM_SETICON
		#endif
	} // End IsActive
}

void CQToolbar::SetPhraseNotFound(bool notfound,bool noredraw)
{
	int IconId;

	if(notfound!=PhraseNotFound)
	{
		PhraseNotFound=notfound;

		if(currentType==TYPE_SEARCH)
		{
			IconId=notfound?ICON_SEARCH_NOTFOUND:ICON_SEARCH;

			if(noredraw==false)
			{
				SetSearchIcon(IconId);
				m_ComboQuero.GetEditCtrl()->RedrawWindow();
			}
			else currentIcon=g_Icons[IconId];
		}
	}
}

void CQToolbar::SetColorScheme(int ColorSchemeId,bool bRedraw)
{
	COLORREF ColorScheme_Black[NCOLORS]={0,GetSysColor(COLOR_WINDOWTEXT),0xc0c0c0,0xe0e0e0,0,0x808080,PHRASENOTFOUNDCOLOR,0};
	COLORREF *ColorScheme;
	UINT i;
	ColorScheme = ColorScheme_Black;

	ColorScheme[COLOR_Background]=GetSysColor(COLOR_WINDOW);

	i=0;
	while(i<NCOLORS)
	{
		if(g_QueroTheme_DLL && g_ThemeColors[i]!=COLOR_UNDEFINED) Colors[i]=g_ThemeColors[i];
		else Colors[i]=ColorScheme[i];
		i++;
	}

	m_Profiles.SetFontColor(Colors[COLOR_Link]);

	// Update colors of search engines
	for(i=0;i<nengines;i++)
	{
		m_ComboEngine.SendMessage(CB_SETITEMDATA,i+1,(LPARAM)m_Profiles.GetEngineColor(&m_Profiles.CurrentProfile.Engines[i]));
	}

	// Create highlight brush
	if(hHighlightBrush) DeleteObject(hHighlightBrush);
	hHighlightBrush=CreateSolidBrush(Colors[COLOR_Highlight]);

	// Redraw
	if(bRedraw)
	{
		m_ComboQuero.bURLChanged=true;
		m_ComboQuero.GetEditCtrl()->RedrawWindow();
		m_ComboEngine.RedrawWindow();
	}
}

TCHAR* CQToolbar::trim(TCHAR *pStr)
{
	TCHAR *pRead,*pWrite;

	if(pStr)
	{
		pRead=pWrite=pStr;

		while(*pRead)
		{
			if(!_istspace(*pRead))
			{
				*pWrite=*pRead;
				pWrite++;
			}
			else if(!_istspace(pRead[1]) && pRead[1] && pWrite!=pStr) // Skip leading and trailing whitespaces
			{
				*pWrite=L' ';
				pWrite++;
			}
			pRead++;
		}
		*pWrite=L'\0';
	}

	return pStr;
}

bool CQToolbar::TrimNewlines(TCHAR *pStrDest,size_t cchDest,TCHAR *pStrSrc)
{
	bool result;
	TCHAR ch;
	
	if(pStrSrc && pStrDest && cchDest)
	{
		ch=*pStrSrc;
		cchDest--; // Reserve space for the terminating null character
		while(ch && cchDest)
		{
			if(ch!=L'\r' && ch!=L'\n')
			{
				*pStrDest=ch;
				pStrDest++;
				cchDest--;
			}
			pStrSrc++;
			ch=*pStrSrc;
		}

		*pStrDest=L'\0';

		result=true;
	}
	else result=false;

	return result;
}

TCHAR *CQToolbar::GetString(UINT id)
{
	if(LoadString(_Module.GetResourceInstance(),id,String,MAX_STRING_LENGTH)==0)
	{
		String[0]=L'?';
		String[1]=0;
	}
	return String;
}

void CQToolbar::SyncHistory(bool Synchronize)
{
	int i;
#ifdef COMPILE_FOR_WIN9X
	CHAR buffer[REGKEYLENGTH];
#else
	TCHAR buffer[REGKEYLENGTH];
#endif
	LONG result;
	HKEY hKeyQuero = NULL;
	DWORD size,regdatasize,n;
	HQRegData historydata;

	// Load main history from registry if logical time differs

	if(Synchronize==false || WaitForSingleObject(g_hQSharedMemoryMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		if(g_QSharedMemory==NULL || g_QSharedMemory->LTimeHistory!=g_LTimeHistory)
		{
			if(Synchronize==false || WaitForSingleObject(g_hQSharedListMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
			{
				FreeHistory(g_History,&g_HistoryIndex);

				hKeyQuero=OpenQueroKey(HKEY_CURRENT_USER,L"History",false);
				if(hKeyQuero)
				{
					n=0;
					result=RegQueryInfoKey(hKeyQuero,NULL,NULL,NULL,NULL,NULL,NULL,&n,NULL,NULL,NULL,NULL);
					if(result==ERROR_SUCCESS)
					{
						if(n>HISTORYSIZE) i=HISTORYSIZE;
						else i=n;
					}
					else i=0;
					g_HistoryIndex=i;

					while(i>0)
					{
						i--;
						n--;
						size=REGKEYLENGTH;
						regdatasize=sizeof(HQRegData);
						#ifdef COMPILE_FOR_WIN9X
						result=RegEnumValueA(hKeyQuero,n,buffer,&size,NULL,NULL,(LPBYTE)&historydata,&regdatasize);
						if(result==ERROR_SUCCESS)
						{
							g_History[i].Query=SysAllocStringLen(NULL,size);
							if(g_History[i].Query)
							{
								if(MultiByteToWideChar(CP_ACP,0,buffer,-1,g_History[i].Query,size+1)==0)
								{
									SysFreeString(g_History[i].Query);
									g_History[i].Query=NULL;
								}
							}
							g_History[i].ProfileId=historydata.ProfileId;
							g_History[i].EngineId=historydata.EngineId;
							g_History[i].Type=historydata.Type;
							g_History[i].Timestamp=historydata.Timestamp;
							g_History[i].Flags=historydata.Flags;
							g_History[i].hIcon=NULL;
						}
						else g_History[i].Query=NULL;
						#else
						result=RegEnumValue(hKeyQuero,n,buffer,&size,NULL,NULL,(LPBYTE)&historydata,&regdatasize);
						if(result==ERROR_SUCCESS)
						{
							g_History[i].Query=SysAllocStringLen(buffer,size);
							g_History[i].ProfileId=historydata.ProfileId;
							g_History[i].EngineId=historydata.EngineId;
							g_History[i].Type=historydata.Type;
							g_History[i].Timestamp=historydata.Timestamp;
							g_History[i].Flags=historydata.Flags;
							g_History[i].hIcon=NULL;
						}
						else g_History[i].Query=NULL;
						#endif
					}
					RegCloseKey(hKeyQuero);
				}

				// Update logical time
				if(g_QSharedMemory) g_LTimeHistory=g_QSharedMemory->LTimeHistory;
				else g_LTimeHistory=LTimeHistory+1;

				if(Synchronize) ReleaseMutex(g_hQSharedListMutex);
			}
		}
		if(Synchronize) ReleaseMutex(g_hQSharedMemoryMutex);
	}
}

bool CQToolbar::SyncLocalHistory()
{
	bool changed=false;

	SyncHistory();

	if(WaitForSingleObject(g_hQSharedListMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		if(g_LTimeHistory!=LTimeHistory)
		{
			FreeHistory(History,&HistoryIndex);

			HistoryIndex=0;
			while(HistoryIndex<g_HistoryIndex)
			{
				History[HistoryIndex]=g_History[HistoryIndex];
				History[HistoryIndex].Query=SysAllocString(g_History[HistoryIndex].Query);
				HistoryIndex++;
			}

			// Update logical time
			LTimeHistory=g_LTimeHistory;

			changed=true;
		}
		ReleaseMutex(g_hQSharedListMutex);
	}

	return changed;
}

void CQToolbar::CopyLastHistoryEntry(HistoryEntry *pHistoryEntry,bool Synchronize)
{
	FreeLastHistoryEntry();

	if(g_Options&OPTION_RememberLastSearch)
	{
		if(Synchronize==false || WaitForSingleObject(g_hQSharedListMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
		{
			if(pHistoryEntry==NULL && g_HistoryIndex) pHistoryEntry=&g_History[g_HistoryIndex-1];

			if(pHistoryEntry)
			{
				LastHistoryEntry=*pHistoryEntry;
				LastHistoryEntry.Query=SysAllocString(pHistoryEntry->Query);
			}
			
			if(Synchronize) ReleaseMutex(g_hQSharedListMutex);
		}
	}
}

void CQToolbar::FreeLastHistoryEntry()
{
	if(LastHistoryEntry.Query)
	{
		SysFreeString(LastHistoryEntry.Query);
		LastHistoryEntry.Query=NULL;
	}
}

void CQToolbar::FreeLastQueryURL()
{
	if(LastQueryURL)
	{
		SysFreeString(LastQueryURL);
		LastQueryURL=NULL;
	}
}

void CQToolbar::FreeCurrentDocumentTitle()
{
	if(bstrCurrentDocumentTitle)
	{
		SysFreeString(bstrCurrentDocumentTitle);
		bstrCurrentDocumentTitle=NULL;
	}
}

void CQToolbar::SyncURLs(bool Synchronize)
{
	int i;
#ifdef COMPILE_FOR_WIN9X
	CHAR buffer[REGKEYLENGTH];
#else
	TCHAR buffer[REGKEYLENGTH];
#endif
	LONG result;
	HKEY hKeyQuero = NULL;
	DWORD size,regdatasize,n;
	HURLRegData URLdata;

	// Load URL history from registry if logical time differs

	if(Synchronize==false || WaitForSingleObject(g_hQSharedMemoryMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		if(g_QSharedMemory==NULL || g_QSharedMemory->LTimeURLs!=g_LTimeURLs)
		{
			if(Synchronize==false || WaitForSingleObject(g_hQSharedListMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
			{
				FreeURLs();
	
				hKeyQuero=OpenQueroKey(HKEY_CURRENT_USER,L"URL",false);
				if(hKeyQuero)
				{
					n=0;
					result=RegQueryInfoKey(hKeyQuero,NULL,NULL,NULL,NULL,NULL,NULL,&n,NULL,NULL,NULL,NULL);
					if(result==ERROR_SUCCESS)
					{
						if(n>URLHISTORYSIZE) i=URLHISTORYSIZE;
						else i=n;
					}
					else i=0;
					g_nURLs=i;

					while(i>0)
					{
						i--;
						n--;
						size=REGKEYLENGTH;
						regdatasize=sizeof(HURLRegData);
						#ifdef COMPILE_FOR_WIN9X
						result=RegEnumValueA(hKeyQuero,n,buffer,&size,NULL,NULL,(LPBYTE)&URLdata,&regdatasize);
						if(result==ERROR_SUCCESS)
						{
							g_URLs[i]=SysAllocStringLen(NULL,size);
							if(g_URLs[i])
								if(MultiByteToWideChar(CP_ACP,0,buffer,-1,g_URLs[i],size+1)==0)
								{
									SysFreeString(g_URLs[i]);
									g_URLs[i]=NULL;
								}
						}
						else g_URLs[i]=NULL;
						#else
						result=RegEnumValue(hKeyQuero,n,buffer,&size,NULL,NULL,(LPBYTE)&URLdata,&regdatasize);
						if(result==ERROR_SUCCESS)
						{
							g_URLs[i]=SysAllocStringLen(buffer,size);
						}
						else g_URLs[i]=NULL;
						#endif
					}
					RegCloseKey(hKeyQuero);
				}

				// Update logical time
				if(g_QSharedMemory) g_LTimeURLs=g_QSharedMemory->LTimeURLs;

				if(Synchronize) ReleaseMutex(g_hQSharedListMutex);
			}
		}
		if(Synchronize) ReleaseMutex(g_hQSharedMemoryMutex);
	}
}

void CQToolbar::FreeHistory(HistoryEntry* History,UINT* HistoryIndex)
{
	UINT i;

	for(i=0;i<*HistoryIndex;i++)
	{
		if(History[i].Query)
		{
			SysFreeString(History[i].Query);
			History[i].Query=NULL;
			if(History[i].hIcon) DestroyIcon(History[i].hIcon);
		}
	}

	*HistoryIndex=0;
}

void CQToolbar::FreeURLs()
{
	UINT i;

	for(i=0;i<g_nURLs;i++)
	{
		SysFreeString(g_URLs[i]);
		g_URLs[i]=NULL;
	}

	g_nURLs=0;
}

void CQToolbar::ClearHistory()
{
	HKEY hKeyQuero;

	if(WaitForSingleObject(g_hQSharedMemoryMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
	{
		if(WaitForSingleObject(g_hQSharedListMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
		{
			hKeyQuero=OpenQueroKey(HKEY_CURRENT_USER,NULL,true);
			if(hKeyQuero)
			{
				SHDeleteKey(hKeyQuero,L"History");
				SHDeleteKey(hKeyQuero,L"URL");
				RegCloseKey(hKeyQuero);

			}
			if(RegOpenKeyEx(HKEY_CURRENT_USER,L"Software\\Microsoft\\Internet Explorer",0,KEY_WRITE,&hKeyQuero)==ERROR_SUCCESS)
			{
				SHDeleteKey(hKeyQuero,L"TypedURLs");
				RegCloseKey(hKeyQuero);
			}

			FreeHistory(g_History,&g_HistoryIndex);
			FreeLastHistoryEntry();
			FreeURLs();

			// Advance logical times
			if(g_QSharedMemory)
			{
				g_LTimeHistory=++(g_QSharedMemory->LTimeHistory);
				g_LTimeURLs=++(g_QSharedMemory->LTimeURLs);
			}

			IUrlHistoryStg2* pUrlHistoryStg2;
			HRESULT hr;

			pUrlHistoryStg2=NULL;
			hr = CoCreateInstance(CLSID_CUrlHistory,NULL,CLSCTX_INPROC_SERVER, IID_IUrlHistoryStg2,(LPVOID*)&pUrlHistoryStg2);
			if(SUCCEEDED_OK(hr))
			{
				hr = pUrlHistoryStg2->ClearHistory(); 
				pUrlHistoryStg2->Release();
			}

			ReleaseMutex(g_hQSharedListMutex);
		}

		ReleaseMutex(g_hQSharedMemoryMutex);
	}

	// Free last history entries of other Quero instances
	UpdateQueroInstances(UPDATE_FREELASTHISTORYENTRY);
}

#define ID_BLOCKED_DIVS 0x1000
#define ID_BLOCKED_ADSCRIPTS 0x1001
#define ID_BLOCK_ADS 0x1002
#define ID_BLOCK_POPUPS 0x1003
#define ID_ALLOWED_SITES 0x1004
#define ID_TEMP_UNBLOCK 0x1005
#define ID_HIDE_FLASH 0x1006

HistoryEntry * CQToolbar::GetLastHistoryEntry()
{
	return (LastHistoryEntry.Query)?&LastHistoryEntry:NULL;
}

void CQToolbar::SelectProfile(int ProfileId,int EngineId)
{
	UINT i;
	TCHAR ProfileName[MAX_PROFILE_NAME_LEN+1];

	// Reset the combo box list
	if(m_ComboEngine.SendMessage(CB_GETDROPPEDSTATE,0,0)) m_ComboEngine.SendMessage(CB_SHOWDROPDOWN,FALSE,0);
	m_ComboEngine.SendMessage(CB_RESETCONTENT,0,0);

	// Workaround: CBN_SELENDOK is sent after CBN_SELENDCANCEL
	#ifdef COMPILE_FOR_WIN9X
	chooseProfile=false;
	#endif

	// Load the search profile
	if(m_Profiles.LoadCurrentProfile(ProfileId))
	{
		CurrentProfileId=ProfileId;
		nengines=m_Profiles.CurrentProfile.EngineCount;
		if(EngineId==-1) EngineId=m_Profiles.CurrentProfile.DefaultEngineId;
	}
	// Fallback: Load default profile
	else if(m_Profiles.LoadCurrentProfile(m_Profiles.DefaultProfileId))
	{
		CurrentProfileId=m_Profiles.DefaultProfileId;
		nengines=m_Profiles.CurrentProfile.EngineCount;
		EngineId=m_Profiles.CurrentProfile.DefaultEngineId;
	}
	else
	{
		CurrentProfileId=-1;
		nengines=0;
	}
	CurrentEngineIndex=m_Profiles.EngineIdToIndex(EngineId);
	if(CurrentEngineIndex==-1) CurrentEngineIndex=0;

	// Insert the search profile header at the top of the list
	ProfileName[0]=L' ';
	ProfileName[1]=0;
	if(m_Profiles.CurrentProfile.Name) StringCchCopy(ProfileName+1,MAX_PROFILE_NAME_LEN,m_Profiles.CurrentProfile.Name);
	m_ComboEngine.SendMessage(CB_ADDSTRING,0,(LPARAM)ProfileName);
	m_ComboEngine.SendMessage(CB_SETITEMDATA,0,(LPARAM)CurrentProfileId);

	// Insert the search engines
	nseparators=1;
	for(i=0;i<nengines;i++)
	{
		m_ComboEngine.SendMessage(CB_ADDSTRING,0,(LPARAM)(m_Profiles.CurrentProfile.Engines[i].Name?m_Profiles.CurrentProfile.Engines[i].Name:L""));
		m_ComboEngine.SendMessage(CB_SETITEMDATA,i+1,(LPARAM)m_Profiles.GetEngineColor(&m_Profiles.CurrentProfile.Engines[i]));
		if(m_Profiles.CurrentProfile.Engines[i].HasSeparator) nseparators++;
	}
	if(nengines==0) m_ComboEngine.SendMessage(CB_ADDSTRING,0,(LPARAM)L"");

	// Set the height of the drop-down list
	SizeComboEngineList();

	// Select the current engine
	SelectEngine(CurrentEngineIndex,true,true,true);
}

void CQToolbar::OnNavigateBrowser(TCHAR *newurl,bool first)
{
	int i;
	bool bUpdateEmbedButtons;
	bool bForceResizeEditCtrl;
	BYTE old_SpecialCharsInURL=SpecialCharsInURL;
	TCHAR old_hostname[MAXURLLENGTH];
	TCHAR new_hostname[MAXURLLENGTH];

	NavigationPending=false;
	
	bUpdateEmbedButtons=false;
	bForceResizeEditCtrl=false;

	if(HostEndIndex) StringCbCopyN(old_hostname,sizeof old_hostname,currentURL,HostEndIndex*sizeof(TCHAR));
	else old_hostname[0]=L'\0';

	SpecialCharsInURL=SetCurrentURL(newurl);

	if(SpecialCharsInURL!=old_SpecialCharsInURL && g_ShowURL) bUpdateEmbedButtons=true;

	ClearLastFoundText();

	LastProgress=0;

	if(ImFeelingLucky)
	{
		Searching=false;
	}

	// Is new hostname or scheme?
	if(HostEndIndex) StringCbCopyN(new_hostname,sizeof new_hostname,currentURL,HostEndIndex*sizeof(TCHAR));
	else new_hostname[0]=L'\0';

	if(StrCmp(old_hostname,new_hostname))
	{
		// Reset certificate information
		if(Certificate_Organization_Extent)
		{
			bUpdateEmbedButtons=true;
			bForceResizeEditCtrl=true;		
			Certificate_Organization_Extent=0;
		}

		// Download new Favicon.ico from root
		if(currentFavIcon)
		{
			DestroyIcon(currentFavIcon);
			currentFavIcon=NULL;
			if(g_Options&OPTION_ShowFavIconsInTaskbar) SetIcon_IEFrame(NULL);
		}

		if(g_Options&OPTION_DownloadFavIcon)
		{
			TCHAR NewFavIconURL[MAXURLLENGTH];

			if(!StrCmpN(currentAsciiURL,L"http://",7) || !StrCmpN(currentAsciiURL,L"https://",8))
			{
				StringCchCopyN(NewFavIconURL,MAXURLLENGTH,currentAsciiURL,HostEndIndexAscii+1);
				StringCchCopy(NewFavIconURL+HostEndIndexAscii,MAXURLLENGTH-HostEndIndexAscii,L"/favicon.ico");
			
			}
		}
	}

	// Update search box and add URL to history
	if(g_IE_MajorVersion<7) SecureLockIcon_IE=(!NavigationFailed && !StrCmpN(currentURL,_T("https://"),8));
	if(SecureLockIcon_Quero!=(SecureLockIcon_IE && g_ShowURL)) bUpdateEmbedButtons=true;
	SecureLockIcon_Quero=SecureLockIcon_IE;

	if(InternalLink)
	{
		if(g_ShowURL && m_ComboQuero.bIsEmptySearch==false)
		{
			m_ComboQuero.SetTextPrompt();
		}
	}
	else if(Searching)
	{
		// Show URL if the hostname of the current URL is different from the LastQueryURL
		// or ShowSearchTermsWhileSearching is disabled
		if(g_ShowURL)
		{
			if(LastQueryURL)
			{
				int LastQueryURL_HostStartIndex;

				if(!StrCmpN(LastQueryURL,L"http://",7)) LastQueryURL_HostStartIndex=7;
				else if(!StrCmpN(LastQueryURL,L"https://",8)) LastQueryURL_HostStartIndex=8;
				else LastQueryURL_HostStartIndex=0;

				if(StrCmpN(currentAsciiURL+HostStartIndexAscii,LastQueryURL+LastQueryURL_HostStartIndex,HostEndIndexAscii-HostStartIndexAscii+1)) FreeLastQueryURL();
			}
			
			if(LastQueryURL==NULL || (g_Options2&OPTION2_ShowSearchTermsWhileSearching)==0) m_ComboQuero.SetTextCurrentURL();
		}
		FreeLastQueryURL();
	}
	else if(InterceptSearch(currentURL,currentAsciiURL,NULL))
	{
		// Search intercepted
		Searching=true;
	}
	else
	{
		// Save to URL history
		if(!NavigationFailed)
		{
			HistoryEntry *lastSearch=GetLastHistoryEntry();
			CoFileTimeNow(&URLNavigationTime);


		}
		// Show URL
		// m_pBrowser->get_AddressBar(&bar); doesnt work correctly: returns always VARIANT_TRUE
		if(g_ShowURL)
		{
			m_ComboQuero.SetTextCurrentURL();
		}
	}

	SetPhraseNotFound(false);

	if(bUpdateEmbedButtons) UpdateEmbedButtons(bForceResizeEditCtrl,true);

	// Zoom Page
	if(ZoomFactor!=100)
	{
		BYTE ZoomMethod;
		
		if(UseOpticalZoom())
		{
			ZoomMethod=first?ZOOMPAGE_SET_OPTICAL_ZOOM:0;
		}
		else ZoomMethod=ZOOMPAGE_PUT_CSSTEXT;

		if(ZoomMethod) ZoomPage(ZoomFactor,ZoomMethod);
	}

	// Initilize words to be highlighted
	if(Highlight)
	{
		InitWords(HighlightedWords,&nHighlightedWords,INITWORDS_SearchBox|INITWORDS_History);
	}
}

void CQToolbar::OnTitleChange(BSTR bstrTitle)
{
	if(StrCmp(bstrCurrentDocumentTitle,bstrTitle))
	{
		FreeCurrentDocumentTitle();
		bstrCurrentDocumentTitle=SysAllocString(bstrTitle);
		SetTitle_IEFrame();
	}
}

void CQToolbar::SetTitle_IEFrame()
{
	if(IsActive && pQueroBroker && (g_Options&OPTION_HideNavigationBar)) pQueroBroker->SetTitle(HandleToLong(GetIEFrameWindow()),bstrCurrentDocumentTitle);
}

void CQToolbar::OnSetSecureLockIcon(SecureLockIconConstants SecureLockIconStatus)
{
	SecureLockIcon_IE=SecureLockIconStatus>=secureLockIconSecureUnknownBits;

	if(SecureLockIcon_IE!=SecureLockIcon_Quero && (SecureLockIcon_IE==false || NavigationPending==false))
	{
		SecureLockIcon_Quero=SecureLockIcon_IE;
		if(g_ShowURL)
		{
			m_ComboQuero.SetRedraw(FALSE);
			m_ComboQuero.GetEditCtrl()->SetRedraw(FALSE);
			UpdateEmbedButtons(false,false);
			m_ComboQuero.SetRedraw(TRUE);
			m_ComboQuero.GetEditCtrl()->SetRedraw(TRUE);
			m_ComboQuero.Redraw(true,RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW,RDW_INVALIDATE|RDW_NOERASE|RDW_UPDATENOW);
		}
	}
}

BYTE CQToolbar::SetCurrentURL(TCHAR *url) // Returns special character class
{
	TCHAR *pCurrentURL;
	int i;
	BYTE result=SPECIALCHARS_NON;

	StringCbCopy(currentAsciiURL,sizeof currentAsciiURL,url);

	HostStartIndex=0;
	HostEndIndex=0;
	HostStartIndexAscii=0;
	HostEndIndexAscii=0;
	DomainStartIndex=0;
	DomainStartIndexAscii=0;
	CoreDomainStartIndex=0;
	CoreDomainEndIndex=0;

		StringCbCopy(currentURL,sizeof currentURL,url);

		// Highlighting the most relevant part of the address
		i=0;
		pCurrentURL=currentURL;
		while(*pCurrentURL && *pCurrentURL!=L':' && *pCurrentURL!=L'/' && *pCurrentURL!=L'\\')
		{
			pCurrentURL++;
			i++;
		}
		if(i) CoreDomainEndIndex=i;

	// Calculate start and end extents for domain highlighting
	MeasureDomainExtents(currentURL,CoreDomainStartIndex,CoreDomainEndIndex,&CoreDomainStartExtent,&CoreDomainEndExtent);

	return result;
}

void CQToolbar::MeasureDomainExtents(TCHAR *url,int DomainStartIndex,int DomainEndIndex,int *DomainStartExtent,int *DomainEndExtent)
{
	HDC hDC;

	if(DomainStartIndex==DomainEndIndex)
	{
		*DomainStartExtent=*DomainEndExtent=0;
	}
	else
	{
		hDC=m_ComboQuero.GetEditCtrl()->GetDC();
		if(hDC)
		{
			SelectObject(hDC,hFont);

			SIZE size;

			GetTextExtentPoint32(hDC,url,DomainStartIndex,&size);
			*DomainStartExtent=size.cx;
			GetTextExtentPoint32(hDC,url,DomainEndIndex,&size);
			*DomainEndExtent=size.cx;
			m_ComboQuero.GetEditCtrl()->ReleaseDC(hDC);
		}
		else *DomainStartExtent=*DomainEndExtent=0;
	}
}

int CQToolbar::MeasureTextExtent(TCHAR *pText,UINT uiTextLength)
{
	HDC hDC;
	SIZE size;

	size.cx=0;

	hDC=m_ComboQuero.GetDC();
	if(hDC)
	{
		SelectObject(hDC,hFont);
		GetTextExtentPoint32(hDC,pText,uiTextLength,&size);
		m_ComboQuero.ReleaseDC(hDC);
	}

	return size.cx;
}

void CQToolbar::MakeAbsoluteURL(TCHAR *AbsoluteURL,TCHAR *URL,TCHAR *BaseURL)
{
	// To do: support base url tag; add parameter IHTMLDocument2, query base url if BaseURL==NULL
		DWORD len;

		len=MAXURLLENGTH;

		if(BaseURL==NULL) BaseURL=currentURL;

		if(UrlCombine(BaseURL,URL,AbsoluteURL,&len,0)!=S_OK)
		{
			AbsoluteURL[0]=0;
		}
}

#define NADIMAGESIZES 11
#define NFILTERPATTERNS 2
#define FILTERPATTERN_SIZE 0
#define NFILTERLABELS 31
#define MAX_FILTER_LABELS 64
#define MAX_FILTER_LABEL_LEN 16
#define NLABELDELIMITERS 8
#define NVIDEOPLAYERURLS 8

void CQToolbar::OnNavigateError(TCHAR *url,long StatusCode,SHORT *Cancel)
{
	NavigationFailed=true;

	if(StatusCode==INET_E_RESOURCE_NOT_FOUND)
	{
		CoFileTimeNow(&URLNavigationTime);

		if(g_Options&OPTION_SearchOnDNSFailure)
		{
			TCHAR *lastSearch;
			HistoryEntry *lastEntry=GetLastHistoryEntry();

			if(lastEntry) lastSearch=lastEntry->Query;
			else lastSearch=NULL;

		} // End OPTION_SearchOnDNSFailure
	} // End INET_E_RESOURCE_NOT_FOUND
}

void CQToolbar::OnSiteChange()
{
	BSTR location=NULL;

	ClearLastFoundText();
	LastProgress=0;
	nHighlightedWords=0;
	m_IconAnimation.Stop(ICON_ANIMATION_ALL);
	Searching=false;

	if(SUCCEEDED_OK(m_pBrowser->get_LocationURL(&location)) && location)
	{
		SpecialCharsInURL=SetCurrentURL(location);
		SysFreeString(location);
	}
	else currentURL[0]=_T('\0');

	if(g_ShowURL)
	{
		bool IsSecureProtocol;

		IsSecureProtocol=!StrCmpN(currentURL,_T("https://"),8);

		SecureLockIcon_Quero=IsSecureProtocol;

		m_ComboQuero.SetTextCurrentURL();
	}
	
	UpdateEmbedButtons(false,true);
}
void CQToolbar::AddToURLHistory(TCHAR* entry)
{
	UINT i;
	HKEY hKeyQuero;
	DWORD size;
	TCHAR buffer[REGKEYLENGTH], * found;
	HURLRegData data;
	size_t len;

	if (g_Options & OPTION_SaveAddressHistory)
	{
		CoFileTimeNow(&data.Timestamp);
		data.reserved1 = 0;
		data.reserved2 = 0;

		StrCchLen(entry, MAXURLLENGTH, len);
		if (len > 0 && len < REGKEYLENGTH) // Do not add if too long for registry
		{
			if (WaitForSingleObject(g_hQSharedMemoryMutex, QMUTEX_TIMEOUT) == WAIT_OBJECT_0)
			{
				if (WaitForSingleObject(g_hQSharedListMutex, QMUTEX_TIMEOUT) == WAIT_OBJECT_0)
				{
					SyncURLs(false);

					for (i = 0; i < g_nURLs; i++) if (g_URLs[i]) if (!StrCmp(entry, g_URLs[i])) break;

					hKeyQuero = OpenQueroKey(HKEY_CURRENT_USER, L"URL", true);

					if (i == g_nURLs) // Not found
					{
						if (g_nURLs >= URLHISTORYSIZE)
						{
							g_nURLs--;
							if (g_URLs[0]) SysFreeString(g_URLs[0]);
							CopyMemory(g_URLs, g_URLs + 1, (g_nURLs) * sizeof(BSTR));

							// Delete first history entry
							if (hKeyQuero)
							{
								size = REGKEYLENGTH;
								if (RegEnumValue(hKeyQuero, 0, buffer, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
									RegDeleteValue(hKeyQuero, buffer);
							}
						}
						g_URLs[g_nURLs] = SysAllocString(entry);
						if (g_URLs[g_nURLs])
						{
							g_nURLs++;

							if (hKeyQuero) RegSetValueEx(hKeyQuero, entry, 0, REG_BINARY, (LPBYTE)&data, sizeof(HURLRegData));
						}
					}
					else if (g_nURLs > 1 && i != g_nURLs - 1) // Copy old history entry to top of list
					{
						if (hKeyQuero)
						{
							RegDeleteValue(hKeyQuero, entry);
							RegSetValueEx(hKeyQuero, entry, 0, REG_BINARY, (LPBYTE)&data, sizeof(HURLRegData));
						}

						found = g_URLs[i];
						CopyMemory(g_URLs + i, g_URLs + i + 1, (g_nURLs - i - 1) * sizeof(BSTR));
						g_URLs[g_nURLs - 1] = found;
					}
					else // Update
					{
						if (hKeyQuero) RegSetValueEx(hKeyQuero, entry, 0, REG_BINARY, (LPBYTE)&data, sizeof(HURLRegData));
					}

					if (hKeyQuero) RegCloseKey(hKeyQuero);

					// Advance logical time
					if (g_QSharedMemory) g_LTimeURLs = ++(g_QSharedMemory->LTimeURLs);

					ReleaseMutex(g_hQSharedListMutex);
				}

				ReleaseMutex(g_hQSharedMemoryMutex);
			}
		}
	} // End OPTION_SaveAddressHistory
}
void CQToolbar::OnBeforeNavigate(IDispatch *pDisp,VARIANT *vUrl,VARIANT *vFlags, VARIANT *vTarget,VARIANT *vPostData,VARIANT *vHeaders,SHORT *Cancel,bool first,bool toplevel)
{
	BSTR newurl=vUrl->bstrVal;
	long PostDataLen=0;
	char *PostData=NULL;
	int idna_status;
	BSTR bstrQuero;
	bool bRedirectBrowser=false;
	TCHAR AsciiURL[MAXURLLENGTH];
	TCHAR interceptedSearchTerms[MAXURLLENGTH];
	TCHAR PostDataUnicode[MAXURLLENGTH];

	if(newurl)
	{
		// Access the PostData
		if(vPostData && vPostData->vt==(VT_VARIANT|VT_BYREF) && ((vPostData->pvarVal->vt)&VT_ARRAY))
		{
			char *szTemp = NULL;
			long plLbound, plUbound;

			SAFEARRAY *parrTemp = vPostData -> pvarVal->parray;
			SafeArrayAccessData(parrTemp , (void HUGEP **) &szTemp);

			SafeArrayGetLBound(parrTemp , 1, &plLbound);
			SafeArrayGetUBound(parrTemp , 1, &plUbound);

			PostDataLen=plUbound - plLbound + 1;
			if(PostDataLen)
			{
				PostData = new char[PostDataLen + 1];
				if(PostData)
				{
					StringCbCopyA(PostData,PostDataLen + 1,szTemp);
				}
				else PostDataLen=0;
			}
			
			SafeArrayUnaccessData(parrTemp);
		}

		idna_status=0;

		if(*Cancel==FALSE)
		{
			// Update whitelist

			if(toplevel) // Handle top-level navigations only
			{
				NavigationFailed=false;
				StringCbCopy(beforeURL,sizeof beforeURL,newurl);
				BeforeHostStartIndex=0;
				BeforeHostEndIndex=0;
				BeforeDomainStartIndex=0;

				// Check URL

				if(*Cancel==FALSE)
				{
					NavigationPending=true;

					// Intercept last query
					if(LastQueryURL)
					{
						if(StrCmp(AsciiURL,LastQueryURL)) FreeLastQueryURL();
					}

					// Reset internal link state
					if(StrCmpN(beforeURL,_T("res://"),6)) InternalLink=false;

					// Determine whether a search is performed
					Searching=(LastQueryURL || ImFeelingLucky);

					// Intercept search
					if(Searching==false && InternalLink==false)
					{
						// Convert PostData to Unicode
						if(PostDataLen)
						{
							MultiByteToWideChar(CP_ACP,0,PostData,-1,PostDataUnicode,MAXURLLENGTH);
							PostDataUnicode[MAXURLLENGTH-1]=0;
						}
						else PostDataUnicode[0]=0;

						// Intercept search
						Searching=InterceptSearch(beforeURL,AsciiURL,PostDataUnicode);
						if(Searching==false && first)  // First navigation?
						{
							if(g_ShowURL)
							{
								m_ComboQuero.SetText(beforeURL,TYPE_ADDRESS,NULL,false);
							}
						}
					} // Intercept search end
				} // Navigation not canceled
			} // Top-level navigation end
			else NavigationPending=false;

		} // Not cancled

		// Free PostData
		if(PostData) delete[] PostData;
		
	} // newurl valid end
}

bool CQToolbar::InterceptSearch(TCHAR *pURL,TCHAR *pAsciiURL,TCHAR *pPostDataUnicode)
{
	bool result;
	int SearchEngineIndex;
	int SearchEngineId;
	TCHAR InterceptedSearchTerms[MAXURLLENGTH];

	result=false;

	if(g_Options2&OPTION2_ShowSearchTermsWhileSearching)
	{
		if(m_Profiles.InterceptSearch(pURL,pPostDataUnicode,&SearchEngineIndex,&SearchEngineId,InterceptedSearchTerms))
		{
			// Security precaution: Do not intercept queries that can be confused with real URLs!
			if(StrStr(InterceptedSearchTerms,L"://")==NULL || g_ShowURL==false)
			{
				result=true;
				m_ComboQuero.SetText(InterceptedSearchTerms,TYPE_SEARCH,NULL,false);
				FreeLastQueryURL();
				LastQueryURL=SysAllocString(pURL);
				SelectEngine(SearchEngineIndex);
			}
			// Add to history
			//AddToHistory(InterceptedSearchTerms,TYPE_SEARCH,0,SearchEngineId,CurrentProfileId);
			AddToURLHistory(InterceptedSearchTerms);
		}
	}

	return result;
}

void CQToolbar::NavigateUp(UINT newWinTab)
{
	TCHAR url[MAXURLLENGTH];
	int i;
	int slash_index_1;
	int slash_index_2;
	TCHAR ch;

	if(NavigateUp_Available())
	{
		StringCbCopy(url,sizeof url,currentURL);

		// Find last two slashes
		i=HostEndIndex;
		slash_index_1=i;
		slash_index_2=i;
		ch=currentURL[i];
		while(ch!=L'\0' && ch!=L'?' && ch!=L'#' && i<MAXURLLENGTH)
		{
			if(ch==L'/')
			{
				slash_index_2=slash_index_1;
				slash_index_1=i;
			}
			i++;
			ch=currentURL[i];
		}

		// Cut after last but one slash
		url[slash_index_2+1]=0;

		// Navigate
		Quero(url,TYPE_ADDRESS,g_ShowURL?QUERO_REDIRECT|QUERO_SETTEXT:QUERO_REDIRECT,newWinTab);
	}
}

bool CQToolbar::NavigateUp_Available()
{
	size_t len;

	StringCchLength(currentURL,MAXURLLENGTH,&len);

	return 1;
}

LRESULT CQToolbar::OnRedirectBrowser(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled=TRUE;
	return 0;
}

bool CQToolbar::GetHtmlDocument2(IHTMLDocument2 **ppHtmlDocument)
{
	IDispatch *pDispatch=NULL;
	VARIANT_BOOL vBool;
	HRESULT hr;
	bool result;

	result=false;

	if(m_pBrowser)
	{
		hr=m_pBrowser->get_Document(&pDispatch);
		if(SUCCEEDED_OK(hr) && pDispatch)
		{
			hr=pDispatch->QueryInterface(IID_IHTMLDocument2,(LPVOID*)ppHtmlDocument);
			if(SUCCEEDED_OK(hr) && (*ppHtmlDocument))
			{
				hr=(*ppHtmlDocument)->queryCommandEnabled(CComBSTR(L"Refresh"),&vBool);
				if(SUCCEEDED_OK(hr) && vBool==VARIANT_TRUE) result=true;
				else (*ppHtmlDocument)->Release();
			}
			
			pDispatch->Release();
		}
	}
	
	if(result==false) *ppHtmlDocument=NULL;
	
	return result;
}

IHTMLDocument2* CQToolbar::GetFrameDocument(IHTMLFramesCollection2 *pFramesCollection,int index)
{
	VARIANT vIdx;
	VARIANT vDispatch;
	IWebBrowser2* pBrowserFrame=NULL;
	IDispatch* pDispatch=NULL;
	IHTMLDocument2* pFrameDocument=NULL;
	IServiceProvider* pServiceProvider=NULL;
	HRESULT hr;

	vIdx.vt=VT_I4;
	vIdx.intVal=index;

	if(SUCCEEDED_OK(pFramesCollection->item(&vIdx,&vDispatch)) && vDispatch.vt==VT_DISPATCH)
	{
		hr=vDispatch.pdispVal->QueryInterface(IID_IServiceProvider,(LPVOID*)&pServiceProvider);
		if(SUCCEEDED_OK(hr) && pServiceProvider)
		{
			hr = pServiceProvider->QueryService(SID_SWebBrowserApp, IID_IWebBrowser2, (LPVOID*)&pBrowserFrame);
			if(SUCCEEDED_OK(hr) && pBrowserFrame)
			{
				if(SUCCEEDED_OK(pBrowserFrame->get_Document(&pDispatch)) && pDispatch)
				{
					if(pDispatch->QueryInterface(IID_IHTMLDocument2,(LPVOID*)&pFrameDocument)!=S_OK) pFrameDocument=NULL;
					pDispatch->Release();
				}
				pBrowserFrame->Release();
			}
			pServiceProvider->Release();
		}
		vDispatch.pdispVal->Release();
	}

	return pFrameDocument;
}

bool CQToolbar::FindText(IHTMLDocument2 *pHtmlDocument,TCHAR *pSearchText, long lFlags, BYTE findOptions,int depth)
{
	IHTMLElement *pBodyElm=NULL;
	IHTMLBodyElement *pBody=NULL;
	IHTMLTxtRange *pTxtRange=NULL;
	bool bSelectFailed;
	HRESULT hr;
	bool result;
	int direction;
	int i;

	result=false;
	if(pHtmlDocument && depth<MAXFRAMEDEPTH)
	{
		// Reset state
		if(findOptions&(FIND_First|FIND_Last))
		{
			Find_Occurence=0;
			if(depth==0) for(i=0;i<MAXFRAMEDEPTH;i++) Find_LastFrameDocument[i]=FRAME_UNDEFINED;
		}

		// Set find direction
		if(findOptions&(FIND_First|FIND_Next))
			direction=1;
		else
			direction=-1;

		if(Find_LastFrameDocument[depth]==FRAME_UNDEFINED)
		{
			Find_LastFrameDocument[depth]=(direction==1)?FRAME_THIS:FRAME_LAST;
		}
		
		// Search current frame
		
		if(Find_LastFrameDocument[depth]==FRAME_THIS)
		{
			hr=pHtmlDocument->get_body(&pBodyElm);

			if(SUCCEEDED_OK(hr) && pBodyElm)
			{
				hr=pBodyElm->QueryInterface(IID_IHTMLBodyElement,(LPVOID*)&pBody);	
				
				if(SUCCEEDED_OK(hr) && pBody)
				{
					hr=pBody->createTextRange(&pTxtRange);
		
					if(SUCCEEDED_OK(hr) && pTxtRange)
					{
						CComBSTR search(pSearchText);
						CComBSTR move_character(L"character");

						VARIANT_BOOL bFound;
						long t;
						 
						//If Word is less than 3 characters perform a "whole word" search
						if(search.Length()<3) lFlags|=2;

						// Calculate next occurence to find
						Find_Occurence+=direction;

						//QDEBUG_PRINTF(L"Find",L"%d %d",Find_Occurence,Find_LastFrameDocument[depth]);

						if(Find_Occurence!=0)
						{
							 // Find backwards?
							if(Find_Occurence<0) lFlags|=1;
							
							// Initialize start position
							pTxtRange->moveStart(move_character,-0xffffff,&t);

							// Skip last found occurences
							i=abs(Find_Occurence)-1;
							while(i>0)
							{
								pTxtRange->findText(search,0xffffff,lFlags,&bFound);
								if(Find_Occurence<0) pTxtRange->collapse(VARIANT_FALSE);
								pTxtRange->move(move_character,Find_Occurence>0?1:-1,&t);
								i--;
							}

							// Find next/previous
							do
							{
								bSelectFailed=false;
								hr=pTxtRange->findText(search,0xffffff,lFlags,&bFound);

								if(SUCCEEDED_OK(hr) && bFound==VARIANT_TRUE)
								{
									hr=pTxtRange->select();
									if(SUCCEEDED_OK(hr))
									{
										pTxtRange->select(); // IE8 workaround: the accelerator icon is only displayed on every second select call
										pTxtRange->scrollIntoView(VARIANT_FALSE);

										if(findOptions&FIND_Focus)
										{
											// Move focus to html window
											SetFocusOnParentWindow(pHtmlDocument);
										}

										result=true;
									}
									else // Select failed, find next/previous
									{
										if(Find_Occurence<0) pTxtRange->collapse(VARIANT_FALSE);
										pTxtRange->move(move_character,Find_Occurence>0?1:-1,&t);
										
										Find_Occurence+=direction;
										bSelectFailed=true;
									}
								}
								else Find_Occurence=0;

							} while(bSelectFailed && Find_Occurence!=0);
						}

						pTxtRange->Release();
					}
					pBody->Release();
				}
				pBodyElm->Release();
			}

			// If search direction is down, continue searching frames
			
			if(result==false)
			{
				Find_LastFrameDocument[depth]=(direction==1)?0:FRAME_UNDEFINED;
			}
		}

		// Search frames

		if(result==false && Find_LastFrameDocument[depth]>=0)
		{
			IHTMLFramesCollection2 *frames = NULL;
			hr=pHtmlDocument->get_frames(&frames);

			if(SUCCEEDED_OK(hr) && frames)
			{
				long n;

				i=Find_LastFrameDocument[depth];

				hr=frames->get_length(&n);
				if(SUCCEEDED_OK(hr))
				{
					IHTMLDocument2* pFrameDocument;

					if(i==FRAME_LAST) i=n-1; // i==FRAME_THIS if n==0
				
					while(i>=0 && i<n && result==false)
					{
						pFrameDocument=GetFrameDocument(frames,i);
						if(pFrameDocument)
						{
							result=FindText(pFrameDocument,pSearchText,lFlags,findOptions,depth+1);
							if(result) Find_LastFrameDocument[depth]=i;
							pFrameDocument->Release();
						}					
						i+=direction;
					}
				}

				frames->Release();

				// If search direction is up finally search in this frame
				if(result==false && i==FRAME_THIS) 
				{
					findOptions=FIND_Previous|(findOptions&~(FIND_Last)); // Prevent that Find_LastFrameDocument is reset

					Find_LastFrameDocument[depth]=FRAME_THIS;

					result=FindText(pHtmlDocument,pSearchText,lFlags,findOptions,depth);
				}

				// If text not found reset Find_LastFrameDocument of current level
				if(result==false) Find_LastFrameDocument[depth]=FRAME_UNDEFINED;

			} // End search frames
		} // End result==false
	} // End function arguments valid

	return result;
}

void CQToolbar::FindOnPage(BYTE initiatedBy,BYTE findOptions)
{
	IHTMLDocument2 *pHtmlDocument;
	bool blurFocus;
	BSTR bstrQuery;
	TCHAR *pFindText;
	VARIANT_BOOL bExec;
	VARIANT vEmpty;

	blurFocus=(findOptions&FIND_Focus)!=0;
	VariantInit(&vEmpty);

	if(GetHtmlDocument2(&pHtmlDocument))
	{
		pFindText=m_ComboQuero.GetFindText(bstrQuery);
		if(pFindText)
		{
			if(pFindText[0])
			{
				// Blur focus if user clicked on search icon
				if(initiatedBy==FIND_INITIATED_BY_SearchIcon)
				{
					if(m_ComboQuero.SendMessage(CB_GETDROPPEDSTATE,0,0)) m_ComboQuero.SendMessage(CB_SHOWDROPDOWN,FALSE,0);
					m_pBand->FocusChange(FALSE);
				}

				// Start new search if current find phrase differs from the last one
				if(StrCmpI(pFindText,LastFoundText)) ClearLastFoundText();

				// Update findOptions if LastFoundText is empty
				if(LastFoundText[0]==0)
				{
					findOptions|=(findOptions&(FIND_First|FIND_Next))?FIND_First:FIND_Last;
				}

				if(findOptions&(FIND_First|FIND_Last))
				{
					// Highlight find phrase if find phrase is not already highlighted
					if(Highlight && initiatedBy!=FIND_INITIATED_BY_QueroMenu && (nHighlightedWords!=1 || StrCmpI(pFindText,HighlightedWords[0])))
					{
						// Unhighlight HighlightedWords

						// Initialize HighlightedWords with find phrase
						StringCchCopy(HighlightedWords[0],MAXWORDLENGTH,pFindText);
						nHighlightedWords=1;

						// Unselect current selection
						pHtmlDocument->execCommand(CComBSTR(L"Unselect"),VARIANT_FALSE,vEmpty,&bExec);
					}
					else // Find first or last occurence
					{
						if(FindText(pHtmlDocument,pFindText,0,findOptions)) 
						{
							SetPhraseNotFound(false);
						}
						else // Phrase not found
						{
							pHtmlDocument->execCommand(CComBSTR(L"Unselect"),VARIANT_FALSE,vEmpty,&bExec);
							SetPhraseNotFound(true);
						}
					}

					if(PhraseNotFound)
					{
						if(blurFocus)
						{
							m_ComboQuero.SetFocus();
							::PostMessage(m_ComboQuero.m_hWndEdit,EM_SETSEL,0,-1);
						}
						PutStatusText(GetString(IDS_FIND_FINISHED));
						MessageBeep(MB_ICONEXCLAMATION);
						ClearLastFoundText();
					}
					else StringCbCopy(LastFoundText,sizeof LastFoundText,pFindText);
				}
				else // Find next or previous occurence
				{
					if(!FindText(pHtmlDocument,LastFoundText,0,findOptions))
					{
						// Phrase not found
						MessageBeep(MB_ICONEXCLAMATION);
						PutStatusText(GetString(IDS_FIND_FINISHED));

						// Try to search from beginning or end of the document
						findOptions|=(findOptions&FIND_Next)?FIND_First:FIND_Last;

						if(!FindText(pHtmlDocument,LastFoundText,0,findOptions))
						{
							// Phrase not in the document
							pHtmlDocument->execCommand(CComBSTR(L"Unselect"),VARIANT_FALSE,vEmpty,&bExec);
							if(blurFocus)
							{
								m_ComboQuero.SetFocus();
								::PostMessage(m_ComboQuero.m_hWndEdit,EM_SETSEL,0,-1);
							}
							ClearLastFoundText();
							SetPhraseNotFound(true);
						}
						else SetPhraseNotFound(false);
					}
					else SetPhraseNotFound(false);
				}
			}

			SysFreeString(bstrQuery);
		}

		pHtmlDocument->Release();
	}
	else // GetHtmlDocument2 failed
	{
		if(blurFocus)
		{
			m_ComboQuero.SetFocus();
			::PostMessage(m_ComboQuero.m_hWndEdit,EM_SETSEL,0,-1);
		}
		MessageBeep(MB_ICONEXCLAMATION);
	}
}

bool CQToolbar::IsFocusOnInput(IHTMLDocument2 *pHtmlDocument)
{
	IHTMLElement *pHtmlElement=NULL;
	IElementBehavior* pElementBehavior;
	VARIANT vEventHandler;
	HRESULT hr;
	bool result=false;

	int ElementType;

	hr=pHtmlDocument->get_activeElement(&pHtmlElement);
	if(SUCCEEDED_OK(hr) && pHtmlElement)
	{
		// Check if focus is on an input element or on an embedded object (e.g. Flash, Java Applet)
		
		ElementType=GetElementType(pHtmlElement);


		if(ElementType&(ELEMENT_TYPE_INPUT|ELEMENT_TYPE_TEXTAREA|ELEMENT_TYPE_SELECT|ELEMENT_TYPE_APPLET|ELEMENT_TYPE_OBJECT|ELEMENT_TYPE_EMBED))
		{
			result=true;
		}
		else
		{
			// Call IsFocusOnInput recursivley on FRAME or IFRAME elements

			if(ElementType&(ELEMENT_TYPE_FRAME|ELEMENT_TYPE_IFRAME))
			{
				IHTMLDocument2* pFrameDocument;

				pFrameDocument=GetFrameDocument(ElementType,pHtmlElement);
				if(pFrameDocument)
				{
					result=IsFocusOnInput(pFrameDocument);
					pFrameDocument->Release();
				}
			}

			// Check if active element is editable

			if(result==false)
			{
				IHTMLElement3 *pHtmlElement3;
				if(SUCCEEDED_OK(pHtmlElement->QueryInterface(IID_IHTMLElement3,(LPVOID*)&pHtmlElement3)))
				{
					VARIANT_BOOL vIsContentEditable;

					if(SUCCEEDED_OK(pHtmlElement3->get_isContentEditable(&vIsContentEditable)))
						if(vIsContentEditable==VARIANT_TRUE) result=true;		

					pHtmlElement3->Release();
				}
			}

			// Check if active element has DHTML behavior attached (detects editor in Outlook Web Access)

			if(result==false && SUCCEEDED_OK(pHtmlElement->QueryInterface(IID_IElementBehavior,(LPVOID*)&pElementBehavior)))
			{
				pElementBehavior->Release();
				result=true;
			}

			// Check if keyboard event handler is installed on active element

			if(result==false && SUCCEEDED_OK(pHtmlElement->get_onkeypress(&vEventHandler))) // get_onkey* succeeds even though no custom event handler is attached
			{
				if(vEventHandler.vt==VT_DISPATCH && vEventHandler.pdispVal)
				{
					vEventHandler.pdispVal->Release();
					result=true;
				}
				else if(SUCCEEDED_OK(pHtmlElement->get_onkeydown(&vEventHandler)))
				{
					if(vEventHandler.vt==VT_DISPATCH && vEventHandler.pdispVal)
					{
						vEventHandler.pdispVal->Release();
						result=true;
					}
					else if(SUCCEEDED_OK(pHtmlElement->get_onkeyup(&vEventHandler)))
					{
						if(vEventHandler.vt==VT_DISPATCH && vEventHandler.pdispVal)
						{
							vEventHandler.pdispVal->Release();
							result=true;
						}
					}
				}
			}
		} // End focus is not on input element

		pHtmlElement->Release();
	} // End active element

	return result;
}

int CQToolbar::GetElementType(IHTMLElement *pHtmlElement)
{
	BSTR tagName=NULL;
	int ElementType;
	int i;
	HRESULT hr;

	hr=pHtmlElement->get_tagName(&tagName);
	if(SUCCEEDED_OK(hr) && tagName)
	{

		const static TCHAR *TagNames[N_ELEMENT_TYPES]={L"input",L"textarea",L"select",L"applet",L"object",L"embed",L"frame",L"iframe"};

		for(i=0;i<N_ELEMENT_TYPES;i++)
		{
			if(!StrCmpI(tagName,TagNames[i])) break;
		}

		if(i<N_ELEMENT_TYPES) ElementType=1<<i;
		else ElementType=ELEMENT_TYPE_UNKNOWN;

		SysFreeString(tagName);
	}
	else
	{
		ElementType=ELEMENT_TYPE_UNKNOWN;
	}

	return ElementType;
}

IHTMLDocument2* CQToolbar::GetFrameDocument(int ElementType,IHTMLElement *pHtmlElement)
{
	IUnknown *pFrameElement=NULL;
	IWebBrowser2* pBrowserFrame=NULL;
	IDispatch* pDispatch=NULL;
	IHTMLDocument2* pFrameDocument;
	HRESULT hr;

	pFrameDocument=NULL;

	hr=pHtmlElement->QueryInterface((ElementType&ELEMENT_TYPE_FRAME)?IID_IHTMLFrameElement:IID_IHTMLIFrameElement,(LPVOID*)&pFrameElement);
	if(SUCCEEDED_OK(hr) && pFrameElement)
	{
		hr=pFrameElement->QueryInterface(IID_IWebBrowser2,(LPVOID*)&pBrowserFrame);
		if(SUCCEEDED_OK(hr) && pBrowserFrame)
		{
			hr=pBrowserFrame->get_Document(&pDispatch);
			if(SUCCEEDED_OK(hr) && pDispatch)
			{
				hr=pDispatch->QueryInterface(IID_IHTMLDocument2,(LPVOID*)&pFrameDocument);
				if(hr!=S_OK) pFrameDocument=NULL;

				pDispatch->Release();
			}
			pBrowserFrame->Release();
		}
		pFrameElement->Release();
	}

	return pFrameDocument;
}

HWND CQToolbar::GetIEFrameWindow()
{
	return (QueroInstanceId!=UNASSIGNED_INSTANCE_ID)?QThreadLocalStg[QueroInstanceId].hIEWnd:GetAncestor(m_hWnd,GA_ROOT);
}

bool CQToolbar::IsIEFrameWindow(HWND hwnd_IEFrame)
{
	bool result;
	TCHAR className[32];

	if(GetClassName(hwnd_IEFrame,className,32))
	{
		result = (StrCmp(className,L"IEFrame")==0);
	}
	else result=false;

	return result;
}

void CQToolbar::IEFrame_Changed()
{
	HWND hwnd_old_IEFrame;
	HWND hwnd_new_IEFrame;

	if(QueroInstanceId!=UNASSIGNED_INSTANCE_ID)
	{
		hwnd_old_IEFrame=QThreadLocalStg[QueroInstanceId].hIEWnd;
		hwnd_new_IEFrame=GetAncestor(m_hWnd,GA_ROOT);
		if(hwnd_old_IEFrame!=hwnd_new_IEFrame)
		{
			if(WaitForSingleObject(g_hQSharedDataMutex,QMUTEX_TIMEOUT)==WAIT_OBJECT_0)
			{
				bool bWindowClosed;
				bool bNewWindow;
				int i;

				bWindowClosed=true;
				bNewWindow=true;

				for(i=0;i<=g_MaxUsedInstanceId;i++)
				{
					if(i!=QueroInstanceId)
					{
						if(QThreadLocalStg[i].hIEWnd==hwnd_old_IEFrame) bWindowClosed=false;
						if(QThreadLocalStg[i].hIEWnd==hwnd_new_IEFrame) bNewWindow=false;
					}
				}

				QThreadLocalStg[QueroInstanceId].hIEWnd=hwnd_new_IEFrame;
				QThreadLocalStg[QueroInstanceId].ThreadId_IEWnd=::GetWindowThreadProcessId(hwnd_new_IEFrame,NULL);
				QThreadLocalStg[QueroInstanceId].bNewWindow=bNewWindow;

				if(pQueroBroker)
				{
					if(bWindowClosed) pQueroBroker->Unhook_IEFrame(HandleToLong(hwnd_old_IEFrame));
					if(bNewWindow) pQueroBroker->Hook_IEFrame(HandleToLong(hwnd_new_IEFrame),HandleToLong(m_hWnd),g_Options,g_Options2,g_IE_MajorVersion);
				}
				
				ReleaseMutex(g_hQSharedDataMutex);
			}
		} // End hwnd_old_IEFrame!=hwnd_new_IEFrame
	}
}

HWND CQToolbar::GetIETabWindow()
{
	HWND hWnd;
	IServiceProvider *pServiceProvider;
	IOleWindow* pOleWindow;
	HRESULT hr;

	hWnd=NULL;

	if(m_pBrowser)
	{
		hr=m_pBrowser->QueryInterface(IID_IServiceProvider,(LPVOID*)&pServiceProvider);
		if(SUCCEEDED_OK(hr))
		{
			hr=pServiceProvider->QueryService(SID_SShellBrowser,IID_IOleWindow,(LPVOID*)&pOleWindow);
			if(SUCCEEDED_OK(hr))
			{
				hWnd=NULL;
				hr=pOleWindow->GetWindow(&hWnd);
				if(FAILED(hr)) hWnd=NULL;

				pOleWindow->Release();
			}
			pServiceProvider->Release();
		}
	}

	return hWnd;
}

void CQToolbar::PostCommandToIE(WPARAM wParam,bool bIEFrame)
{
	HWND hWnd;

	#ifdef COMPILE_FOR_WINDOWS_VISTA
		if(bIEFrame)
		{
			hWnd=GetIEFrameWindow();
			if(pQueroBroker && hWnd) pQueroBroker->PostMessageToIE(HandleToLong(hWnd),WM_COMMAND,wParam,NULL);
		}
		else
		{
			hWnd=GetIETabWindow();
			if(hWnd) ::PostMessage(hWnd,WM_COMMAND,wParam,NULL);
		}
	#else
		if(bIEFrame || g_IE_MajorVersion<7) hWnd=GetIEFrameWindow();
		else hWnd=GetIETabWindow();

		if(hWnd) ::PostMessage(hWnd,WM_COMMAND,wParam,NULL);
	#endif
}

HRESULT CQToolbar::SetFocusOnIEServerWindow()
{
	HWND hWnd;
	HRESULT hr;

	hr=E_FAIL;

	hWnd=GetIETabWindow();
	if(hWnd)
	{
		hWnd=FindWindowEx(hWnd,NULL,L"Shell DocObject View",NULL);
		if(hWnd)
		{
			hWnd=FindWindowEx(hWnd,NULL,L"Internet Explorer_Server",NULL);
			if(hWnd)
			{
				::SetFocus(hWnd);
				hr=S_OK;
			}
		}
	}

	return hr;
}

HRESULT CQToolbar::SetFocusOnParentWindow(IHTMLDocument2 *pHtmlDocument)
{
	IHTMLWindow2 *pHtmlWindow=NULL;
	HRESULT hr;

	hr=pHtmlDocument->get_parentWindow(&pHtmlWindow);
	if(SUCCEEDED_OK(hr) && pHtmlWindow)
	{
		hr=pHtmlWindow->focus();
		pHtmlWindow->Release();
	}
	else hr=E_FAIL;

	return hr;
}

bool CQToolbar::ZoomPage(UINT zoom,BYTE method)
{
	IHTMLDocument2 *pHtmlDocument;
	IHTMLStyleSheet *pStyleSheet=NULL;
	IHTMLElement *pHtmlElement=NULL;
	IHTMLStyle *pHtmlStyle=NULL;
	IHTMLStyle3 *pHtmlStyle3=NULL;
	HRESULT hr;
	bool result=false;

	VARIANT factor;

	TCHAR style[128];

	switch(method)
	{
	case ZOOMPAGE_PUT_CSSTEXT:
		if(GetHtmlDocument2(&pHtmlDocument))
		{
			if(pHtmlDocument->createStyleSheet(NULL,0,&pStyleSheet)==S_OK && pStyleSheet)
			{
				StringCbPrintf(style,sizeof style,L"body,frameset {zoom:%d%%}",ZoomFactor);
				if(SUCCEEDED_OK(pStyleSheet->put_cssText(CComBSTR(style)))) result=true;

				pStyleSheet->Release();
			}
			pHtmlDocument->Release();
		}
		break;

	case ZOOMPAGE_PUT_ZOOM:
		if(GetHtmlDocument2(&pHtmlDocument))
		{
			hr=pHtmlDocument->get_body(&pHtmlElement);
			if(SUCCEEDED_OK(hr) && pHtmlElement)
			{
				hr=pHtmlElement->get_style(&pHtmlStyle);
				if(SUCCEEDED_OK(hr) && pHtmlStyle)
				{
					hr=pHtmlStyle->QueryInterface(IID_IHTMLStyle3,(LPVOID*)&pHtmlStyle3);
					if(SUCCEEDED_OK(hr) && pHtmlStyle3)
					{
						StringCbPrintf(style,sizeof style,L"%d%%",zoom);

						factor.vt=VT_BSTR;
						factor.bstrVal=SysAllocString(style);
						if(factor.bstrVal)
						{							
							if(SUCCEEDED_OK(pHtmlStyle3->put_zoom(factor))) result=true;
							SysFreeString(factor.bstrVal);
						}
						pHtmlStyle3->Release();
					}
					pHtmlStyle->Release();
				}
				pHtmlElement->Release();
			}	
			pHtmlDocument->Release();
		}
		else hr=E_FAIL;
		break;

	case ZOOMPAGE_SET_OPTICAL_ZOOM:
		if(m_pBrowser)
		{
			factor.vt=VT_I4;
			factor.lVal=zoom;
			if(SUCCEEDED_OK(m_pBrowser->ExecWB(OLECMDID_OPTICAL_ZOOM,OLECMDEXECOPT_DONTPROMPTUSER,&factor,NULL))) result=true;
		}
		break;
	}

	return result;
}

void CQToolbar::SetZoomFactor(UINT NewZoomFactor,bool bUpdateZoomFactor)
{
	if(NewZoomFactor<ZOOMFACTOR_MIN) NewZoomFactor=ZOOMFACTOR_MIN;
	else if(NewZoomFactor>ZOOMFACTOR_MAX) NewZoomFactor=ZOOMFACTOR_MAX;

	ZoomPage(NewZoomFactor,UseOpticalZoom()?ZOOMPAGE_SET_OPTICAL_ZOOM:ZOOMPAGE_PUT_ZOOM);

	if(bUpdateZoomFactor)
	{
		ZoomFactor=NewZoomFactor;
		SaveSettingsValue(SETTINGS_VALUES_ZOOMFACTOR,ZoomFactor);
	}
}

HRESULT CQToolbar::GetElementCollection(BSTR pTagName,IHTMLDocument2* pHtmlDocument,IHTMLElementCollection** ppElementColl)
{
	HRESULT hr;
	IHTMLElementCollection* pElementCollAll;

	hr=pHtmlDocument->get_all(&pElementCollAll);
	if(SUCCEEDED_OK(hr) && pElementCollAll)
	{
		IDispatch* pTagsDisp;
		VARIANT vTagName;

		vTagName.vt=VT_BSTR;
		vTagName.bstrVal=pTagName;

		hr=pElementCollAll->tags(vTagName,&pTagsDisp);
		if(SUCCEEDED_OK(hr) && pTagsDisp)
		{
			hr=pTagsDisp->QueryInterface(IID_IHTMLElementCollection,(LPVOID*)ppElementColl);
			pTagsDisp->Release();
		}

		pElementCollAll->Release();
	}
	return hr;
}

// Retrieve the last searched keywords
void CQToolbar::InitWords(TCHAR Words[MAXWORDS][MAXWORDLENGTH],UINT *nWords,BYTE options,BSTR *pbstrQuery)
{
	BSTR bstrQuery = NULL;
	TCHAR* pPhrase = NULL;
	HistoryEntry* lastHistoryEntry;

	// Take the last searched keywords from the search box
	if((options&INITWORDS_SearchBox) && currentType==TYPE_SEARCH && !m_ComboQuero.bIsEmptySearch)
	{
		pPhrase=m_ComboQuero.GetFindText(bstrQuery);
	}

	// Take the last searched keywords from the last history entry
	if(pPhrase==NULL && (options&INITWORDS_History))
	{
		lastHistoryEntry=GetLastHistoryEntry();
		
		// Use the last history entry only if it was created after the current IE process was launched
		if(lastHistoryEntry && lastHistoryEntry->Type==TYPE_SEARCH && IsMoreRecent_Than(lastHistoryEntry->Timestamp,g_QueroStartTime))
		{
			pPhrase=lastHistoryEntry->Query;
		}
	}

	// Split the query into words and return a copy of the string if pbstrQuery was specified
	if(pPhrase)
	{
		*nWords=SplitIntoWords(pPhrase,Words,options);
		if(pbstrQuery) *pbstrQuery=SysAllocString(pPhrase);
	}
	else
	{
		*nWords=0;
		if(pbstrQuery) *pbstrQuery=NULL;
	}

	// Free bstrQuery returned by GetFindText
	if(bstrQuery) SysFreeString(bstrQuery);
}

void CQToolbar::OnProgressChange(int progress)
{
	LastProgress=progress;
}

void CQToolbar::OnDocumentComplete()
{
	Searching=false;
}

LRESULT CQToolbar::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{

	switch(wParam)
	{
	case ID_COMBOENGINE_HOVER_TIMER:
		LRESULT index;
		POINT point;

		if(::IsWindowVisible(m_ComboEngine.m_hWndList))
		{
			GetCursorPos(&point);
			::ScreenToClient(m_ComboEngine.m_hWndList,&point);
			index=::SendMessage(m_ComboEngine.m_hWndList,LB_ITEMFROMPOINT,0,MAKELPARAM(point.x,point.y));

			if(chooseProfile==false)
			{
				if((UINT)index==LastHighlightedItemIndex)
				{
					Times_LastHighlightedItem_Identical++;
				}
				else
				{
					Times_LastHighlightedItem_Identical=0;
				}

				if(Times_LastHighlightedItem_Identical>HOVER_INTERVALS_BEFORE_SELECTION)
				{
					LastHighlightedItemIndex=INDEX_UNDEFINED;
					Times_LastHighlightedItem_Identical=0;

					if(index==0)
					{
						m_ComboEngine.SendMessage(CB_SETCURSEL,0,0);
						m_ComboEngine.SendMessage(WM_KEYDOWN,VK_RETURN,0);
					}
				}
			}
			else
			{
				LastHighlightedItemIndex=INDEX_UNDEFINED;
				Times_LastHighlightedItem_Identical=0;
			}
		}
		else KillTimer(ID_COMBOENGINE_HOVER_TIMER);
		break;

	case ID_COMBOQUERO_TOOLTIP_TIMER:
		m_ComboQuero.ShowToolTip(0,false);
		KillTimer(ID_COMBOQUERO_TOOLTIP_TIMER);
		break;

	}

	bHandled=TRUE;
	return 0;
}

LRESULT CQToolbar::OnMenuSelect(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if(hPopupMenu && (HMENU)lParam==hPopupMenu)
	{
		int index=LOWORD(wParam);
		UINT ids=0;

		if(*LastFoundText!=_T('\0') && HIWORD(wParam)&MF_POPUP) index--;

		switch(index)
		{
		case 0:
			if(HIWORD(wParam)&MF_POPUP) ids=IDS_HELP_FINDONPAGE;
			break;
		case 7:
			if(HIWORD(wParam)&MF_POPUP) ids=IDS_HELP_SELECTPROFILE;
			break;
		case 8:
			if(HIWORD(wParam)&MF_POPUP) ids=IDS_HELP_SELECTDEFAULT;
			break;
		case 9:
			if(HIWORD(wParam)&MF_POPUP) ids=IDS_HELP_RESIZE;
			break;
		case 10:
			if(HIWORD(wParam)&MF_POPUP) ids=IDS_HELP_ZOOM;
			break;
		}
		
		PutStatusText(ids?GetString(ids):NULL);
	}
	return 0;
}

void CQToolbar::PutStatusText(const TCHAR *pText)
{
	if(m_pBrowser) m_pBrowser->put_StatusText(pText?CComBSTR(pText):CComBSTR());
}

LRESULT CQToolbar::OnCtlColorEdit(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if(g_QueroTheme_DLL && g_ThemeColors[COLOR_Background]!=COLOR_UNDEFINED) return (LRESULT)hDefaultBackground;

	/*else if((HWND)lParam == m_ComboEngine.m_hWnd)
	{	
		if(chooseProfile || ::SendMessage(m_ComboEngine.m_hWnd,CB_GETCURSEL,0,0)==0)
		{
			bHandled=TRUE;
			return (LRESULT)hProfileBackground;
		}
	}*/

	bHandled=FALSE;
	return 0;
}

void CQToolbar::CopyWords(TCHAR WordsDest[MAXWORDS][MAXWORDLENGTH],UINT *nWordsDest,TCHAR WordsSrc[MAXWORDS][MAXWORDLENGTH],UINT nWordsSrc)
{
	UINT i,j;

	i=0;
	while(i<nWordsSrc)
	{
		j=0;
		while(WordsSrc[i][j])
		{
			WordsDest[i][j]=WordsSrc[i][j];
			j++;
		}
		WordsDest[i][j]=_T('\0');
		i++;
	}
	*nWordsDest=nWordsSrc;
}

int CQToolbar::MeasureEngineWidth()
{
	HDC hDC;
	UINT i;
	int max;
	SIZE sz;
	BSTR pName;

	max=0;

	hDC=GetDC();
	if(hDC)
	{
		SelectObject(hDC,hFontBold);

		pName=m_Profiles.GetProfileName(CurrentProfileId);
		if(pName) if(GetTextExtentPoint32(hDC,pName,SysStringLen(pName),&sz)) max=sz.cx;
			
		for(i=0;i<nengines;i++)
		{
			pName=m_Profiles.CurrentProfile.Engines[i].Name;
			if(pName) if(GetTextExtentPoint32(hDC,pName,SysStringLen(pName),&sz) && sz.cx>max) max=sz.cx;
		}

		ReleaseDC(hDC);
	}

	max+=g_Scaled_IconSize + 11 + m_ComboEngine.m_rcButton.right-m_ComboEngine.m_rcButton.left;
	if(max<MIN_ENGINEWIDTH) max=MIN_ENGINEWIDTH;
	if(max>MAX_ENGINEWIDTH) max=MAX_ENGINEWIDTH;

	return max;
}

int CQToolbar::GetToolbarMinWidth()
{
	int MinWidth;

	MinWidth=0;
	if(g_Options2&OPTION2_ShowSearchBox) MinWidth+=180;
	else
	{
		if(g_Options&OPTION_ShowSearchEngineComboBox)
		{
			MinWidth+=LOGOGAP+IdealEngineWidth;
		}
	}

	return MinWidth;
}

#define N_ACCELERATORS 17
#define ACCELERATOR_QueroKey 0
#define ACCELERATOR_QueroMenu 1
#define ACCELERATOR_QuickTabs 2
#define ACCELERATOR_Slash 12
#define ACCELERATOR_CtrlSlash 13

LRESULT CQToolbar::OnKeyboardHook_IEFrame(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	return OnKeyboardHook(HC_ACTION,wParam,lParam,true);
}

LRESULT CQToolbar::OnKeyboardHook(int nCode, WPARAM wParam, LPARAM lParam,bool bIEFrame)
{
	LRESULT result=0;

	if(nCode==HC_ACTION)
	{
		if((lParam&0x80000000)==0) // Key pressed?
		{
			BYTE CtrlAltShiftKey;
			UINT ShortcutKey;
			UINT QueroKey;
			UINT i;
			SHORT VkSlash;

			CtrlAltShiftKey=KEY_None;

			if(GetKeyState(VK_CONTROL)<0) CtrlAltShiftKey|=KEY_Ctrl;
			if(GetKeyState(VK_MENU)<0) CtrlAltShiftKey|=KEY_Alt;
			if(GetKeyState(VK_SHIFT)<0) CtrlAltShiftKey|=KEY_Shift;
			
			VkSlash=VkKeyScan(L'/'); // Scan code and shift state for the slash character in current keyboard layout

			// Determine if and which shortcut key was pressed

			QueroKey=OPTION_QueroShortcutKey(g_Options);

			ACCEL Accelerators[N_ACCELERATORS]={
				{FCONTROL,L'Q',KEY_Quero},
				{FALT,L'Q',KEY_QueroMenu},
				{FCONTROL|FSHIFT,L'Q',KEY_QuickTabs},
				{0,VK_F3,KEY_F3},
				{FSHIFT,VK_F3,KEY_F3},
				{0,VK_F2,KEY_F2},
				{FSHIFT,VK_F2,KEY_F2},
				{FCONTROL,VK_DELETE,KEY_CtrlDel},
				{FCONTROL,VK_CANCEL,KEY_CtrlPause},
				{FCONTROL,L'U',KEY_ViewSource},
				{FCONTROL|FSHIFT,L'B',KEY_ViewBlockedContent},
				{FALT|FSHIFT,L'H',KEY_Highlight},
				{0,LOBYTE(VkSlash),KEY_Slash},
				{FCONTROL,LOBYTE(VkSlash),KEY_CtrlSlash},
				{0,VK_F4,KEY_F4},
				{FCONTROL,VK_BACK,KEY_CtrlBack},
				{FCONTROL,L'L',KEY_CtrlL}
			};

			if(QueroKey!=QKEY_CtrlQ)
			{
				Accelerators[ACCELERATOR_QueroKey].fVirt=FALT;
				Accelerators[ACCELERATOR_QueroKey].key=QKEY_CharMap[QueroKey];
				if(QueroKey==QKEY_AltQ)	Accelerators[ACCELERATOR_QueroMenu].fVirt=FALT|FSHIFT;
				Accelerators[ACCELERATOR_QuickTabs].cmd=KEY_None; // Disable KEY_QuickTabs
			}
			if(VkSlash&0x0100) // Shift key needed for typing /
			{
				Accelerators[ACCELERATOR_Slash].fVirt=FSHIFT;
				Accelerators[ACCELERATOR_CtrlSlash].fVirt=FCONTROL|FSHIFT;
			}
			if((g_Options&OPTION_EnableQuickType)==0)
			{
				Accelerators[ACCELERATOR_Slash].cmd=KEY_None;
				Accelerators[ACCELERATOR_CtrlSlash].cmd=KEY_None;
			}

			i=0;
			while(i<N_ACCELERATORS && (wParam!=Accelerators[i].key || CtrlAltShiftKey!=Accelerators[i].fVirt)) i++;

			if(i<N_ACCELERATORS) ShortcutKey=Accelerators[i].cmd;
			else if(MapVirtualKey((UINT)wParam,2)>0x20 && ((CtrlAltShiftKey&(KEY_Ctrl|KEY_Alt))==KEY_None || (CtrlAltShiftKey&(KEY_Ctrl|KEY_Alt))==(KEY_Ctrl|KEY_Alt)) && (g_Options&OPTION_EnableQuickType)) // Printable character key pressed?
			{
				ShortcutKey=KEY_Char;
				// QDEBUG_PRINTF(L"KEY_Char pressed",L"%c ; %c",wParam,MapVirtualKeyEx(wParam,2,GetKeyboardLayout(0)));
			}
			else ShortcutKey=KEY_None;

			// Handle shortcut key

			if(ShortcutKey && ((1<<ShortcutKey)&g_Keys))
			{
				HWND focus=GetFocus();

				if(::IsChild(GetIEFrameWindow(),focus))
				{
					if(IsPopUpMenuVisible()==false)
					{
						HWND hWndEdit=m_ComboQuero.m_hWndEdit;
						bool FocusOnInput;
						BYTE findOptions;

						result=1; // Do not call next hook

						switch(ShortcutKey)
						{
						case KEY_Slash:
						case KEY_CtrlSlash:
						case KEY_Char:
						case KEY_CtrlBack:
							if((g_Options2&OPTION2_ShowSearchBox)==0 && ShortcutKey!=KEY_CtrlBack)
							{
								FocusOnInput=true;
							}
							else if(ShortcutKey==KEY_Char || ShortcutKey==KEY_Slash || ShortcutKey==KEY_CtrlBack)
							{
								// Check whether IE Frame hook is invoked or focus is on a toolbar
								if(bIEFrame)
								{
									FocusOnInput=false;
								}
								else if(::IsChild(m_pBand->GetParentWindow(),focus))
								{
									FocusOnInput=true;
								}
								else
								{	
									IHTMLDocument2 *pHtmlDocument;

									// Assume focus is on input
									FocusOnInput=true;
								
									// Test if focus is really in the main html window and not in a side bar
									if(GetHtmlDocument2(&pHtmlDocument))
									{
										IOleWindow *pOleWindow=NULL;
										HRESULT hr;

										hr=pHtmlDocument->QueryInterface(IID_IOleWindow,(LPVOID*)&pOleWindow);
										if(SUCCEEDED_OK(hr) && pOleWindow)
										{
											HWND winhwnd;

											pOleWindow->GetWindow(&winhwnd);
											if(winhwnd==focus || ::IsChild(winhwnd,focus))
											{
												FocusOnInput=IsFocusOnInput(pHtmlDocument);
											}
											pOleWindow->Release();
										}
										pHtmlDocument->Release();
									}						
								}
							}
							else FocusOnInput=false; // KEY_CtrlSlash

							if(FocusOnInput==false)
							{
								if(ShortcutKey==KEY_CtrlBack)
								{
									NavigateUp(OPEN_SameWindow);
								}
								else
								{
									m_ComboQuero.SetText(L"",TYPE_SEARCH,NULL,false);
							
									// Set focus

									if(bIEFrame) SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_QTOOLBAR,0); // Workaround if IE7 address bar drop-down is opened (steals focus)
									m_ComboQuero.GetEditCtrl()->bSelectText=false;
									SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_QEDITCTRL,0);

									// Fake keyboard message (SendInput does not guarantee that the sent key appears first in the edit box)

									if(ShortcutKey==KEY_CtrlSlash)
									{
										::PostMessage(hWndEdit,WM_CHAR,L'/',0x0);									
									}
									else
									{
										MSG msg;

										msg.hwnd=hWndEdit;
										msg.message=WM_KEYDOWN;
										msg.wParam=wParam;
										msg.lParam=lParam;								
										msg.time=GetMessageTime();
										TranslateMessage(&msg);
										DispatchMessage(&msg);
									}
								}
							}
							else result=0; // Call next hook
							break;
						
						case KEY_Quero:
						case KEY_F4:
							if(g_Options2&OPTION2_ShowSearchBox)
							{
								if(focus==hWndEdit)
								{
									if(ShortcutKey==KEY_Quero) PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_IESERVERWINDOW,0);
								}
								else
								{
									if(bIEFrame) SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_QTOOLBAR,0); // Workaround if IE7 address bar drop-down is opened (steals focus)
									SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_QEDITCTRL,0);
									SendMessage(hWndEdit,EM_SETSEL,0,-1);
								}
							}
							else if(g_Options&OPTION_ShowSearchEngineComboBox)
							{
								if(focus==m_ComboEngine.m_hWnd)
								{
									if(ShortcutKey==KEY_Quero) PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_IESERVERWINDOW,0);
								}
								else
								{
									if(bIEFrame) SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_QTOOLBAR,0); // Workaround if IE7 address bar drop-down is opened (steals focus)
									SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_COMBOENGINE,0);
								}
							}
							else if(ShortcutKey==KEY_Quero) PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SHOWTOOLBAR,1);
							else result=0;
							break;

						case KEY_QueroMenu:
							PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SHOWQUEROMENU,0);
							break;

						case KEY_F3:
						case KEY_F2:
							if(g_Options2&OPTION2_ShowSearchBox)
							{
								findOptions=(ShortcutKey==KEY_F3)?FIND_Next:FIND_Previous;
								if(CtrlAltShiftKey==KEY_Shift) findOptions=(findOptions==FIND_Next)?FIND_Previous:FIND_Next; // Swap direction if shift is pressed

								if(focus!=hWndEdit)
								{
									// If this is a new search clear the search box and set focus in it
									if(LastFoundText[0]==0)
									{
										m_ComboQuero.SetText(L"",TYPE_SEARCH,NULL,false);
										m_ComboQuero.GetEditCtrl()->bSelectText=false;
										if(bIEFrame) SetFocus();
										SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_QEDITCTRL,0);
										findOptions=0;
									}
									else findOptions|=FIND_Focus;
								}
								
								if(findOptions) PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_FINDONPAGE,findOptions);
							}
							else result=0; // Search box hidden
							break;

						case KEY_CtrlDel:
							PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_HIDEFLASHADS,0);
							break;

						case KEY_CtrlPause:							
							PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_TOGGLE_AD_BLOCKER,0);
							break;

						case KEY_Highlight:
							PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETHIGHLIGHT,(LPARAM)!Highlight);
							break;

						case KEY_ViewSource:
							PostCommandToIE(IE_COMMAND_VIEWSOURCE,false);
							break;

						case KEY_ViewBlockedContent:
							PostMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_VIEW_BLOCKED_CONTENT,0);
							break;

						case KEY_QuickTabs:
							if(g_IE_MajorVersion>=7)
								PostCommandToIE(IE_COMMAND_QUICKTABS,true);
							else
								result=0;
							break;

						case KEY_CtrlL:
							if((g_Options2&OPTION2_ShowSearchBox) &&
								((g_Options2&OPTION2_HideAddressBox) || (g_Options&OPTION_HideNavigationBar) || (g_IE_MajorVersion<=6)) &&
								focus!=hWndEdit)
							{
								SendMessage(WM_QUERO_TOOLBAR_COMMAND,QUERO_COMMAND_SETFOCUS_QEDITCTRL,0);
							}
							else result=0;
							break;
						}
					} // End bPopUpMenuVisible==false
				} // End IsChild
			} // End Quero Key
		} // End Key pressed
		else
		{
			if(wParam==VK_MENU && g_IgnoreAltKeyUpOnce)
			{
				g_IgnoreAltKeyUpOnce=false;
				result=1; // Do not call next hook
			}
		}
	} // End HC_ACTION

	return result;
}

bool CQToolbar::IsPopUpMenuVisible()
{
	bool bPopUpMenuVisible;
	HWND hWndTop;
	LONG style;
	TCHAR ClassName[32];

	// Check whether popup menu is visible
	bPopUpMenuVisible=false;
	hWndTop=::GetTopWindow(NULL);

	style=::GetWindowLong(hWndTop,GWL_STYLE);
	if((style&WS_POPUPWINDOW) && ::IsWindowVisible(hWndTop))
	{
		if(GetClassName(hWndTop,ClassName,32))
		{
			if(!StrCmp(ClassName,L"#32768") || !StrCmp(ClassName,L"BaseBar")) // BaseBar: Favorites menu,toolbar chevron menu
			{
				bPopUpMenuVisible=true;
			}
		}
	}

	return bPopUpMenuVisible;
}

LRESULT CQToolbar::OnQueroToolbarCommand(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LRESULT result;

	result=0;

	switch(wParam)
	{
	case QUERO_COMMAND_FINDONPAGE:
		FindOnPage(FIND_INITIATED_BY_FKey,(BYTE)lParam);
		break;

	case QUERO_COMMAND_HIDEFLASHADS:
		IHTMLDocument2 *pHtmlDocument;

		if(GetHtmlDocument2(&pHtmlDocument))
		{
			pHtmlDocument->Release();
		}
		break;

	case QUERO_COMMAND_SETFOCUS_QEDITCTRL:
		ShowToolbarIfHidden();
		if(g_Options2&OPTION2_ShowSearchBox) ::SetFocus(m_ComboQuero.m_hWndEdit);
		break;

	case QUERO_COMMAND_SETFOCUS_QTOOLBAR:
		SetFocus();
		break;

	case QUERO_COMMAND_SETFOCUS_IESERVERWINDOW:
		SetFocusOnIEServerWindow();
		break;

	case QUERO_COMMAND_SHOW_OPERATION_LOCKED_ERROR:
		::MessageBox(GetActiveWindow(),GetString(IDS_ERR_OPERATION_LOCKED),L"Quero Toolbar",MB_OK|MB_ICONSTOP);
		break;

	case QUERO_COMMAND_SETFOCUS_COMBOENGINE:
		ShowToolbarIfHidden();
		if(g_Options&OPTION_ShowSearchEngineComboBox) m_ComboEngine.SetFocus();
		break;

	case QUERO_COMMAND_SHOWTOOLBAR:
		if(lParam) ShowToolbarIfHidden();
		else if(m_pBrowser) m_pBand->ShowQueroToolbar(m_pBrowser,VARIANT_FALSE);
		break;

	#ifdef COMPILE_FOR_WINDOWS_VISTA
	case QUERO_COMMAND_UPDATE_DWMTOPMARGIN_DELAYED:
		if(pQueroBroker) pQueroBroker->PostMessageToIE((LONG)lParam,WM_QUERO_UPDATE_DWMTOPMARGIN,0xFFFFFFFF,0xFFFFFFFF);
		break;

	case QUERO_COMMAND_UPDATE_DWMTOPMARGIN:
		if(g_Options2&OPTION2_EnableAeroTheme)
		{
			// Extend frame into client area and redraw window
			InterlockedIncrement(&g_ToolbarBackgroundState);
			m_ReBar.DwmFrameTopMargin=0;
			if(pQueroBroker) pQueroBroker->SetDwmFrameTopMargin((LONG)lParam,0);
			HWND hWndReBar = m_pBand->GetParentWindow();
			if(hWndReBar) ::RedrawWindow(hWndReBar,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN);
		}
		break;

	case QUERO_COMMAND_IEFRAME_ACTIVATED:
		if(g_Options2&OPTION2_EnableAeroTheme)
		{
			if(IsCompositionActive()==FALSE && IsThemeActive())
			{
				InterlockedIncrement(&g_ToolbarBackgroundState);
				::RedrawWindow(m_ReBar.GetParent(),NULL,NULL,RDW_INVALIDATE|RDW_NOERASE|RDW_ALLCHILDREN);
			}
		}
		break;
	#endif // End COMPILE_FOR_WINDOWS_VISTA

	case QUERO_COMMAND_UPDATE_QUERO_INSTANCE:
		UpdateQueroInstance((UINT)lParam);
		break;

	case QUERO_COMMAND_IEFRAME_CHANGED:
		IEFrame_Changed();
		break;

	case QUERO_COMMAND_SETAUTOMAXIMIZE:
		SetAutoMaximize((lParam&OPTION2_AutoMaximize)!=0);
		break;

	case QUERO_COMMAND_MAXIMIZE:
		::ShowWindow(GetIEFrameWindow(),SW_MAXIMIZE);
		break;
	}

	return result;
}

void CQToolbar::ShowToolbarIfHidden()
{
	HWND hWndReBar;

	if(m_pBrowser)
	{
		hWndReBar = m_pBand->GetParentWindow();
		if(hWndReBar)
		{
			if(::IsWindowVisible(hWndReBar)==FALSE && IsTheaterMode()==false)
			{
				BOOL b;

				m_ButtonBar.ShowButtons(0);
				OnSysColorChange(WM_SYSCOLORCHANGE,0,0,b);
				m_pBrowser->put_ToolBar(TRUE);
			}
			if(IsWindowVisible()==FALSE)
			{
				m_pBand->ShowQueroToolbar(m_pBrowser,VARIANT_TRUE);
			}
		}
	}
}

LRESULT CQToolbar::OnMiddleClickUp(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	MapMiddleButton_ContextMenu(wParam,lParam);
	return 0;
}

void CQToolbar::CenterDialog(HWND hwnd_Dialog)
{
	HWND hIEWnd=GetIEFrameWindow();

	if(hIEWnd)
	{
		int PosX,PosY;
		RECT Rect_IE;
		RECT Rect_Dialog;

		::GetWindowRect(hwnd_Dialog,&Rect_Dialog);
		::GetWindowRect(hIEWnd,&Rect_IE);

		PosX=Rect_IE.left + (Rect_IE.right-Rect_IE.left-Rect_Dialog.right+Rect_Dialog.left)/2;
		PosY=Rect_IE.top + (Rect_IE.bottom-Rect_IE.top-Rect_Dialog.bottom+Rect_Dialog.top)/2;

		::SetWindowPos(hwnd_Dialog,HWND_TOP,PosX,PosY,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOREDRAW);
	}
}

bool CQToolbar::IsWindowMaximized()
{	
	return ::IsZoomed(GetIEFrameWindow())==TRUE;
}

bool CQToolbar::IsTheaterMode()
{
	VARIANT_BOOL vbTheaterMode;

	if(m_pBrowser==NULL || m_pBrowser->get_TheaterMode(&vbTheaterMode)!=S_OK)
		vbTheaterMode=VARIANT_FALSE;

	return (vbTheaterMode==VARIANT_TRUE);
}