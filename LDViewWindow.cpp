#define _WIN32_WINNT 0x501
#include "LDViewWindow.h"
#include <shlwapi.h>
#include "LDVExtensionsSetup.h"
#include "LDViewPreferences.h"
#include "SSModelWindow.h"
#include "AppResources.h"
#include "UserDefaultsKeys.h"
#include <LDLoader/LDLModel.h>
#include <TCFoundation/TCUserDefaults.h>
#include <TCFoundation/mystring.h>
#include <TCFoundation/TCAutoreleasePool.h>
#include <TCFoundation/TCStringArray.h>
#include <TCFoundation/TCSortedStringArray.h>
#include <TCFoundation/TCThreadManager.h>
#include <TCFoundation/TCTypedObjectArray.h>
#include <TCFoundation/TCLocalStrings.h>
#include <CUI/CUIWindowResizer.h>
#include <LDLib/LDLibraryUpdater.h>
#include <afxres.h>

#include "ModelWindow.h"
#include <TCFoundation/TCMacros.h>

#define DOWNLOAD_TIMER 12
#define DEFAULT_WIN_WIDTH 640
#define DEFAULT_WIN_HEIGHT 480

static char monthShortNames[12][4] =
{
	"Jan",
	"Feb",
	"Mar",
	"Apr",
	"May",
	"Jun",
	"Jul",
	"Aug",
	"Sep",
	"Oct",
	"Nov",
	"Dec"
};

TCStringArray* LDViewWindow::recentFiles = NULL;
TCStringArray* LDViewWindow::extraSearchDirs = NULL;

LDViewWindow::LDViewWindowCleanup LDViewWindow::ldViewWindowCleanup;

void debugOut(char *fmt, ...);

TbButtonInfo::TbButtonInfo(void)
	:tooltipText(NULL),
	commandId(-1),
	stdBmpId(-1),
	tbBmpId(-1),
	state(TBSTATE_ENABLED),
	style(TBSTYLE_BUTTON)
{
}

void TbButtonInfo::dealloc(void)
{
	delete tooltipText;
	TCObject::dealloc();
}

void TbButtonInfo::setTooltipText(const char *value)
{
	if (value != tooltipText)
	{
		delete tooltipText;
		tooltipText = copyString(value);
	}
}

int TbButtonInfo::getBmpId(int stdBitmapStartId, int tbBitmapStartId)
{
	if (stdBmpId == -1)
	{
		return tbBitmapStartId + tbBmpId;
	}
	else
	{
		return stdBitmapStartId + stdBmpId;
	}
}

LDViewWindow::LDViewWindowCleanup::~LDViewWindowCleanup(void)
{
	if (LDViewWindow::recentFiles)
	{
		LDViewWindow::recentFiles->release();
		LDViewWindow::recentFiles = NULL;
	}
	TCObject::release(LDViewWindow::extraSearchDirs);
	LDViewWindow::extraSearchDirs = NULL;
}

LDViewWindow::LDViewWindow(const char* windowTitle, HINSTANCE hInstance, int x,
						   int y, int width, int height)
			  :CUIWindow(windowTitle, hInstance, x, y, width, height),
			   modelWindow(NULL),
			   hAboutWindow(NULL),
			   hLDrawDirWindow(NULL),
			   hOpenGLInfoWindow(NULL),
			   hExtraDirsWindow(NULL),
			   hStatusBar(NULL),
			   hToolbar(NULL),
			   userLDrawDir(NULL),
			   fullScreen(NO),
			   fullScreenActive(NO),
			   switchingModes(NO),
			   videoModes(NULL),
			   numVideoModes(0),
			   currentVideoModeIndex(-1),
			   showStatusBarOverride(false),
			   skipMinimize(NO),
			   screenSaver(NO),
			   originalMouseX(-999999),
			   originalMouseY(-999999),
			   hFileMenu(NULL),
			   hViewMenu(NULL),
			   loading(FALSE),
			   openGLInfoWindoResizer(NULL),
			   hOpenGLStatusBar(NULL),
			   hExamineIcon(NULL),
			   hFlythroughIcon(NULL),
			   libraryUpdater(NULL),
			   productVersion(NULL),
			   legalCopyright(NULL),
			   tbButtonInfos(NULL),
			   stdBitmapStartId(-1),
			   tbBitmapStartId(-1),
			   prefs(NULL),
			   drawWireframe(false),
			   lastViewAngle(LDVAngleDefault)
//			   modelWindowShown(false)
{
	CUIThemes::init();
	if (CUIThemes::isThemeLibLoaded())
	{
		if (TCUserDefaults::longForKey(VISUAL_STYLE_ENABLED_KEY, 1, false))
		{
			CUIThemes::setThemeAppProperties(STAP_ALLOW_NONCLIENT |
				STAP_ALLOW_CONTROLS);
		}
		else
		{
			CUIThemes::setThemeAppProperties(STAP_ALLOW_NONCLIENT);
		}
	}
	loadSettings();
	standardWindowStyle = windowStyle;
	if (!recentFiles)
	{
		recentFiles = new TCStringArray(10);
		populateRecentFiles();
	}
	if (!extraSearchDirs)
	{
		extraSearchDirs = new TCStringArray;
		populateExtraSearchDirs();
	}
	hExamineIcon = (HICON)LoadImage(getLanguageModule(),
		MAKEINTRESOURCE(IDI_EXAMINE), IMAGE_ICON, 32, 16, LR_DEFAULTCOLOR);
	hFlythroughIcon = (HICON)LoadImage(getLanguageModule(),
		MAKEINTRESOURCE(IDI_FLYTHROUGH), IMAGE_ICON, 32, 16, LR_DEFAULTCOLOR);
//	DeleteObject(hBackgroundBrush);
// 	hBackgroundBrush = CreateSolidBrush(RGB(backgroundColor & 0xFF,
//		(backgroundColor >> 8) & 0xFF, (backgroundColor >> 16) & 0xFF));
}

LDViewWindow::~LDViewWindow(void)
{
}

void LDViewWindow::dealloc(void)
{
	delete userLDrawDir;
	userLDrawDir = NULL;
	delete videoModes;
	videoModes = NULL;
	if (hOpenGLInfoWindow)
	{
		DestroyWindow(hOpenGLInfoWindow);
	}
	if (openGLInfoWindoResizer)
	{
		openGLInfoWindoResizer->release();
	}
	if (hExtraDirsWindow)
	{
		DestroyWindow(hExtraDirsWindow);
	}
	delete productVersion;
	delete legalCopyright;
	TCObject::release(tbButtonInfos);
	TCObject::release(prefs);
	CUIWindow::dealloc();
}

void LDViewWindow::loadSettings(void)
{
	fsWidth = TCUserDefaults::longForKey(FULLSCREEN_WIDTH_KEY, 640);
	fsHeight = TCUserDefaults::longForKey(FULLSCREEN_HEIGHT_KEY, 480);
	fsDepth = TCUserDefaults::longForKey(FULLSCREEN_DEPTH_KEY, 32);
	showStatusBar = TCUserDefaults::longForKey(STATUS_BAR_KEY, 1, false) != 0;
	showToolbar = TCUserDefaults::longForKey(TOOLBAR_KEY, 1, false) != 0;
	topmost = TCUserDefaults::longForKey(TOPMOST_KEY, 0, false) != 0;
	visualStyleEnabled = TCUserDefaults::longForKey(VISUAL_STYLE_ENABLED_KEY,
		1, false) != 0;
}

HBRUSH LDViewWindow::getBackgroundBrush(void)
{
//	return CUIWindow::getBackgroundBrush();
	return NULL;
}

void LDViewWindow::showWindow(int nCmdShow)
{
	LDrawModelViewer *modelViewer;
	
	if (screenSaver)
	{
		x = 0;
		y = 0;
		width = GetSystemMetrics(SM_CXSCREEN);
		height = GetSystemMetrics(SM_CYSCREEN);
		CUIWindow::showWindow(SW_SHOW);
	}
	else
	{
		CUIWindow::showWindow(nCmdShow);
	}
	modelViewer = modelWindow->getModelViewer();
	if (modelViewer)
	{
		modelViewer->setExtraSearchDirs(extraSearchDirs);
	}
	modelWindow->finalSetup();
}

void LDViewWindow::setScreenSaver(BOOL flag)
{
	screenSaver = flag;
	if (screenSaver)
	{
		windowStyle |= WS_CLIPCHILDREN;
	}
	else
	{
		windowStyle &= ~WS_CLIPCHILDREN;
	}
}

char* LDViewWindow::windowClassName(void)
{
	if (fullScreen || screenSaver)
	{
		return "LDViewFullScreenWindow";
	}
	else
	{
		return "LDViewWindow";
	}
}

LRESULT LDViewWindow::doEraseBackground(RECT* updateRect)
{
	BOOL noRect = FALSE;

	if (updateRect == NULL)
	{
		if (paintStruct)
		{
			return 0;
		}
		updateRect = new RECT;
		GetUpdateRect(hWindow, updateRect, FALSE);
		noRect = TRUE;
	}
	DWORD backgroundColor = LDViewPreferences::getColor(BACKGROUND_COLOR_KEY);
 	HBRUSH hBrush = CreateSolidBrush(RGB(backgroundColor & 0xFF,
		(backgroundColor >> 8) & 0xFF, (backgroundColor >> 16) & 0xFF));

	debugPrintf(2, "updateRect size1: %d, %d\n",
		updateRect->right - updateRect->left,
		updateRect->bottom - updateRect->top);
	if (!fullScreen && !screenSaver)
	{
		int bottomMargin = 2;
		int topMargin = 2;

		if (updateRect->left < 2)
		{
			updateRect->left = 2;
		}
		if (updateRect->top < 2)
		{
			updateRect->top = 2;
		}
		if (updateRect->right > width - 2)
		{
			updateRect->right = width - 2;
		}
		if (showStatusBar || showStatusBarOverride)
		{
			bottomMargin += getStatusBarHeight();
		}
		if (updateRect->bottom > height - bottomMargin)
		{
			updateRect->bottom = height - bottomMargin;
		}
		if (showToolbar)
		{
			topMargin += getToolbarHeight();
		}
		if (updateRect->top < topMargin)
		{
			updateRect->top = topMargin;
		}
	}
	debugPrintf(2, "updateRect size2: %d, %d\n",
		updateRect->right - updateRect->left,
		updateRect->bottom - updateRect->top);
	FillRect(hdc, updateRect, hBrush);
	DeleteObject(hBrush);
	CUIWindow::doEraseBackground(updateRect);
	if (hToolbar)
	{
		HRGN region = CreateRectRgn(updateRect->left, updateRect->top,
			updateRect->right, updateRect->bottom);
		RECT rect;
		POINT points[2];
		static int num = 0;

		GetClientRect(hToolbar, &rect);
		points[0].x = rect.left;
		points[0].y = rect.top;
		points[1].x = rect.right;
		points[1].y = rect.bottom;
		MapWindowPoints(hWindow, hWindow, points, 2);
		rect.left = points[0].x;
		rect.top = points[0].y;
		rect.right = points[1].x;
		rect.bottom = points[1].y;
		if (RectInRegion(region, &rect) || updateRect->top ==
			getToolbarHeight() + 2)
		{
			// For some reason, the toolbar won't redraw itself until there's an
			// idle moment.  So it won't redraw while the model is spinning, for
			// example.  The folowing forces it to redraw itself right now.
			RedrawWindow(hToolbar, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		}
		DeleteObject(region);
	}
	if (noRect)
	{
		delete updateRect;
		updateRect = NULL;
	}
	return 0;
}

void LDViewWindow::forceShowStatusBar(bool value)
{
	if (value != showStatusBarOverride)
	{
		showStatusBarOverride = value;
		if (fullScreenActive)
		{
			if (!hStatusBar && showStatusBarOverride)
			{
				addStatusBar();
			}
			else if (hStatusBar && !showStatusBarOverride)
			{
				removeStatusBar();
			}
		}
		else if (!showStatusBar)
		{
			if (showStatusBarOverride)
			{
				addStatusBar();
			}
			else
			{
				removeStatusBar();
			}
		}
	}
}

void LDViewWindow::showStatusIcon(bool examineMode)
{
	if ((showStatusBar || showStatusBarOverride) && hStatusBar)
	{
		HICON hModeIcon = hExamineIcon;
		const char *tipText = TCLocalStrings::get("ExamineMode");

		if (!examineMode)
		{
			hModeIcon = hFlythroughIcon;
			tipText = TCLocalStrings::get("FlyThroughMode");
		}
		SendMessage(hStatusBar, SB_SETICON, 2, (LPARAM)hModeIcon);
		SendMessage(hStatusBar, SB_SETTIPTEXT, 2, (LPARAM)tipText);
	}
}

void LDViewWindow::addTbButtonInfo(const char *tooltipText, int commandId,
								   int stdBmpId, int tbBmpId, BYTE style,
								   BYTE state)
{
	TbButtonInfo *buttonInfo = new TbButtonInfo;

	buttonInfo->setTooltipText(tooltipText);
	buttonInfo->setCommandId(commandId);
	buttonInfo->setStdBmpId(stdBmpId);
	buttonInfo->setTbBmpId(tbBmpId);
	buttonInfo->setStyle(style);
	buttonInfo->setState(state);
	tbButtonInfos->addObject(buttonInfo);
	buttonInfo->release();
}

void LDViewWindow::addTbCheckButtonInfo(const char *tooltipText, int commandId,
										int stdBmpId, int tbBmpId, bool checked,
										BYTE style, BYTE state)
{
	state &= ~TBSTATE_CHECKED;
	if (checked)
	{
		state |= TBSTATE_CHECKED;
	}
	addTbButtonInfo(tooltipText, commandId, stdBmpId, tbBmpId, style, state);
}

void LDViewWindow::addTbSeparatorInfo(void)
{
	TbButtonInfo *buttonInfo = new TbButtonInfo;

	buttonInfo->setStyle(TBSTYLE_SEP);
	tbButtonInfos->addObject(buttonInfo);
	buttonInfo->release();
}

void LDViewWindow::populateTbButtonInfos(void)
{
	if (!tbButtonInfos)
	{
		tbButtonInfos = new TbButtonInfoArray;
		addTbButtonInfo(TCLocalStrings::get("OpenFile"), ID_FILE_OPEN,
			STD_FILEOPEN, -1);
		addTbButtonInfo(TCLocalStrings::get("SaveSnapshot"), ID_FILE_SAVE, -1,
			5);
		addTbButtonInfo(TCLocalStrings::get("Reload"), ID_FILE_RELOAD, -1, 0);
		addTbSeparatorInfo();
		drawWireframe = prefs->getDrawWireframe();
		seams = prefs->getUseSeams() != 0;
		edges = prefs->getShowsHighlightLines();
		primitiveSubstitution = prefs->getAllowPrimitiveSubstitution();
		lighting = prefs->getUseLighting();
		bfc = prefs->getBfc();
		addTbCheckButtonInfo(TCLocalStrings::get("Wireframe"), IDC_WIREFRAME,
			-1, 1, drawWireframe);
		addTbCheckButtonInfo(TCLocalStrings::get("Seams"), IDC_SEAMS, -1, 2,
			seams);
		addTbCheckButtonInfo(TCLocalStrings::get("EdgeLines"), IDC_HIGHLIGHTS,
			-1, 3, edges, TBSTYLE_CHECK | TBSTYLE_DROPDOWN);
		addTbCheckButtonInfo(TCLocalStrings::get("PrimitiveSubstitution"),
			IDC_PRIMITIVE_SUBSTITUTION, -1, 4, primitiveSubstitution);
		addTbCheckButtonInfo(TCLocalStrings::get("Lighting"), IDC_LIGHTING, -1,
			7, lighting);
		addTbCheckButtonInfo(TCLocalStrings::get("BFC"), IDC_BFC, -1, 9, bfc);
		addTbButtonInfo(TCLocalStrings::get("ResetSelectView"), ID_VIEWANGLE,
			-1, 6, TBSTYLE_DROPDOWN);
		addTbButtonInfo(TCLocalStrings::get("Preferences"), ID_EDIT_PREFERENCES,
			-1, 8);
	}
}

void LDViewWindow::createToolbar(void)
{
	if (showToolbar)
	{
		TBADDBITMAP addBitmap;
		TBBUTTON *buttons;
		char buttonTitle[128];
		int i;
		int count;

		populateTbButtonInfos();
		ModelWindow::initCommonControls(ICC_BAR_CLASSES);
		hToolbar = CreateWindowEx(0, TOOLBARCLASSNAME,
			NULL, WS_CHILD | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS, 0, 0,
			0, 0, hWindow, (HMENU)ID_TOOLBAR, hInstance, NULL);
		SendMessage(hToolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_DRAWDDARROWS);
		memset(buttonTitle, 0, sizeof(buttonTitle));
		strcpy(buttonTitle, "");
		ModelWindow::initCommonControls(ICC_BAR_CLASSES | ICC_WIN95_CLASSES);
		SendMessage(hToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
		SendMessage(hToolbar, TB_SETBUTTONWIDTH, 0, MAKELONG(25, 25));
		SendMessage(hToolbar, TB_SETBUTTONSIZE, 0, MAKELONG(25, 16));
		addBitmap.hInst = HINST_COMMCTRL;
		addBitmap.nID = IDB_STD_SMALL_COLOR;
		stdBitmapStartId = SendMessage(hToolbar, TB_ADDBITMAP, 0,
			(LPARAM)&addBitmap);
		addBitmap.hInst = getLanguageModule();
		addBitmap.nID = IDB_TOOLBAR;
		tbBitmapStartId = SendMessage(hToolbar, TB_ADDBITMAP, 4,
			(LPARAM)&addBitmap);
		SendMessage(hToolbar, TB_ADDSTRING, 0, (LPARAM)buttonTitle);
		count = tbButtonInfos->getCount();
		buttons = new TBBUTTON[count];
		for (i = 0; i < count; i++)
		{
			TbButtonInfo *buttonInfo = (*tbButtonInfos)[i];

			buttons[i].iBitmap = buttonInfo->getBmpId(stdBitmapStartId,
				tbBitmapStartId);
			buttons[i].idCommand = buttonInfo->getCommandId();
			buttons[i].fsState = buttonInfo->getState();
			buttons[i].fsStyle = buttonInfo->getStyle();
			buttons[i].dwData = (DWORD)this;
			buttons[i].iString = -1;
		}
		SendMessage(hToolbar, TB_ADDBUTTONS, count, (LPARAM)buttons);
		delete[] buttons;
		ShowWindow(hToolbar, SW_SHOW);
		SendMessage(hToolbar, TB_AUTOSIZE, 0, 0);
	}
}

void LDViewWindow::createStatusBar(void)
{
	if (showStatusBar || showStatusBarOverride)
	{
		int parts[] = {100, 100, -1};
		HWND hProgressBar;
		RECT rect;

		ModelWindow::initCommonControls(ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES);
		hStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP |
			SBT_TOOLTIPS, "", hWindow, ID_STATUS_BAR);
		SetWindowLong(hStatusBar, GWL_EXSTYLE, WS_EX_TRANSPARENT);
		SendMessage(hStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
//		SendMessage(hStatusBar, SB_SETTEXT, 1, (LPARAM)"");
//		SendMessage(hStatusBar, SB_SETTEXT, 0 | SBT_OWNERDRAW, (LPARAM)"");
		SendMessage(hStatusBar, SB_SETTEXT, 0 | SBT_NOBORDERS, (LPARAM)"");
		SendMessage(hStatusBar, SB_GETRECT, 0, (LPARAM)&rect);
		InflateRect(&rect, -4, -3);
//		OffsetRect(&rect, -2, 0);
		hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, "",
			WS_CHILD | WS_VISIBLE | PBS_SMOOTH, rect.left,
			rect.top, rect.right - rect.left, rect.bottom - rect.top,
			hStatusBar, NULL, hInstance, NULL);
		SendMessage(hStatusBar, SB_GETRECT, 2, (LPARAM)&rect);
		parts[1] += rect.right - rect.left - 32;
		SendMessage(hStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
		if (modelWindow && modelWindow->getViewMode() == LDVViewFlythrough)
		{
			showStatusIcon(false);
		}
		else
		{
			showStatusIcon(true);
		}
//		SendMessage(hProgressBar, PBM_SETPOS, 50, 0);
		if (modelWindow)
		{
			modelWindow->setStatusBar(hStatusBar);
			modelWindow->setProgressBar(hProgressBar);
		}
		RedrawWindow(hStatusBar, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	}
}

void LDViewWindow::reflectViewMode(void)
{
	switch (TCUserDefaults::longForKey(VIEW_MODE_KEY, 0, false))
	{
	case LDVViewExamine:
		switchToExamineMode();
		break;
	case LDVViewFlythrough:
		switchToFlythroughMode();
		break;
	default:
		switchToExamineMode();
		break;
	}
}

BOOL LDViewWindow::initWindow(void)
{
	if (!modelWindow)
	{
		createModelWindow();
	}
	if (fullScreen || screenSaver)
	{
		if (hWindowMenu)
		{
			DestroyMenu(hWindowMenu);
			hWindowMenu = NULL;
		}
		windowStyle = WS_POPUP | WS_MAXIMIZE;
		if (screenSaver)
		{
			windowStyle |= WS_CLIPCHILDREN;
		}
#ifndef _DEBUG
		exWindowStyle |= WS_EX_TOPMOST;
#endif
	}
	else
	{
		hWindowMenu = LoadMenu(getLanguageModule(),
			MAKEINTRESOURCE(IDR_MAIN_MENU));
		windowStyle = standardWindowStyle;
		if (TCUserDefaults::longForKey(WINDOW_MAXIMIZED_KEY, 0, false))
		{
			windowStyle |= WS_MAXIMIZE;
		}
		if (topmost)
		{
			exWindowStyle |= WS_EX_TOPMOST;
		}
		else
		{
			exWindowStyle &= ~WS_EX_TOPMOST;
		}
	}
	if (CUIWindow::initWindow())
	{
		hFileMenu = GetSubMenu(GetMenu(hWindow), 0);
		hViewMenu = GetSubMenu(GetMenu(hWindow), 2);
		hViewAngleMenu = findSubMenu(hViewMenu, 0);
		if (!CUIThemes::isThemeLibLoaded())
		{
			RemoveMenu(hViewMenu, ID_VIEW_VISUALSTYLE, MF_BYCOMMAND);
		}
		else
		{
			reflectVisualStyle();
		}
//		hViewAngleMenu = GetSubMenu(hViewMenu, 7);
		hToolbarMenu = LoadMenu(getLanguageModule(),
			MAKEINTRESOURCE(IDR_TOOLBAR_MENU));
		hEdgesMenu = GetSubMenu(hToolbarMenu, 0);
		reflectViewMode();
		populateRecentFileMenuItems();
		updateModelMenuItems();
		if (!fullScreen && !screenSaver)
		{
			setMenuCheck(hViewMenu, ID_VIEW_ALWAYSONTOP, topmost);
		}
		return modelWindow->initWindow();
	}
	return FALSE;
}

void LDViewWindow::createModelWindow(void)
{
	int width;
	int height;
	bool maximized;

	TCObject::release(modelWindow);
	width = TCUserDefaults::longForKey(WINDOW_WIDTH_KEY, DEFAULT_WIN_WIDTH,
		false);
	height = TCUserDefaults::longForKey(WINDOW_HEIGHT_KEY, DEFAULT_WIN_HEIGHT,
		false);
	maximized = TCUserDefaults::longForKey(WINDOW_MAXIMIZED_KEY, 0, false) != 0;
	if (screenSaver)
	{
		modelWindow = new SSModelWindow(this, 0, 0, width, height);
	}
	else
	{
		// Note that while the toolbar and status bar might be turned on, they
		// haven't been shown yet.  They'll resize the model window when they
		// get shown.
		modelWindow = new ModelWindow(this, 0, 0, width, height);
	}
	prefs = modelWindow->getPrefs();
	prefs->retain();
}

BOOL LDViewWindow::showAboutBox(void)
{
	if (!hAboutWindow)
	{
		createAboutBox();
	}
	if (hAboutWindow)
	{
		runDialogModal(hAboutWindow);
		return TRUE;
	}
	return FALSE;
}

const char *LDViewWindow::getProductVersion(void)
{
	if (!productVersion)
	{
		readVersionInfo();
	}
	return productVersion;
}

const char *LDViewWindow::getLegalCopyright(void)
{
	if (!legalCopyright)
	{
		readVersionInfo();
	}
	return legalCopyright;
}

void LDViewWindow::readVersionInfo(void)
{
	char moduleFilename[1024];

	if (GetModuleFileName(NULL, moduleFilename, sizeof(moduleFilename)) > 0)
	{
		DWORD zero;
		DWORD versionInfoSize = GetFileVersionInfoSize(moduleFilename, &zero);

		if (versionInfoSize > 0)
		{
			BYTE *versionInfo = new BYTE[versionInfoSize];

			if (GetFileVersionInfo(moduleFilename, NULL, versionInfoSize,
				versionInfo))
			{
				char *value;
				UINT versionLength;

				if (VerQueryValue(versionInfo,
					"\\StringFileInfo\\040904B0\\ProductVersion",
					(void**)&value, &versionLength))
				{
					productVersion = copyString(value);
				}
				if (VerQueryValue(versionInfo,
					"\\StringFileInfo\\040904B0\\LegalCopyright",
					(void**)&value, &versionLength))
				{
					legalCopyright = copyString(value);
				}
			}
			delete versionInfo;
		}
	}
}

#include <time.h>

void LDViewWindow::createAboutBox(void)
{
	char fullVersionFormat[1024];
	char fullVersionString[1024];
	char versionString[128];
	char copyrightString[128];
	char buildDateString[128] = __DATE__;
	int dateCount;
	char **dateComponents = componentsSeparatedByString(buildDateString, " ",
		dateCount);

	sprintf(buildDateString, "!UnknownDate!");
	if (dateCount == 3)
	{
		const char *buildMonth = TCLocalStrings::get(dateComponents[0]);

		if (buildMonth)
		{
			sprintf(buildDateString, "%s %s, %s", dateComponents[1], buildMonth,
				dateComponents[2]);
		}
	}
	deleteStringArray(dateComponents, dateCount);
	strcpy(versionString, TCLocalStrings::get("!UnknownVersion!"));
	strcpy(copyrightString, TCLocalStrings::get("Copyright"));
	hAboutWindow = createDialog(IDD_ABOUT_BOX);
	SendDlgItemMessage(hAboutWindow, IDC_VERSION_LABEL, WM_GETTEXT,
		sizeof(fullVersionFormat), (LPARAM)fullVersionFormat);
	readVersionInfo();
	if (productVersion)
	{
		strcpy(versionString, productVersion);
	}
	if (legalCopyright)
	{
		strcpy(copyrightString, legalCopyright);
	}
	sprintf(fullVersionString, fullVersionFormat, versionString,
		buildDateString, copyrightString);
	SendDlgItemMessage(hAboutWindow, IDC_VERSION_LABEL, WM_SETTEXT,
		sizeof(fullVersionString), (LPARAM)fullVersionString);
}

BOOL LDViewWindow::doLDrawDirOK(HWND hDlg)
{
	int length = SendDlgItemMessage(hDlg, IDC_LDRAWDIR, WM_GETTEXTLENGTH, 0, 0);

	if (length)
	{
		delete userLDrawDir;
		userLDrawDir = new char[length + 1];
		SendDlgItemMessage(hDlg, IDC_LDRAWDIR, WM_GETTEXT, (WPARAM)(length + 1),
			(LPARAM)userLDrawDir);
		doDialogClose(hDlg);
	}
	else
	{
		MessageBeep(MB_ICONEXCLAMATION);
	}
	return TRUE;
}

LRESULT LDViewWindow::doMouseWheel(short keyFlags, short zDelta, int /*xPos*/,
								  int /*yPos*/)
{
	if (modelWindow)
	{
		if (keyFlags & MK_CONTROL)
		{
			modelWindow->setClipZoom(YES);
		}
		else
		{
			modelWindow->setClipZoom(NO);
		}
		modelWindow->zoom((float)zDelta * -0.5f);
		return 0;
	}
	return 1;
}


UINT CALLBACK lDrawDirBrowseHook(HWND /*hDlg*/, UINT message, WPARAM /*wParam*/,
								 LPARAM lParam)
{
#ifdef _DEBUG
	_CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "hook message: 0x%X\n", message);
#endif // _DEBUG
	if (message == WM_NOTIFY)
	{
		LPOFNOTIFY notification = (LPOFNOTIFY)lParam;

		switch (notification->hdr.code)
		{
			case CDN_FILEOK:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "OK Pressed\n");
#endif // _DEBUG
				break;
			case CDN_FOLDERCHANGE:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL,
					"Folder Change: 0x%X\n", notification->hdr.hwndFrom);
#endif // _DEBUG
				break;
			case CDN_HELP:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "Help Pressed\n");
#endif // _DEBUG
				break;
			case CDN_INITDONE:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "Init done\n");
#endif // _DEBUG
				break;
			case CDN_SELCHANGE:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL,
					"Selection Change: 0x%X\n", notification->hdr.hwndFrom);
#endif // _DEBUG
				break;
			case CDN_SHAREVIOLATION:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "Share violation\n");
#endif // _DEBUG
				break;
			case CDN_TYPECHANGE:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "Type change\n");
#endif // _DEBUG
				break;
			default:
#ifdef _DEBUG
				_CrtDbgReport(_CRT_WARN, NULL, 0, NULL,
					"Unknown notification: 0x%X\n", notification->hdr.code);
#endif // _DEBUG
				break;
		}
	}
	return 0;
}

BOOL LDViewWindow::tryVideoMode(VideoModeT* videoMode, int refreshRate)
{
	if (videoMode)
	{
		DEVMODE deviceMode;
		long result;

		fullScreenActive = YES;

		memset(&deviceMode, 0, sizeof DEVMODE);
		deviceMode.dmSize = sizeof DEVMODE;
		deviceMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
		deviceMode.dmPelsWidth = videoMode->width;
		deviceMode.dmPelsHeight = videoMode->height;
		deviceMode.dmBitsPerPel = videoMode->depth;
		if (refreshRate)
		{
			deviceMode.dmFields |= DM_DISPLAYFREQUENCY;
			deviceMode.dmDisplayFrequency = refreshRate;
		}
		result = ChangeDisplaySettings(&deviceMode, CDS_FULLSCREEN);
		switch (result)
		{
			case DISP_CHANGE_SUCCESSFUL:
				return YES;
				break;
			case DISP_CHANGE_RESTART:
				fullScreenActive = NO;
				return NO;
				break;
			case DISP_CHANGE_BADFLAGS:
				fullScreenActive = NO;
				return NO;
				break;
			case DISP_CHANGE_BADPARAM:
				fullScreenActive = NO;
				return NO;
				break;
			case DISP_CHANGE_FAILED:
				fullScreenActive = NO;
				return NO;
				break;
			case DISP_CHANGE_BADMODE:
				fullScreenActive = NO;
				return NO;
				break;
			case DISP_CHANGE_NOTUPDATED:
				fullScreenActive = NO;
				return NO;
				break;
		}
		fullScreenActive = NO;
	}
	return NO;
}

void LDViewWindow::setFullScreenDisplayMode(void)
{
	VideoModeT* videoMode = getCurrentVideoMode();

	if (!fullScreenActive && videoMode)
	{
		int refreshRate = TCUserDefaults::longForKey(FULLSCREEN_REFRESH_KEY);

		if (!tryVideoMode(videoMode, refreshRate) && refreshRate)
		{
			tryVideoMode(videoMode, 0);
		}
	}
}

void LDViewWindow::restoreDisplayMode(void)
{
	if (fullScreenActive)
	{
		long result = ChangeDisplaySettings(NULL, 0);

		switch (result)
		{
			case DISP_CHANGE_SUCCESSFUL:
				break;
			case DISP_CHANGE_RESTART:
				break;
			case DISP_CHANGE_BADFLAGS:
				break;
			case DISP_CHANGE_BADPARAM:
				break;
			case DISP_CHANGE_FAILED:
				break;
			case DISP_CHANGE_BADMODE:
				break;
			case DISP_CHANGE_NOTUPDATED:
				break;
		}
		fullScreenActive = NO;
	}
}

/*
void LDViewWindow::setModelWindow(ModelWindow *value)
{
	value->retain();
	if (modelWindow)
	{
		modelWindow->release();
	}
	modelWindow = value;
}
*/

void LDViewWindow::activateFullScreenMode(void)
{
	if (!fullScreenActive)
	{
		skipMinimize = YES;
		if (!getCurrentVideoMode())
		{
			MessageBeep(MB_ICONEXCLAMATION);
			return;
		}
//		modelWindow->uncompile();
		switchingModes = YES;
		if (modelWindow)
		{
			modelWindow->closeWindow();
//			modelWindowShown = false;
		}
		DestroyWindow(hWindow);
		switchingModes = NO;
		setFullScreenDisplayMode();
		if (initWindow())
		{
//			if (modelWindow->initWindow())
			{
//				modelWindow->showWindow(SW_SHOW);
//				modelWindowShown = true;
				showWindow(SW_SHOW);
				modelWindow->setNeedsRecompile();
				modelWindow->forceRedraw(1);
			}
		}
		skipMinimize = NO;
	}
}

void LDViewWindow::deactivateFullScreenMode(void)
{
	modelWindow->uncompile();
	restoreDisplayMode();
	if (skipMinimize)
	{
//		modelWindow->forceRedraw(1);
	}
	else
	{
		ShowWindow(hWindow, SW_MINIMIZE);
		modelWindow->closeWindow();
//		modelWindowShown = false;
	}
}

LRESULT LDViewWindow::doActivate(int activateFlag, BOOL /*minimized*/,
								  HWND /*previousHWindow*/)
{
	if (modelWindow)
	{
		if (!modelWindowIsShown() && !switchingModes &&
			activateFlag != WA_INACTIVE)
		{
			modelWindow->showWindow(SW_SHOWNORMAL);
		}
		if (activateFlag == WA_ACTIVE || activateFlag == WA_CLICKACTIVE)
		{
			modelWindow->startPolling();
		}
		else
		{
			if (modelWindow->getPollSetting() != POLL_BACKGROUND)
			{
				modelWindow->stopPolling();
			}
		}
	}
	return 0;
}

LRESULT LDViewWindow::doActivateApp(BOOL activateFlag, DWORD /*threadId*/)
{
	VideoModeT* videoMode = getCurrentVideoMode();

	if (fullScreen && videoMode)
	{
		if (activateFlag)
		{
			activateFullScreenMode();
		}
		else
		{
			deactivateFullScreenMode();
		}
		return 0;
	}
	else
	{
		return 1;
/*
		if (activateFlag)
		{
			SetActiveWindow(hWindow);
		}
*/
	}
}

/*
BOOL LDViewWindow::doLDrawDirBrowse(HWND hDlg)
{
	OPENFILENAME openStruct;
	char fileTypes[1024];
	char filename[1024] = "";
	char initialDir[1024];

	SendDlgItemMessage(hDlg, IDC_LDRAWDIR, WM_GETTEXT, (WPARAM)(1024),
		(LPARAM)initialDir);
	memset(fileTypes, 0, 2);
	addFileType(fileTypes, TCLocalStrings::get("AllFileTypes"), "*.*");
	memset(&openStruct, 0, sizeof(OPENFILENAME));
	openStruct.lStructSize = sizeof(OPENFILENAME);
	openStruct.lpstrFilter = fileTypes;
	openStruct.nFilterIndex = 1;
	openStruct.lpstrFile = filename;
	openStruct.nMaxFile = 1024;
	openStruct.lpstrInitialDir = initialDir;
	openStruct.lpstrTitle = "Select the LDraw directory";
	openStruct.lpfnHook = lDrawDirBrowseHook;
	openStruct.Flags = OFN_EXPLORER | OFN_NOTESTFILECREATE | OFN_HIDEREADONLY |
		OFN_ALLOWMULTISELECT | OFN_ENABLEHOOK |
		OFN_ENABLESIZING;
	if (GetSaveFileName(&openStruct))
	{
		SendDlgItemMessage(hDlg, IDC_LDRAWDIR, WM_SETTEXT, 0,
			(LPARAM)filename);
	}
	return TRUE;
}
*/

BOOL LDViewWindow::doRemoveExtraDir(void)
{
	int index = SendMessage(hExtraDirsList, LB_GETCURSEL, 0, 0);

	if (index != LB_ERR)
	{
		extraSearchDirs->removeString(index);
		SendMessage(hExtraDirsList, LB_DELETESTRING, index, 0);
		if (index >= extraSearchDirs->getCount())
		{
			index--;
		}
		if (index >= 0)
		{
			SendMessage(hExtraDirsList, LB_SELECTSTRING, index,
				(LPARAM)extraSearchDirs->stringAtIndex(index));
		}
	}
	updateExtraDirsEnabled();
	return TRUE;
}

BOOL LDViewWindow::doAddExtraDir(void)
{
	BROWSEINFO browseInfo;
	char displayName[MAX_PATH];
	ITEMIDLIST* itemIdList;
	char *currentSelection = NULL;
	int index = SendMessage(hExtraDirsList, LB_GETCURSEL, 0, 0);

	if (index != LB_ERR)
	{
		currentSelection = (*extraSearchDirs)[index];
	}
	browseInfo.hwndOwner = NULL; //hWindow;
	browseInfo.pidlRoot = NULL;
	browseInfo.pszDisplayName = displayName;
	browseInfo.lpszTitle = TCLocalStrings::get("AddExtraDirPrompt");
	browseInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	browseInfo.lpfn = pathBrowserCallback;
	browseInfo.lParam = (LPARAM)currentSelection;
	browseInfo.iImage = 0;
	if ((itemIdList = SHBrowseForFolder(&browseInfo)) != NULL)
	{
		char path[MAX_PATH+10];

		if (SHGetPathFromIDList(itemIdList, path))
		{
			stripTrailingPathSeparators(path);
			extraSearchDirs->addString(path);
			SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_LIST, LB_ADDSTRING, 0,
				(LPARAM)path);
			SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_LIST, LB_SETCURSEL,
				extraSearchDirs->getCount() - 1, (LPARAM)path);
			updateExtraDirsEnabled();
		}
	}
	return TRUE;
}

BOOL LDViewWindow::doMoveExtraDirUp(void)
{
	int index = SendMessage(hExtraDirsList, LB_GETCURSEL, 0, 0);
	char *extraDir;

	if (index == LB_ERR || index == 0)
	{
		// we shouldn't get here, but just in case...
		return TRUE;
	}
	extraDir = copyString((*extraSearchDirs)[index]);
	extraSearchDirs->removeString(index);
	SendMessage(hExtraDirsList, LB_DELETESTRING, index, 0);
	extraSearchDirs->insertString(extraDir, index - 1);
	SendMessage(hExtraDirsList, LB_INSERTSTRING, index - 1, (LPARAM)extraDir);
	SendMessage(hExtraDirsList, LB_SETCURSEL, index - 1, (LPARAM)extraDir);
	updateExtraDirsEnabled();
	delete extraDir;
	return TRUE;
}

BOOL LDViewWindow::doMoveExtraDirDown(void)
{
	int index = SendMessage(hExtraDirsList, LB_GETCURSEL, 0, 0);
	char *extraDir;

	if (index == LB_ERR || index >= extraSearchDirs->getCount() - 1)
	{
		// we shouldn't get here, but just in case...
		return TRUE;
	}
	extraDir = copyString((*extraSearchDirs)[index]);
	extraSearchDirs->removeString(index);
	SendMessage(hExtraDirsList, LB_DELETESTRING, index, 0);
	extraSearchDirs->insertString(extraDir, index + 1);
	SendMessage(hExtraDirsList, LB_INSERTSTRING, index + 1, (LPARAM)extraDir);
	SendMessage(hExtraDirsList, LB_SETCURSEL, index + 1, (LPARAM)extraDir);
	updateExtraDirsEnabled();
	delete extraDir;
	return TRUE;
}

BOOL LDViewWindow::doExtraDirSelected(void)
{
	updateExtraDirsEnabled();
	return TRUE;
}

BOOL LDViewWindow::doExtraDirsCommand(int controlId, int notifyCode,
									  HWND hControlWnd)
{
	if (hControlWnd == hExtraDirsToolbar)
	{
		switch (controlId)
		{
		case 42:
			return doAddExtraDir();
			break;
		case 43:
			return doRemoveExtraDir();
			break;
		case 44:
			return doMoveExtraDirUp();
			break;
		case 45:
			return doMoveExtraDirDown();
			break;
		default:
			return FALSE;
		}
	}
	else if (notifyCode == BN_CLICKED)
	{
		switch (controlId)
		{
			case IDOK:
				recordExtraSearchDirs();
				doDialogClose(hExtraDirsWindow);
				break;
			case IDCANCEL:
				populateExtraSearchDirs();
				populateExtraDirsListBox();
				doDialogClose(hExtraDirsWindow);
				break;
			default:
				return FALSE;
				break;
		}
		return TRUE;
	}
	else if (notifyCode == LBN_SELCHANGE)
	{
		return doExtraDirSelected();
	}
	else
	{
		return FALSE;
	}
}

BOOL LDViewWindow::doLDrawDirCommand(int controlId, int notifyCode,
									 HWND /*hControlWnd*/)
{
	if (notifyCode == BN_CLICKED)
	{
		switch (controlId)
		{
			case IDOK:
				return doLDrawDirOK(hLDrawDirWindow);
				break;
			case IDCANCEL:
				doDialogClose(hLDrawDirWindow);
				break;
			default:
				return FALSE;
				break;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL LDViewWindow::doDialogCommand(HWND hDlg, int controlId, int notifyCode,
								   HWND hControlWnd)
{
//	debugPrintf("LDViewWindow::doDialogCommand: 0x%04X, 0x%04X, 0x%04x\n", hDlg,
//		controlId, notifyCode);
	if (hDlg)
	{
		if (hDlg == hExtraDirsWindow)
		{
			return doExtraDirsCommand(controlId, notifyCode, hControlWnd);
		}
		else if (hDlg == hLDrawDirWindow)
		{
			return doLDrawDirCommand(controlId, notifyCode, hControlWnd);
		}
	}
	if (notifyCode == BN_CLICKED)
	{
		switch (controlId)
		{
			case IDOK:
				doDialogClose(hDlg);
				break;
			case IDCANCEL:
				doDialogClose(hDlg);
				break;
			default:
				return FALSE;
				break;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL LDViewWindow::checkMouseMove(HWND hWnd, LPARAM lParam)
{
	POINT mousePoint = { LOWORD(lParam), HIWORD(lParam)};

	ClientToScreen(hWnd, &mousePoint);
	if (originalMouseX == -999999)
	{
		originalMouseX = mousePoint.x;
		originalMouseY = mousePoint.y;
	}
	if (originalMouseX == mousePoint.x && originalMouseY == mousePoint.y)
	{
		return FALSE;
	}
	return TRUE;
}

void LDViewWindow::initMouseMove(void)
{
	originalMouseX = -999999;
}

LRESULT LDViewWindow::doEnterMenuLoop(bool /*isTrackPopupMenu*/)
{
	if (loading)
	{
		return 0;
	}
	if (modelWindow)
	{
		LDrawModelViewer *modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			modelViewer->pause();
		}
	}
	return 1;
}

LRESULT LDViewWindow::doExitMenuLoop(bool /*isTrackPopupMenu*/)
{
	if (modelWindow)
	{
		LDrawModelViewer *modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			modelViewer->unpause();
		}
		modelWindow->forceRedraw();
	}
	return 1;
}

LRESULT LDViewWindow::windowProc(HWND hWnd, UINT message, WPARAM wParam,
								 LPARAM lParam)
{
	if (screenSaver && modelWindow &&
		((SSModelWindow*)modelWindow)->checkForExit(hWnd, message, wParam,
		lParam))
	{
		shutdown();
		return 1;
	}
	return CUIWindow::windowProc(hWnd, message, wParam, lParam);
}

void LDViewWindow::switchModes(void)
{
	if (hStatusBar)
	{
		removeStatusBar();
	}
	if (hToolbar)
	{
		removeToolbar();
	}
	skipMinimize = YES;
	if (!getCurrentVideoMode())
	{
		MessageBeep(MB_ICONEXCLAMATION);
		return;
	}
	modelWindow->uncompile();
	switchingModes = YES;
	if (modelWindow)
	{
		modelWindow->closeWindow();
//		modelWindowShown = false;
	}
	DestroyWindow(hWindow);
//	modelWindow->forceRedraw(3);
	switchingModes = NO;
	fullScreen = !fullScreen;
	if (fullScreen)
	{
//		debugPrintf("switching to fullscreen...\n");
		setFullScreenDisplayMode();
//		debugPrintf("done.\n");
	}
	else
	{
		int newWidth = TCUserDefaults::longForKey(WINDOW_WIDTH_KEY, 0, false);
		int newHeight = TCUserDefaults::longForKey(WINDOW_HEIGHT_KEY, 0, false);
		int dWidth = newWidth - width;
		int dHeight = newHeight - height;

//		debugPrintf("switching from fullscreen...\n");
		restoreDisplayMode();
//		debugPrintf("done\n");
		width = newWidth;
		height = newHeight;
		newWidth = modelWindow->getWidth() + dWidth;
		newHeight = modelWindow->getHeight() + dHeight;
		modelWindow->resize(newWidth, newHeight);
	}
	if (initWindow())
	{
//		if (modelWindow->initWindow())
		{
			SetWindowPos(modelWindow->getHWindow(), HWND_TOP, 0, 0,
				width, height, 0);
			showWindow(SW_SHOW);
			if (!fullScreen)
			{
//				debugPrintf("normal.\n");
			}
			modelWindow->setNeedsRecompile();
			skipMinimize = NO;
		}
/*
		else
		{
			skipMinimize = NO;
			if (fullScreen)
			{
				switchModes();
			}
			else
			{
				MessageBox(hWindow, "Error switching back.  Aborting.", "Error",
					MB_OK);
				shutdown();
			}
		}
*/
	}
	else
	{
		skipMinimize = NO;
		if (fullScreen)
		{
			switchModes();
		}
		else
		{
			MessageBox(hWindow, TCLocalStrings::get("SwitchBackError"),
				TCLocalStrings::get("Error"), MB_OK);
			shutdown();
		}
	}
	modelWindow->forceRedraw();
	SetActiveWindow(hWindow);
	debugPrintf(2, "0x0%08X: Just set active\n", hWindow);
}

void LDViewWindow::shutdown(void)
{
	skipMinimize = YES;
	if (modelWindow)
	{
		ModelWindow *tmpModelWindow = modelWindow;

		modelWindow = NULL;
		tmpModelWindow->closeWindow();
		tmpModelWindow->release();
//		modelWindowShown = false;
	}
	DestroyWindow(hWindow);
}

LRESULT LDViewWindow::doChar(TCHAR characterCode, LPARAM /*keyData*/)
{
	switch (characterCode)
	{
		case 10: // Ctrl+Enter (Linefeed)
			switchModes();
			return 0;
			break;
		case 27: // Escape
			if (fullScreen)
			{
				switchModes();
//				shutdown();
				return 0;
			}
			break;
	}
	return 1;
}

void LDViewWindow::selectFSVideoModeMenuItem(int index)
{
	VideoModeT* videoMode = getCurrentVideoMode();
	HMENU bitDepthMenu;
	MENUITEMINFO itemInfo;
	int menuIndex;
	HMENU viewMenu = GetSubMenu(GetMenu(hWindow), 2);

	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_STATE;
	if (videoMode)
	{
		bitDepthMenu = menuForBitDepth(hWindow, videoMode->depth, &menuIndex);
		itemInfo.fState = MFS_UNCHECKED;
		SetMenuItemInfo(bitDepthMenu, 30000 + currentVideoModeIndex, FALSE,
			&itemInfo);
		SetMenuItemInfo(viewMenu, menuIndex, TRUE, &itemInfo);
	}
	currentVideoModeIndex = index;
	videoMode = getCurrentVideoMode();
	fsWidth = videoMode->width;
	fsHeight = videoMode->height;
	fsDepth = videoMode->depth;
	TCUserDefaults::setLongForKey(fsWidth, FULLSCREEN_WIDTH_KEY);
	TCUserDefaults::setLongForKey(fsHeight, FULLSCREEN_HEIGHT_KEY);
	TCUserDefaults::setLongForKey(fsDepth, FULLSCREEN_DEPTH_KEY);
	if (videoMode)
	{
		bitDepthMenu = menuForBitDepth(hWindow, videoMode->depth, &menuIndex);
		itemInfo.fState = MFS_CHECKED;
		SetMenuItemInfo(bitDepthMenu, 30000 + currentVideoModeIndex, FALSE,
			&itemInfo);
		SetMenuItemInfo(viewMenu, menuIndex, TRUE, &itemInfo);
	}
}

void LDViewWindow::reflectPolling(void)
{
	selectPollingMenuItem(ID_FILE_POLLING_DISABLED +
		TCUserDefaults::longForKey(POLL_KEY, 0, false));
}

void LDViewWindow::initPollingMenu(void)
{
	int pollSetting = TCUserDefaults::longForKey(POLL_KEY, 0, false);
	HMENU pollingMenu = getPollingMenu();
	int i;
	int count = GetMenuItemCount(pollingMenu);
	MENUITEMINFO itemInfo;
	char title[256];

	memset(&itemInfo, 0, sizeof(MENUITEMINFO));
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_TYPE | MIIM_STATE;
	itemInfo.dwTypeData = title;
	for (i = 0; i < count; i++)
	{
		itemInfo.cch = 256;
		GetMenuItemInfo(pollingMenu, i, TRUE, &itemInfo);
		itemInfo.fType = MFT_STRING | MFT_RADIOCHECK;
		if (i == pollSetting)
		{
			itemInfo.fState = MFS_CHECKED;
		}
		else
		{
			itemInfo.fState = MFS_UNCHECKED;
		}
		SetMenuItemInfo(pollingMenu, i, TRUE, &itemInfo);
	}
}

void LDViewWindow::selectPollingMenuItem(int index)
{
	int i;
	HMENU pollingMenu = getPollingMenu();
	MENUITEMINFO itemInfo;

	for (i = ID_FILE_POLLING_DISABLED; i <= ID_FILE_POLLING_BACKGROUND; i++)
	{
		itemInfo.cbSize = sizeof(MENUITEMINFO);
		itemInfo.fMask = MIIM_STATE;
		if (i == index)
		{
			itemInfo.fState = MFS_CHECKED;
		}
		else
		{
			itemInfo.fState = MFS_UNCHECKED;
		}
		SetMenuItemInfo(pollingMenu, i, FALSE, &itemInfo);
	}
	TCUserDefaults::setLongForKey(index - ID_FILE_POLLING_DISABLED, POLL_KEY,
		false);
	if (modelWindow)
	{
		modelWindow->setPollSetting(index - ID_FILE_POLLING_DISABLED);
	}
}

HMENU LDViewWindow::getPollingMenu(void)
{
	HMENU fileMenu = GetSubMenu(GetMenu(hWindow), 0);
	int i;
	int count = GetMenuItemCount(fileMenu);
	HMENU pollingMenu = 0;
	MENUITEMINFO itemInfo;

	memset(&itemInfo, 0, sizeof(MENUITEMINFO));
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_SUBMENU | MIIM_ID;
	for (i = 0; i < count && !pollingMenu; i++)
	{
		GetMenuItemInfo(fileMenu, i, TRUE, &itemInfo);
		if (itemInfo.hSubMenu)
		{
			pollingMenu = itemInfo.hSubMenu;
//			pollingMenu = GetSubMenu(viewMenu, i);
			GetMenuItemInfo(pollingMenu, 0, TRUE, &itemInfo);
			if (itemInfo.wID != ID_FILE_POLLING_DISABLED)
			{
				pollingMenu = 0;
			}
		}
	}
	return pollingMenu;
}

void LDViewWindow::showHelp(void)
{
	HINSTANCE executeHandle;
	char* helpPath = LDViewPreferences::getLDViewPath("Help.html", true);

	setWaitCursor();
	executeHandle = ShellExecute(hWindow, NULL, helpPath, NULL, ".",
		SW_SHOWNORMAL);
	delete helpPath;
	setArrowCursor();
	if ((int)executeHandle <= 32)
	{
		const char* errorString;

		switch ((int)executeHandle)
		{
			case 0:
			case SE_ERR_OOM:
				errorString = TCLocalStrings::get("HelpHtmlOom");
				break;
			case ERROR_FILE_NOT_FOUND:
				errorString = TCLocalStrings::get("HelpHtmlFileNotFound");
				break;
			case ERROR_PATH_NOT_FOUND:
				errorString = TCLocalStrings::get("HelpHtmlPathNotFound");
				break;
			case SE_ERR_ACCESSDENIED:
				errorString = TCLocalStrings::get("HelpHtmlAccess");
				break;
			case SE_ERR_SHARE:
				errorString = TCLocalStrings::get("HelpHtmlShare");
				break;
			default:
				errorString = TCLocalStrings::get("HelpHtmlError");
				break;
		}
		MessageBox(hWindow, errorString, TCLocalStrings::get("Error"),
			MB_OK | MB_ICONEXCLAMATION);
	}
}

void LDViewWindow::openRecentFile(int index)
{
	char* filename = recentFiles->stringAtIndex(index);

	if (filename)
	{
		openModel(filename);
	}
}

void LDViewWindow::setMenuEnabled(HMENU hParentMenu, int itemID, bool enabled,
								  BOOL byPosition)
{
	MENUITEMINFO itemInfo;
	BYTE tbState = 0;

	itemInfo.cbSize = sizeof(itemInfo);
	itemInfo.fMask = MIIM_STATE;
	GetMenuItemInfo(hParentMenu, itemID, byPosition, &itemInfo);
	if (enabled)
	{
		itemInfo.fState &= ~MFS_DISABLED;
	}
	else
	{
		itemInfo.fState |= MFS_DISABLED;
	}
	itemInfo.fMask = MIIM_STATE;
	SetMenuItemInfo(hParentMenu, itemID, byPosition, &itemInfo);
	if (enabled)
	{
		tbState |= TBSTATE_ENABLED;
	}
	if (hToolbar)
	{
		SendMessage(hToolbar, TB_SETSTATE, (WPARAM)itemID,
			MAKELONG(tbState, 0));
	}
	else
	{
		int i;
		int count;

		populateTbButtonInfos();
		count = tbButtonInfos->getCount();
		for (i = 0; i < count; i++)
		{
			TbButtonInfo *buttonInfo = (*tbButtonInfos)[i];

			if (buttonInfo->getCommandId() == itemID)
			{
				buttonInfo->setState((BYTE)(tbState |
					(buttonInfo->getState() & ~TBSTATE_ENABLED)));
				break;
			}
		}
	}
}

void LDViewWindow::updateModelMenuItems(void)
{
	bool haveModel = modelIsLoaded();

	setMenuEnabled(hFileMenu, ID_FILE_SAVE, haveModel);
	setMenuEnabled(hFileMenu, ID_FILE_RELOAD, haveModel);
	setMenuEnabled(hFileMenu, ID_FILE_PRINT, haveModel);
	setMenuEnabled(hFileMenu, ID_FILE_PAGESETUP, haveModel);
}

bool LDViewWindow::modelIsLoaded(void)
{
	if (modelWindow)
	{
		LDrawModelViewer* modelViewer = modelWindow->getModelViewer();

		if (modelViewer && modelViewer->getMainTREModel())
		{
			return true;
		}
	}
	return false;
}

LRESULT LDViewWindow::doMenuSelect(UINT menuID, UINT /*flags*/, HMENU hMenu)
{
//	debugPrintf("LDViewWindow::doMenuSelect(%d, 0x%04X, 0x%04X)\n", menuID,
//		flags, hMenu);
	if (hMenu == GetMenu(hWindow) && menuID == 0)
	{
		// This shouldn't ever be necessary, but it can't hurt.
		updateModelMenuItems();
	}
	return 1;
}

void cleanupMatrix(float *matrix)
{
	int i;

	for (i = 0; i < 16; i++)
	{
		if (fabs(matrix[i]) < 1e-6)
		{
			matrix[i] = 0.0f;
		}
	}
}

/*
void LDViewWindow::showDefaultMatrix(const char *matrixString,
									 const char *title)
{
	char buf[1024];

	sprintf(buf, "Hit OK to copy the following matrix to the clipboard:\n\n"
		"%s\n\n(You can use this on the command line with the -DefaultMatrix "
		"command line option.)", matrixString);
	if (MessageBox(hWindow, buf, title, MB_OKCANCEL) ==
		IDOK)
	{
		copyToClipboard(matrixString);
	}
}

void LDViewWindow::showTransformationMatrix(void)
{
	if (modelWindow)
	{
		LDrawModelViewer* modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			float matrix[16];
			float rotationMatrix[16];
			float otherMatrix[16] = {1,0,0,0,0,-1,0,0,0,0,-1,0,0,0,0,1};
			char matrixString[1024];
			TRECamera &camera = modelViewer->getCamera();
			TCVector cameraPosition = camera.getPosition();

			memcpy(rotationMatrix, modelViewer->getRotationMatrix(),
				sizeof(rotationMatrix));
			TCVector::multMatrix(otherMatrix, rotationMatrix, matrix);
			matrix[12] = cameraPosition[0];
			matrix[13] = cameraPosition[1];
			matrix[14] = cameraPosition[2];
			cleanupMatrix(matrix);
			sprintf(matrixString,
				"%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,"
				"%.6g,%.6g,%.6g,%.6g",
				matrix[0], matrix[4], matrix[8], matrix[12],
				matrix[1], matrix[5], matrix[9], matrix[13],
				matrix[2], matrix[6], matrix[10], matrix[14],
				matrix[3], matrix[7], matrix[11], matrix[15]);
			showDefaultMatrix(matrixString, "Transformation Matrix");
		}
	}
}
*/

void LDViewWindow::showViewInfo(void)
{
	if (modelWindow)
	{
		LDrawModelViewer* modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			float matrix[16];
			float rotationMatrix[16];
			float otherMatrix[16] = {1,0,0,0,0,-1,0,0,0,0,-1,0,0,0,0,1};
			char matrixString[1024];
			char zoomString[128];
			char message[4096];
			TRECamera &camera = modelViewer->getCamera();
			float defaultDistance = modelViewer->getDefaultDistance();
			float distanceMultiplier = modelViewer->getDistanceMultiplier();
			float cameraDistance;

			memcpy(rotationMatrix, modelViewer->getRotationMatrix(),
				sizeof(rotationMatrix));
			TCVector::multMatrix(otherMatrix, rotationMatrix, matrix);
			cleanupMatrix(matrix);
			sprintf(matrixString,
				"%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g", matrix[0],
				matrix[4], matrix[8], matrix[1], matrix[5], matrix[9],
				matrix[2], matrix[6], matrix[10]);
			cameraDistance = camera.getPosition().length();
			if (distanceMultiplier == 0.0f || cameraDistance == 0.0f)
			{
				// If we don't have a model, we don't know the default zoom, so
				// just say 1.
				strcpy(zoomString, "1");
			}
			else
			{
				sprintf(zoomString, "%.6g", defaultDistance /
					distanceMultiplier / cameraDistance);
			}
			sprintf(message, TCLocalStrings::get("ViewInfoMessage"),
				matrixString, zoomString);
			if (MessageBox(hWindow, message,
				TCLocalStrings::get("ViewInfoTitle"), MB_OKCANCEL) == IDOK)
			{
				char commandLine[1024];

				sprintf(commandLine, "-DefaultMatrix=%s -DefaultZoom=%s",
					matrixString, zoomString);
				copyToClipboard(commandLine);
			}
		}
	}
}

/*
void LDViewWindow::showRotationMatrix(void)
{
	if (modelWindow)
	{
		LDrawModelViewer* modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			float matrix[16];
			float rotationMatrix[16];
			float otherMatrix[16] = {1,0,0,0,0,-1,0,0,0,0,-1,0,0,0,0,1};
			char matrixString[1024];

			memcpy(rotationMatrix, modelViewer->getRotationMatrix(),
				sizeof(rotationMatrix));
			TCVector::multMatrix(otherMatrix, rotationMatrix, matrix);
			cleanupMatrix(matrix);
			sprintf(matrixString,
				"%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g", matrix[0],
				matrix[4], matrix[8], matrix[1], matrix[5], matrix[9],
				matrix[2], matrix[6], matrix[10]);
			showDefaultMatrix(matrixString, "Rotation Matrix");
		}
	}
}
*/

void LDViewWindow::showLDrawCommandLine(void)
{
	if (modelWindow)
	{
		LDrawModelViewer* modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			HGLOBAL hBuf = GlobalAlloc(GMEM_DDESHARE, 1024);
			char *buf = (char*)GlobalLock(hBuf);
//			char shortFilename[1024];

//			GetShortPathName(modelViewer->getFilename(), shortFilename, 1024);
//			modelViewer->getLDrawCommandLine(shortFilename, buf, 1024);
			modelViewer->getLDGLiteCommandLine(buf, 1024);
			MessageBox(hWindow, buf, TCLocalStrings::get("LDrawCommandLine"),
				MB_OK);
//			debugPrintf("%s\n", buf);
			GlobalUnlock(hBuf);
			if (OpenClipboard(hWindow))
			{
				EmptyClipboard();
				SetClipboardData(CF_TEXT, hBuf);
				CloseClipboard();
//				GlobalFree(hBuf);
			}
		}
	}
}

BOOL LDViewWindow::doDialogSize(HWND hDlg, WPARAM sizeType, int newWidth,
							   int newHeight)
{
//	debugPrintf("LDViewWindow::doDialogSize(%d, %d, %d)\n", sizeType, newWidth,
//		newHeight);
	if (hDlg == hOpenGLInfoWindow)
	{
		SendMessage(hOpenGLStatusBar, WM_SIZE, sizeType,
			MAKELPARAM(newWidth, newHeight));
		openGLInfoWindoResizer->resize(newWidth, newHeight);
	}
	return FALSE;
}

LRESULT LDViewWindow::showOpenGLDriverInfo(void)
{
	if (!hOpenGLInfoWindow)
	{
//		LDrawModelViewer *modelViewer = modelWindow->getModelViewer();
		int numOpenGlExtensions;
		char *openGlMessage = modelWindow->getModelViewer()->
			getOpenGLDriverInfo(numOpenGlExtensions);
		const char *wglExtensionsString =
			LDVExtensionsSetup::getWglExtensions();
		char *wglExtensionsList;
		int len;
		char *message;
		int parts[2] = {100, -1};
		char buf[128];
		int count;

		if (!wglExtensionsString)
		{
			wglExtensionsString = TCLocalStrings::get("*None*");
		}
		wglExtensionsList = stringByReplacingSubstring(wglExtensionsString,
			" ", "\r\n");
		stripCRLF(wglExtensionsList);
		len = strlen(openGlMessage) + strlen(wglExtensionsList) + 128;
		message = new char[len];
		sprintf(message, TCLocalStrings::get("OpenGl+WglInfo"), openGlMessage,
			wglExtensionsList);
		hOpenGLInfoWindow = createDialog(IDD_OPENGL_INFO);
		SendDlgItemMessage(hOpenGLInfoWindow, IDC_OPENGL_INFO, WM_SETTEXT, 0,
			(LPARAM)message);
		hOpenGLStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE |
			SBARS_SIZEGRIP, "", hOpenGLInfoWindow, ID_TOOLBAR);
		SendMessage(hOpenGLStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
		if (numOpenGlExtensions == 1)
		{
			strcpy(buf, TCLocalStrings::get("OpenGl1Extension"));
		}
		else
		{
			sprintf(buf, TCLocalStrings::get("OpenGlnExtensions"),
				numOpenGlExtensions);
		}
		SendMessage(hOpenGLStatusBar, SB_SETTEXT, 0, (LPARAM)buf);
		count = countStringLines(wglExtensionsList);
		if (count == 1)
		{
			strcpy(buf, TCLocalStrings::get("OpenGl1WglExtension"));
		}
		else
		{
			sprintf(buf, TCLocalStrings::get("OpenGlnWglExtensions"), count);
		}
		SendMessage(hOpenGLStatusBar, SB_SETTEXT, 1, (LPARAM)buf);
		calcSystemSizes();
		if (openGLInfoWindoResizer)
		{
			openGLInfoWindoResizer->release();
		}
		openGLInfoWindoResizer = new CUIWindowResizer;
		openGLInfoWindoResizer->setHWindow(hOpenGLInfoWindow);
		openGLInfoWindoResizer->addSubWindow(IDC_OPENGL_INFO,
			CUISizeHorizontal | CUISizeVertical);
		openGLInfoWindoResizer->addSubWindow(IDOK, CUIFloatLeft | CUIFloatTop);
		delete message;
		delete wglExtensionsList;
		delete openGlMessage;
	}
	ShowWindow(hOpenGLInfoWindow, SW_SHOW);
	return 0;
}

/*
LRESULT LDViewWindow::showOpenGLDriverInfo(void)
{
	if (!hOpenGLInfoWindow)
	{
		const char *vendorString = (const char*)glGetString(GL_VENDOR);
		const char *rendererString = (const char*)glGetString(GL_RENDERER);
		const char *versionString = (const char*)glGetString(GL_VERSION);
		const char *extensionsString = (const char*)glGetString(GL_EXTENSIONS);
		const char *wglExtensionsString =
			LDVExtensionsSetup::getWglExtensions();
		char *extensionsList;
		char *wglExtensionsList;
		int len;
		char *message;
		int parts[2] = {100, -1};
		char buf[128];
		int count;

		if (!vendorString)
		{
			vendorString = TCLocalStrings::get("*Unknown*");
		}
		if (!rendererString)
		{
			rendererString = TCLocalStrings::get("*Unknown*");
		}
		if (!versionString)
		{
			versionString = TCLocalStrings::get("*Unknown*");
		}
		if (!extensionsString)
		{
			extensionsString = TCLocalStrings::get("*None*");
		}
		if (!wglExtensionsString)
		{
			wglExtensionsString = TCLocalStrings::get("*None*");
		}
		extensionsList = stringByReplacingSubstring(extensionsString, " ",
			"\r\n");
		wglExtensionsList = stringByReplacingSubstring(wglExtensionsString,
			" ", "\r\n");
		stripCRLF(extensionsList);
		stripCRLF(wglExtensionsList);
		len = strlen(vendorString) + strlen(rendererString) +
			strlen(versionString) + strlen(extensionsList) +
			strlen(wglExtensionsList) + 128;
		message = new char[len];
		sprintf(message, TCLocalStrings::get("OpenGlInfo"),
			vendorString, rendererString, versionString, extensionsList,
			wglExtensionsList);
		hOpenGLInfoWindow = createDialog(IDD_OPENGL_INFO);
		SendDlgItemMessage(hOpenGLInfoWindow, IDC_OPENGL_INFO, WM_SETTEXT, 0,
			(LPARAM)message);
		hOpenGLStatusBar = CreateStatusWindow(WS_CHILD | WS_VISIBLE |
			SBARS_SIZEGRIP, "", hOpenGLInfoWindow, ID_TOOLBAR);
		SendMessage(hOpenGLStatusBar, SB_SETPARTS, 2, (LPARAM)parts);
		count = countStringLines(extensionsList);
		if (count == 1)
		{
			strcpy(buf, TCLocalStrings::get("OpenGl1Extension"));
		}
		else
		{
			sprintf(buf, TCLocalStrings::get("OpenGlnExtensions"), count);
		}
		SendMessage(hOpenGLStatusBar, SB_SETTEXT, 0, (LPARAM)buf);
		count = countStringLines(wglExtensionsList);
		if (count == 1)
		{
			strcpy(buf, TCLocalStrings::get("OpenGl1WglExtension"));
		}
		else
		{
			sprintf(buf, TCLocalStrings::get("OpenGlnWglExtensions"), count);
		}
		SendMessage(hOpenGLStatusBar, SB_SETTEXT, 1, (LPARAM)buf);
		calcSystemSizes();
		if (openGLInfoWindoResizer)
		{
			openGLInfoWindoResizer->release();
		}
		openGLInfoWindoResizer = new CUIWindowResizer;
		openGLInfoWindoResizer->setHWindow(hOpenGLInfoWindow);
		openGLInfoWindoResizer->addSubWindow(IDC_OPENGL_INFO,
			CUISizeHorizontal | CUISizeVertical);
		openGLInfoWindoResizer->addSubWindow(IDOK, CUIFloatLeft | CUIFloatTop);
		delete message;
		delete extensionsList;
		delete wglExtensionsList;
	}
	ShowWindow(hOpenGLInfoWindow, SW_SHOW);
	return 0;
}
*/


/*
{
	int pollSetting = TCUserDefaults::longForKey(POLL_KEY, 0, false);
	HMENU pollingMenu = getPollingMenu();
	int i;
	int count = GetMenuItemCount(pollingMenu);
	MENUITEMINFO itemInfo;
	char title[256];

	memset(&itemInfo, 0, sizeof(MENUITEMINFO));
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_TYPE | MIIM_STATE;
	itemInfo.dwTypeData = title;
	for (i = 0; i < count; i++)
	{
		itemInfo.cch = 256;
		GetMenuItemInfo(pollingMenu, i, TRUE, &itemInfo);
		itemInfo.fType = MFT_STRING | MFT_RADIOCHECK;
		if (i == pollSetting)
		{
			itemInfo.fState = MFS_CHECKED;
		}
		else
		{
			itemInfo.fState = MFS_UNCHECKED;
		}
		SetMenuItemInfo(pollingMenu, i, TRUE, &itemInfo);
	}
}
*/

void LDViewWindow::setMenuRadioCheck(HMENU hParentMenu, UINT uItem, bool checked)
{
	setMenuCheck(hParentMenu, uItem, checked, true);
}

bool LDViewWindow::getMenuCheck(HMENU hParentMenu, UINT uItem)
{
	MENUITEMINFO itemInfo;

	memset(&itemInfo, 0, sizeof(MENUITEMINFO));
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_STATE;
	GetMenuItemInfo(hParentMenu, uItem, FALSE, &itemInfo);
	return (itemInfo.fState & MFS_CHECKED) != 0;
}

void LDViewWindow::setMenuCheck(HMENU hParentMenu, UINT uItem, bool checked,
								bool radio)
{
	MENUITEMINFO itemInfo;
	char title[256];

	memset(&itemInfo, 0, sizeof(MENUITEMINFO));
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_STATE | MIIM_TYPE;
	itemInfo.dwTypeData = title;
	itemInfo.cch = 256;
	GetMenuItemInfo(hParentMenu, uItem, FALSE, &itemInfo);
	if (checked)
	{
		itemInfo.fState = MFS_CHECKED;
	}
	else
	{
		itemInfo.fState = MFS_UNCHECKED;
	}
	itemInfo.fType = MFT_STRING;
	if (radio)
	{
		itemInfo.fType |= MFT_RADIOCHECK;
	}
	SetMenuItemInfo(hParentMenu, uItem, FALSE, &itemInfo);
}

LRESULT LDViewWindow::switchToExamineMode(void)
{
	setMenuRadioCheck(hViewMenu, ID_VIEW_EXAMINE, true);
	setMenuRadioCheck(hViewMenu, ID_VIEW_FLYTHROUGH, false);
	modelWindow->setViewMode(LDVViewExamine);
	showStatusIcon(true);
	return 0;
}

LRESULT LDViewWindow::switchToFlythroughMode(void)
{
	setMenuRadioCheck(hViewMenu, ID_VIEW_EXAMINE, false);
	setMenuRadioCheck(hViewMenu, ID_VIEW_FLYTHROUGH, true);
	modelWindow->setViewMode(LDVViewFlythrough);
	showStatusIcon(false);
	return 0;
}

void LDViewWindow::updateEdgesMenu(void)
{
	setMenuCheck(hEdgesMenu, ID_EDGES_SHOWEDGESONLY, prefs->getEdgesOnly());
	setMenuCheck(hEdgesMenu, ID_EDGES_CONDITIONALLINES,
		prefs->getDrawConditionalHighlights());
	setMenuCheck(hEdgesMenu, ID_EDGES_HIGHQUALITY,
		prefs->getUsePolygonOffset());
	setMenuCheck(hEdgesMenu, ID_EDGES_ALWAYSBLACK, prefs->getBlackHighlights());
	setMenuItemsEnabled(hEdgesMenu, edges);
}

void LDViewWindow::doToolbarDropDown(LPNMTOOLBAR toolbarNot)
{
	RECT rect;
	TPMPARAMS tpm;
	HMENU hMenu = NULL;

    SendMessage(toolbarNot->hdr.hwndFrom, TB_GETRECT, (WPARAM)toolbarNot->iItem,
		(LPARAM)&rect);
	MapWindowPoints(toolbarNot->hdr.hwndFrom, HWND_DESKTOP, (LPPOINT)&rect, 2);
	tpm.cbSize = sizeof(TPMPARAMS);
	tpm.rcExclude.top    = rect.top;
	tpm.rcExclude.left   = rect.left;
	tpm.rcExclude.bottom = rect.bottom;
	tpm.rcExclude.right  = rect.right;
	switch (toolbarNot->iItem)
	{
	case IDC_HIGHLIGHTS:
		hMenu = hEdgesMenu;
		break;
	case ID_VIEWANGLE:
		hMenu = hViewAngleMenu;
		break;
	}
	if (hMenu)
	{
		TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_LEFTBUTTON |
			TPM_VERTICAL, rect.left, rect.bottom, hWindow, &tpm);
	}
}

LRESULT LDViewWindow::doNotify(int /*controlId*/, LPNMHDR notification)
{
//	debugPrintf("LDViewWindow::doNotify: 0x%04X, 0x%04x\n", controlId,
//		notification->code);
	switch (notification->code)
	{
	case TTN_GETDISPINFO:
		{
			LPNMTTDISPINFO dispInfo = (LPNMTTDISPINFO)notification;
			int i;
			int count = tbButtonInfos->getCount();

			for (i = 0; i < count; i++)
			{
				TbButtonInfo *buttonInfo = (*tbButtonInfos)[i];

				if ((DWORD)buttonInfo->getCommandId() == notification->idFrom)
				{
					if (CUIThemes::isThemeLibLoaded())
					{
						// Turning off theme support in the tooltip makes it
						// work properly.  With theme support on, it gets erased
						// by the OpenGL window immediately after being drawn.
						// Haven't the foggiest why this happens, but turning
						// off theme support solves the problem.  This has to
						// be done ever time the tooltip is about to pop up.
						// Not sure why that is either, but it is.
						CUIThemes::setWindowTheme(notification->hwndFrom, NULL,
							L"");
					}
					strcpy(dispInfo->szText, buttonInfo->getTooltipText());
					break;
				}
			}
			dispInfo->hinst = NULL;
		}
		break;
	case TBN_DROPDOWN:
		if (notification->idFrom == ID_TOOLBAR)
		{
			doToolbarDropDown((LPNMTOOLBAR)notification);
		}
		break;
	}
	return 0;
}

BOOL LDViewWindow::doDialogNotify(HWND hDlg, int controlId,
								  LPNMHDR notification)
{
//	debugPrintf("LDViewWindow::doDialogNotify: 0x%04X, 0x%04X, 0x%04x\n", hDlg,
//		controlId, notification->code);
	if (hDlg)
	{
		if (hDlg == hExtraDirsWindow)
		{
			if (controlId >= 42 && controlId <= 45)
			{
				switch (notification->code)
				{
				case TTN_GETDISPINFO:
					{
						LPNMTTDISPINFO dispInfo = (LPNMTTDISPINFO)notification;
						bool gotTooltip = true;

						switch (controlId)
						{
						case 42:
							strcpy(dispInfo->szText,
								TCLocalStrings::get("AddExtraDirTooltip"));
							break;
						case 43:
							strcpy(dispInfo->szText,
								TCLocalStrings::get("RemoveExtraDirTooltip"));
							break;
						case 44:
							strcpy(dispInfo->szText,
								TCLocalStrings::get("MoveExtraDirUpTooltip"));
							break;
						case 45:
							strcpy(dispInfo->szText,
								TCLocalStrings::get("MoveExtraDirDownTooltip"));
							break;
						default:
							gotTooltip = false;
							break;
						}
						if (gotTooltip && CUIThemes::isThemeLibLoaded())
						{
							// Turning off theme support in the tooltip makes it
							// work properly.  With theme support on, it gets
							// erased by the OpenGL window immediately after
							// being drawn if it overlaps the OpenGL window.
							// Haven't the foggiest why this happens, but
							// turning off theme support solves the problem.
							// This has to be done ever time the tooltip is
							// about to pop up. Not sure why that is either, but
							// it is.
							CUIThemes::setWindowTheme(notification->hwndFrom,
								NULL, L"");
						}
						dispInfo->hinst = NULL;
					}
					break;
				case WM_COMMAND:
					debugPrintf("WM_COMMAND\n");
					break;
				}
			}
		}
	}
	return FALSE;
}

void LDViewWindow::populateExtraDirsListBox(void)
{
	int i;
	int count = extraSearchDirs->getCount();

	SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_LIST, LB_RESETCONTENT, 0, 0);
	for (i = 0; i < count; i++)
	{
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_LIST, LB_ADDSTRING, 0,
			(LPARAM)(*extraSearchDirs)[i]);
	}
	if (count)
	{
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_LIST, LB_SELECTSTRING,
			0, (LPARAM)(*extraSearchDirs)[0]);
	}
}

void LDViewWindow::updateExtraDirsEnabled(void)
{
	int index = SendMessage(hExtraDirsList, LB_GETCURSEL, 0, 0);

	if (index == LB_ERR)
	{
		int i;

		for (i = 1; i < 4; i++)
		{
			SendMessage(hExtraDirsToolbar, TB_SETSTATE, 42 + i,
				MAKELONG(0, 0));
		}
	}
	else
	{
		// There's a selection; therefore it can be deleted.
		SendMessage(hExtraDirsToolbar, TB_SETSTATE, 43,
			MAKELONG(TBSTATE_ENABLED, 0));
		if (index == 0)
		{
			// Can't move up from the top
			SendMessage(hExtraDirsToolbar, TB_SETSTATE, 44, MAKELONG(0, 0));
		}
		else
		{
			SendMessage(hExtraDirsToolbar, TB_SETSTATE, 44,
				MAKELONG(TBSTATE_ENABLED, 0));
		}
		if (index == extraSearchDirs->getCount() - 1)
		{
			// Can't move down from the bottom
			SendMessage(hExtraDirsToolbar, TB_SETSTATE, 45, MAKELONG(0, 0));
		}
		else
		{
			SendMessage(hExtraDirsToolbar, TB_SETSTATE, 45,
				MAKELONG(TBSTATE_ENABLED, 0));
		}
	}
}

void LDViewWindow::chooseExtraDirs(void)
{
	if (!hExtraDirsWindow)
	{
		TBADDBITMAP addBitmap;
		TBBUTTON buttons[4];
		char buttonTitle[128];
		int i;
		RECT tbRect;

		memset(buttonTitle, 0, sizeof(buttonTitle));
		strcpy(buttonTitle, "");
		ModelWindow::initCommonControls(ICC_WIN95_CLASSES);
		hExtraDirsWindow = createDialog(IDD_EXTRA_DIRS);
		hExtraDirsToolbar = GetDlgItem(hExtraDirsWindow, IDC_ESD_TOOLBAR);
		hExtraDirsList = GetDlgItem(hExtraDirsWindow, IDC_ESD_LIST);
		populateExtraDirsListBox();
		GetClientRect(hExtraDirsToolbar, &tbRect);
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_TOOLBAR,
			TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0); 
		addBitmap.hInst = getLanguageModule();
		addBitmap.nID = IDB_EXTRA_DIRS;
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_TOOLBAR, TB_SETINDENT,
			tbRect.right - tbRect.left - 100, 0);
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_TOOLBAR, TB_SETBUTTONWIDTH,
			0, MAKELONG(25, 25));
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_TOOLBAR, TB_ADDBITMAP, 4,
			(LPARAM)&addBitmap);
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_TOOLBAR, TB_ADDSTRING,
			0, (LPARAM)buttonTitle);
		for (i = 0; i < 4; i++)
		{
			buttons[i].iBitmap = i;
			buttons[i].idCommand = 42 + i;
			buttons[i].fsState = TBSTATE_ENABLED;
			buttons[i].fsStyle = TBSTYLE_BUTTON;
			buttons[i].dwData = (DWORD)this;
			buttons[i].iString = -1;
		}
		if (extraSearchDirs->getCount() < 2)
		{
			// Can't move down either.
			buttons[3].fsState = 0;
		}
		SendDlgItemMessage(hExtraDirsWindow, IDC_ESD_TOOLBAR, TB_ADDBUTTONS,
			4, (LPARAM)buttons);
		updateExtraDirsEnabled();
	}
	ShowWindow(hExtraDirsWindow, SW_SHOW);
}

void LDViewWindow::chooseNewLDrawDir(void)
{
	char *oldDir = getLDrawDir();

	if (!verifyLDrawDir(true))
	{
		if (oldDir)
		{
			TCUserDefaults::setStringForKey(oldDir, LDRAWDIR_KEY, false);
			LDLModel::setLDrawDir(oldDir);
		}
	}
	delete oldDir;
}

void LDViewWindow::reshapeModelWindow(void)
{
	if (modelWindow)
	{
		int newX = 0;
		int newY = getToolbarHeight();
		int newWidth = width;
		int newHeight = height - getDockedHeight();

		SetWindowPos(modelWindow->getHWindow(), HWND_TOP, newX, newY, newWidth,
			newHeight, 0);
/*
		SetWindowPos(modelWindow->getHWindow(), HWND_TOP, 2, 2, newWidth - 4,
			newHeight - 4, 0);
		SetWindowPos(hFrameWindow, HWND_BOTTOM, 0, 0, newWidth, newHeight,
			SWP_DRAWFRAME | SWP_NOZORDER);
//		MoveWindow(hFrameWindow, 0, 0, newWidth, newHeight, TRUE);
		RedrawWindow(hFrameWindow, NULL, NULL, RDW_FRAME | RDW_INVALIDATE |
			RDW_INTERNALPAINT | RDW_UPDATENOW);
*/
	}
}

void LDViewWindow::removeStatusBar(void)
{
	HWND hActiveWindow = GetActiveWindow();
	bool needActive = hActiveWindow == hStatusBar;

	debugPrintf(2, "0x%08X: removing status bar: 0x%08X\n", hWindow, hStatusBar);
	if (modelWindow)
	{
		modelWindow->setStatusBar(NULL);
		modelWindow->setProgressBar(NULL);
	}
	DestroyWindow(hStatusBar);
	hStatusBar = NULL;
	reshapeModelWindow();
	if (needActive)
	{
		SetActiveWindow(hWindow);
	}
}

int LDViewWindow::getDockedHeight(void)
{
	return getToolbarHeight() + getStatusBarHeight();
}

int LDViewWindow::getModelWindowTop(void)
{
	return getToolbarHeight();
}

int LDViewWindow::getToolbarHeight(void)
{
	if (hToolbar)
	{
		RECT rect;

		GetWindowRect(hToolbar, &rect);
		return rect.bottom - rect.top;
	}
	else
	{
		return 0;
	}
}

int LDViewWindow::getStatusBarHeight(void)
{
	if (hStatusBar)
	{
		RECT rect;

		GetWindowRect(hStatusBar, &rect);
		return rect.bottom - rect.top;
	}
	else
	{
		return 0;
	}
}

void LDViewWindow::removeToolbar(void)
{
	HWND hActiveWindow = GetActiveWindow();
	bool needActive = hActiveWindow == hToolbar;

	debugPrintf(2, "0x%08X: removing toolbar: 0x%08X\n", hWindow, hToolbar);
/*
	if (modelWindow)
	{
		modelWindow->setToolbar(NULL);
	}
*/
	DestroyWindow(hToolbar);
	hToolbar = NULL;
	reshapeModelWindow();
	if (needActive)
	{
		SetActiveWindow(hWindow);
	}
}

void LDViewWindow::addToolbar(void)
{
	createToolbar();
	reshapeModelWindow();
}

LRESULT LDViewWindow::switchToolbar(void)
{
	showToolbar = !showToolbar;
	TCUserDefaults::setLongForKey(showToolbar ? 1 : 0, TOOLBAR_KEY, false);
	reflectToolbar();
	return 0;
}

void LDViewWindow::addStatusBar(void)
{
	createStatusBar();
	reshapeModelWindow();
}

LRESULT LDViewWindow::switchStatusBar(void)
{
	showStatusBar = !showStatusBar;
	TCUserDefaults::setLongForKey(showStatusBar ? 1 : 0, STATUS_BAR_KEY, false);
	reflectStatusBar();
	return 0;
}

void LDViewWindow::reflectTopmost(void)
{
	bool oldTopmost = getMenuCheck(hViewMenu, ID_VIEW_ALWAYSONTOP);

	if (oldTopmost != topmost)
	{
		HWND hPlaceFlag = HWND_NOTOPMOST;
		RECT rect;

		GetWindowRect(hWindow, &rect);
		if (topmost)
		{
			setMenuCheck(hViewMenu, ID_VIEW_ALWAYSONTOP, true);
			hPlaceFlag = HWND_TOPMOST;
		}
		else
		{
			setMenuCheck(hViewMenu, ID_VIEW_ALWAYSONTOP, false);
		}
		SetWindowPos(hWindow, hPlaceFlag, rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top, 0);
	}
}

LRESULT LDViewWindow::switchTopmost(void)
{
	topmost = !topmost;
	TCUserDefaults::setLongForKey(topmost ? 1 : 0, TOPMOST_KEY, false);
	reflectTopmost();
	return 0;
}

void LDViewWindow::reflectVisualStyle(void)
{
	if (CUIThemes::isThemeLibLoaded())
	{
		setMenuCheck(hViewMenu, ID_VIEW_VISUALSTYLE, visualStyleEnabled);
	}
}

LRESULT LDViewWindow::switchVisualStyle(void)
{
	visualStyleEnabled = !visualStyleEnabled;
	TCUserDefaults::setLongForKey(visualStyleEnabled ? 1 : 0,
		VISUAL_STYLE_ENABLED_KEY, false);
	reflectVisualStyle();
	return 0;
}

/*
LRESULT LDViewWindow::doTimer(UINT timerID)
{
	switch (timerID)
	{
	case DOWNLOAD_TIMER:
		TCThreadManager *threadManager = TCThreadManager::threadManager();

		if (threadManager->timedWaitForFinishedThread(0))
		{
			TCThread *finishedThread;

			while ((finishedThread = threadManager->getFinishedThread()) !=
				NULL)
			{
				threadManager->removeFinishedThread(finishedThread);
			}
		}
		break;
	}
	return 0;
}
*/

void LDViewWindow::doLibraryUpdateFinished(int finishType)
{
	if (libraryUpdater)
	{
		TCThreadManager *threadManager = TCThreadManager::threadManager();
		bool gotFinish = false;
		DWORD dwStartTicks = GetTickCount();

		while (!gotFinish)
		{
			if (threadManager->timedWaitForFinishedThread(250))
			{
				TCThread *finishedThread;

				while ((finishedThread = threadManager->getFinishedThread()) !=
					NULL)
				{
					threadManager->removeFinishedThread(finishedThread);
					gotFinish = true;
				}
			}
			if (GetTickCount() - dwStartTicks > 1000)
			{
				break;
			}
			if (!gotFinish)
			{
				Sleep(50);
			}
		}
		if (!gotFinish)
		{
			debugPrintf("No finished thread!!!\n");
		}
		if (libraryUpdater->getError() && strlen(libraryUpdater->getError()))
		{
			char error[1024];

			strcpy(error, libraryUpdater->getError());
			MessageBox(hWindow, error,
				TCLocalStrings::get("LibraryUpdateError"), MB_OK);
		}
		libraryUpdater->release();
		libraryUpdater = NULL;
		SendMessage(modelWindow->getProgressBar(), PBM_SETPOS, 0, 0);
		switch (finishType)
		{
		case LIBRARY_UPDATE_FINISHED:
			MessageBox(hWindow, TCLocalStrings::get("LibraryUpdateComplete"),
				"LDView", MB_OK);
			break;
		case LIBRARY_UPDATE_CANCELED:
			MessageBox(hWindow, TCLocalStrings::get("LibraryUpdateCanceled"),
				"LDView", MB_OK);
			break;
		case LIBRARY_UPDATE_NONE:
			MessageBox(hWindow, TCLocalStrings::get("LibraryUpdateUnnecessary"),
				"LDView", MB_OK);
			break;
		}
	}
}

/*
void LDViewWindow::fetchHeaderFinish(TCWebClient* webClient)
{
	debugPrintf("fetchHeaderFinish: 0x%08X\n", GetCurrentThreadId());
	webClient->fetchURLInBackground();
}
*/

void LDViewWindow::checkForLibraryUpdates(void)
{
	if (libraryUpdater)
	{
		MessageBox(hWindow, TCLocalStrings::get("LibraryUpdateAlready"),
			TCLocalStrings::get("Error"), MB_OK);
	}
	else
	{
		libraryUpdater = new LDLibraryUpdater;
		char *ldrawDir = getLDrawDir();
		
		libraryUpdater->setLibraryUpdateKey(LAST_LIBRARY_UPDATE_KEY);
		libraryUpdater->setLdrawDir(ldrawDir);
		delete ldrawDir;
		libraryUpdater->checkForUpdates();
	}
}

LRESULT LDViewWindow::doCommand(int itemId, int notifyCode, HWND controlHWnd)
{
//	char* message = NULL;

	if (modelWindow && controlHWnd == modelWindow->getHPrefsWindow())
	{
		SendMessage(modelWindow->getHWindow(), WM_COMMAND,
			MAKELONG(BN_CLICKED, notifyCode),
			(LPARAM)modelWindow->getHPrefsWindow());
	}
//	debugPrintf("LDViewWindow::doCommand(%d, %0x%08X, 0x%04X)\n", itemId,
//		notifyCode, controlHWnd);
	switch (itemId)
	{
		case ID_EDIT_PREFERENCES:
			modelWindow->showPreferences();
			return 0;
			break;
		case ID_FILE_OPEN:
			openModel();
			return 0;
			break;
		case ID_FILE_PRINT:
			printModel();
			return 0;
			break;
		case ID_FILE_PAGESETUP:
			pageSetup();
			return 0;
			break;
		case ID_FILE_SAVE:
			saveSnapshot();
			return 0;
			break;
		case ID_FILE_LDRAWDIR:
			chooseNewLDrawDir();
			return 0;
			break;
		case ID_FILE_EXTRADIRS:
			chooseExtraDirs();
			return 0;
			break;
		case ID_FILE_CANCELLOAD:
			if (modelWindow)
			{
				modelWindow->setCancelLoad();
			}
			return 0;
			break;
		case ID_FILE_EXIT:
			shutdown();
			return 0;
			break;
		case ID_FILE_CHECKFORLIBUPDATES:
			checkForLibraryUpdates();
			return 0;
			break;
		case ID_VIEW_FULLSCREEN:
			switchModes();
			return 0;
			break;
		case ID_VIEW_RESET:
			resetView();
			return 0;
			break;
		case ID_VIEW_DEFAULT:
			resetView();
			return 0;
			break;
		case ID_VIEW_FRONT:
			resetView(LDVAngleFront);
			return 0;
			break;
		case ID_VIEW_BACK:
			resetView(LDVAngleBack);
			return 0;
			break;
		case ID_VIEW_LEFT:
			resetView(LDVAngleLeft);
			return 0;
			break;
		case ID_VIEW_RIGHT:
			resetView(LDVAngleRight);
			return 0;
			break;
		case ID_VIEW_TOP:
			resetView(LDVAngleTop);
			return 0;
			break;
		case ID_VIEW_BOTTOM:
			resetView(LDVAngleBottom);
			return 0;
			break;
		case ID_VIEW_ISO:
			resetView(LDVAngleIso);
			return 0;
			break;
		case ID_VIEW_SAVE_DEFAULT:
			saveDefaultView();
			return 0;
			break;
		case ID_VIEW_ZOOMTOFIT:
			zoomToFit();
			return 0;
			break;
/*
		case ID_VIEW_RESET_DEFAULT:
			resetDefaultView();
			return 0;
			break;
*/
		case ID_VIEW_ERRORS:
			modelWindow->showErrors();
			return 0;
			break;
		case ID_VIEW_STATUSBAR:
			return switchStatusBar();
			break;
		case ID_VIEW_TOOLBAR:
			return switchToolbar();
			break;
		case ID_VIEW_ALWAYSONTOP:
			return switchTopmost();
			break;
		case ID_VIEW_VISUALSTYLE:
			return switchVisualStyle();
			break;
		case ID_VIEW_EXAMINE:
			return switchToExamineMode();
			break;
		case ID_VIEW_FLYTHROUGH:
			return switchToFlythroughMode();
			break;
		case ID_VIEW_INFO:
			showViewInfo();
			return 0;
			break;
/*
		case ID_VIEW_TRANS_MATRIX:
			showTransformationMatrix();
			return 0;
			break;
*/
/*
		case ID_VIEW_LDRAWCOMMANDLINE:
			showLDrawCommandLine();
			return 0;
			break;
*/
		case ID_FILE_RELOAD:
			modelWindow->reload();
			modelWindow->update();
			return 0;
			break;
		case ID_HELP_ABOUT:
			showAboutBox();
			return 0;
		case ID_HELP_CONTENTS:
			if (!fullScreenActive)
			{
				showHelp();
				return 0;
			}
			break;
		case ID_HELP_OPENGL_INFO:
			return showOpenGLDriverInfo();
			break;
/*
		case ID_HELP_OPENGLINFO_VENDOR:
			message = (char*)glGetString(GL_VENDOR);
			break;
		case ID_HELP_OPENGLINFO_RENDERER:
			message = (char*)glGetString(GL_RENDERER);
			break;
		case ID_HELP_OPENGLINFO_VERSION:
			message = (char*)glGetString(GL_VERSION);
			break;
		case ID_HELP_OPENGLINFO_EXTENSIONS:
			message = (char*)glGetString(GL_EXTENSIONS);
			break;
		case ID_HELP_OPENGLINFO_WGLEXTENSIONS:
			message = LDVExtensionsSetup::getWglExtensions();
			if (!message)
			{
				message = "None";
			}
			break;
*/
		case BN_CLICKED:
			switch (notifyCode)
			{
			case LIBRARY_UPDATE_FINISHED:
				doLibraryUpdateFinished(LIBRARY_UPDATE_FINISHED);
				return 0;
				break;
			case LIBRARY_UPDATE_CANCELED:
				doLibraryUpdateFinished(LIBRARY_UPDATE_CANCELED);
				return 0;
				break;
			case LIBRARY_UPDATE_NONE:
				doLibraryUpdateFinished(LIBRARY_UPDATE_NONE);
				return 0;
				break;
			}
		case IDC_WIREFRAME:
			doWireframe();
			break;
		case IDC_SEAMS:
			doSeams();
			break;
		case IDC_HIGHLIGHTS:
			doEdges();
			break;
		case IDC_PRIMITIVE_SUBSTITUTION:
			doPrimitiveSubstitution();
			break;
		case IDC_LIGHTING:
			doLighting();
			break;
		case IDC_BFC:
			doBfc();
			break;
		case ID_VIEWANGLE:
			doViewAngle();
			break;
		case ID_EDGES_SHOWEDGESONLY:
			doShowEdgesOnly();
			break;
		case ID_EDGES_CONDITIONALLINES:
			doConditionalLines();
			break;
		case ID_EDGES_HIGHQUALITY:
			doHighQualityEdges();
			break;
		case ID_EDGES_ALWAYSBLACK:
			doAlwaysBlack();
			break;
	}
	if (itemId >= ID_HOT_KEY_0 && itemId <= ID_HOT_KEY_9)
	{
		if (modelWindow && modelWindow->performHotKey(itemId - ID_HOT_KEY_0))
		{
			return 0;
		}
	}
	if (itemId >= 30000 && itemId < 30000 + numVideoModes)
	{
		selectFSVideoModeMenuItem(itemId - 30000);
	}
	else if (itemId >= 31000 && itemId < 31100)
	{
		openRecentFile(itemId - 31000);
		return 0;
	}
	if (itemId >= ID_FILE_POLLING_DISABLED &&
		itemId <= ID_FILE_POLLING_BACKGROUND)
	{
		selectPollingMenuItem(itemId);
	}
/*
	if (message)
	{
		float rotationSpeed = modelWindow->getRotationSpeed();
		float zoomSpeed = modelWindow->getZoomSpeed();

		modelWindow->setRotationSpeed(0.0f);
		modelWindow->setZoomSpeed(0.0f);
		MessageBox(NULL, message, "Open GL Info",
			MB_OK | MB_ICONINFORMATION | MB_TASKMODAL);
		modelWindow->setRotationSpeed(rotationSpeed);
		modelWindow->setZoomSpeed(zoomSpeed);
		return 0;
	}
*/
	return CUIWindow::doCommand(itemId, notifyCode, controlHWnd);
}

void LDViewWindow::doWireframe(void)
{
	if (doToolbarCheck(drawWireframe, IDC_WIREFRAME))
	{
		prefs->setDrawWireframe(drawWireframe);
		modelWindow->forceRedraw();
	}
}

void LDViewWindow::doSeams(void)
{
	if (doToolbarCheck(seams, IDC_SEAMS))
	{
		prefs->setUseSeams(seams);
		modelWindow->forceRedraw();
	}
}

void LDViewWindow::doEdges(void)
{
	if (doToolbarCheck(edges, IDC_HIGHLIGHTS))
	{
		prefs->setShowsHighlightLines(edges);
		modelWindow->forceRedraw();
	}
}

void LDViewWindow::doPrimitiveSubstitution(void)
{
	if (doToolbarCheck(primitiveSubstitution, IDC_PRIMITIVE_SUBSTITUTION))
	{
		prefs->setAllowPrimitiveSubstitution(primitiveSubstitution);
		modelWindow->forceRedraw();
	}
}

void LDViewWindow::doLighting(void)
{
	if (doToolbarCheck(lighting, IDC_LIGHTING))
	{
		prefs->setUseLighting(lighting);
		modelWindow->forceRedraw();
	}
}

void LDViewWindow::doShowEdgesOnly(void)
{
	prefs->setEdgesOnly(!prefs->getEdgesOnly());
	setMenuCheck(hEdgesMenu, ID_EDGES_SHOWEDGESONLY, prefs->getEdgesOnly());
}

void LDViewWindow::doConditionalLines(void)
{
	prefs->setDrawConditionalHighlights(!prefs->getDrawConditionalHighlights());
	setMenuCheck(hEdgesMenu, ID_EDGES_CONDITIONALLINES,
		prefs->getDrawConditionalHighlights());
}

void LDViewWindow::doHighQualityEdges(void)
{
	prefs->setUsePolygonOffset(!prefs->getUsePolygonOffset());
	setMenuCheck(hEdgesMenu, ID_EDGES_HIGHQUALITY,
		prefs->getUsePolygonOffset());
}

void LDViewWindow::doAlwaysBlack(void)
{
	prefs->setBlackHighlights(!prefs->getBlackHighlights());
	setMenuCheck(hEdgesMenu, ID_EDGES_ALWAYSBLACK, prefs->getBlackHighlights());
}

void LDViewWindow::doBfc(void)
{
	if (doToolbarCheck(bfc, IDC_BFC))
	{
		prefs->setBfc(bfc);
		modelWindow->forceRedraw();
	}
}

bool LDViewWindow::doToolbarCheck(bool &value, LPARAM commandId)
{
	BYTE state = (BYTE)SendMessage(hToolbar, TB_GETSTATE, commandId, 0);
	bool newValue = false;

	if (state & TBSTATE_CHECKED)
	{
		newValue = true;
	}
	if (newValue != value)
	{
		HWND hTooltip = (HWND)SendMessage(hToolbar, TB_GETTOOLTIPS, 0, 0);

		SendMessage(hTooltip, TTM_POP, 0, 0);
		value = newValue;
		return true;
	}
	else
	{
		return false;
	}
}

void LDViewWindow::doViewAngle(void)
{
/*
	int newViewAngle = lastViewAngle + 1;

	if (newViewAngle > LDVAngleIso)
	{
		newViewAngle = LDVAngleDefault;
	}
	lastViewAngle = (LDVAngle)newViewAngle;
*/
	if (modelWindow)
	{
		LDrawModelViewer *modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			modelViewer->resetView(lastViewAngle);
			modelWindow->forceRedraw();
		}
	}
}

WNDCLASSEX LDViewWindow::getWindowClass(void)
{
	WNDCLASSEX windowClass = CUIWindow::getWindowClass();

	windowClass.hIcon = LoadIcon(getLanguageModule(),
		MAKEINTRESOURCE(IDI_APP_ICON));
/*
	if (fullScreen || screenSaver)
	{
		windowClass.lpszMenuName = NULL;
	}
	else
	{
		windowClass.lpszMenuName = MAKEINTRESOURCE(IDR_MAIN_MENU);
	}
*/
	return windowClass;
}

LRESULT LDViewWindow::doSize(WPARAM sizeType, int newWidth, int newHeight)
{
	if (!fullScreen && !screenSaver)
	{
		if (sizeType == SIZE_MAXIMIZED)
		{
			TCUserDefaults::setLongForKey(1, WINDOW_MAXIMIZED_KEY, false);
		}
		else
		{
			TCUserDefaults::setLongForKey(0, WINDOW_MAXIMIZED_KEY, false);
			if (sizeType == SIZE_RESTORED)
			{
				TCUserDefaults::setLongForKey(newWidth, WINDOW_WIDTH_KEY,
					false);
				TCUserDefaults::setLongForKey(newHeight, WINDOW_HEIGHT_KEY,
					false);
			}
		}
		if ((showStatusBar || showStatusBarOverride) && hStatusBar)
		{
			int parts[] = {100, 150, -1};
			RECT rect;

			SendMessage(hStatusBar, WM_SIZE, SIZE_RESTORED,
				MAKELPARAM(newWidth, newHeight));
			SendMessage(hStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
			SendMessage(hStatusBar, SB_GETRECT, 2, (LPARAM)&rect);
			parts[1] += rect.right - rect.left - 32;
			SendMessage(hStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
		}
		if (showToolbar && hToolbar)
		{
			SendMessage(hToolbar, TB_AUTOSIZE, 0, 0);
		}
	}
	return CUIWindow::doSize(sizeType, newWidth, newHeight);
}

/*
int LDViewWindow::getDecorationHeight(void)
{
	int menuHeight = GetSystemMetrics(SM_CYMENU);

	return CUIWindow::getDecorationHeight() + menuHeight;
}
*/

LRESULT LDViewWindow::doClose(void)
{
#ifdef _DEBUG
//	_CrtDbgReport(_CRT_WARN, NULL, 0, NULL, "doClose\n");
#endif // _DEBUG
	skipMinimize = YES;
	if (modelWindow)
	{
		modelWindow->closeWindow();
//		modelWindowShown = false;
	}
	return CUIWindow::doClose();
}

LRESULT LDViewWindow::doDestroy(void)
{
	if (!switchingModes)
	{
		if (fullScreen)
		{
			restoreDisplayMode();
		}
		if (modelWindow)
		{
			modelWindow->release();
		}
		CUIWindow::doDestroy();
		TCAutoreleasePool::processReleases();
		debugOut("LDViewWindow::doDestroy\n");
		PostQuitMessage(0);
	}
	return 1;
}


HMENU LDViewWindow::menuForBitDepth(HWND hWnd, int bitDepth, int* index)
{
	HMENU viewMenu = GetSubMenu(GetMenu(hWnd), 2);
	int i;
	int count = GetMenuItemCount(viewMenu);
	HMENU bitDepthMenu = 0;
	MENUITEMINFO itemInfo;

	memset(&itemInfo, 0, sizeof(MENUITEMINFO));
	itemInfo.cbSize = sizeof(MENUITEMINFO);
	itemInfo.fMask = MIIM_DATA | MIIM_SUBMENU;
	for (i = 0; i < count && !bitDepthMenu; i++)
	{
		GetMenuItemInfo(viewMenu, i, TRUE, &itemInfo);
		if (itemInfo.hSubMenu && itemInfo.dwItemData == (unsigned)bitDepth)
		{
			bitDepthMenu = itemInfo.hSubMenu;
//			bitDepthMenu = GetSubMenu(viewMenu, i);
			if (index)
			{
				*index = i;
			}
		}
	}
	return bitDepthMenu;
}

VideoModeT* LDViewWindow::getCurrentVideoMode(void)
{
	if (currentVideoModeIndex >= 0 && currentVideoModeIndex < numVideoModes)
	{
		return videoModes + currentVideoModeIndex;
	}
	else
	{
		return NULL;
	}
}

int LDViewWindow::getMenuItemIndex(HMENU hMenu, UINT itemID)
{
	int count = GetMenuItemCount(hMenu);
	int i;

	for (i = 0; i < count; i++)
	{
		if (GetMenuItemID(hMenu, i) == itemID)
		{
			return i;
		}
	}
	return -1;
}

int LDViewWindow::clearRecentFileMenuItems(void)
{
	int firstIndex = getMenuItemIndex(hFileMenu, ID_FILE_PRINT);
	int lastIndex = getMenuItemIndex(hFileMenu, ID_FILE_EXIT);

	if (firstIndex >= 0 && lastIndex >= 0)
	{
		int i;

		for (i = firstIndex + 1; i < lastIndex - 1; i++)
		{
			RemoveMenu(hFileMenu, firstIndex + 2, MF_BYPOSITION);
		}
		return firstIndex + 1;
	}
	return -1;
}

void LDViewWindow::populateExtraSearchDirs(void)
{
	int i;

	extraSearchDirs->removeAll();
	for (i = 1; true; i++)
	{
		char key[128];
		char *extraSearchDir;

		sprintf(key, "%s/Dir%03d", EXTRA_SEARCH_DIRS_KEY, i);
		extraSearchDir = TCUserDefaults::stringForKey(key, NULL, false);
		if (extraSearchDir)
		{
			extraSearchDirs->addString(extraSearchDir);
			delete extraSearchDir;
		}
		else
		{
			break;
		}
	}
}

void LDViewWindow::recordExtraSearchDirs(void)
{
	int i;
	int count = extraSearchDirs->getCount();

	for (i = 0; i <= count; i++)
	{
		char key[128];
		char *extraDir;

		sprintf(key, "%s/Dir%03d", EXTRA_SEARCH_DIRS_KEY, i + 1);
		extraDir = extraSearchDirs->stringAtIndex(i);
		if (extraDir)
		{
			TCUserDefaults::setStringForKey(extraDir, key, false);
		}
		else
		{
			TCUserDefaults::removeValue(key, false);
		}
	}
	if (modelWindow)
	{
		LDrawModelViewer *modelViewer = modelWindow->getModelViewer();

		if (modelViewer)
		{
			modelViewer->setExtraSearchDirs(extraSearchDirs);
			modelWindow->forceRedraw();
		}
	}
}

void LDViewWindow::populateRecentFiles(void)
{
	int i;
	long maxRecentFiles = TCUserDefaults::longForKey(MAX_RECENT_FILES_KEY, 10,
		false);

	recentFiles->removeAll();
	for (i = 1; i <= maxRecentFiles; i++)
	{
		char key[128];
		char *filename;

		sprintf(key, "%s/File%02d", RECENT_FILES_KEY, i);
		filename = TCUserDefaults::stringForKey(key, NULL, false);
		if (filename)
		{
			recentFiles->addString(filename);
			delete filename;
		}
		else
		{
			recentFiles->addString(NULL);
		}
	}
}

void LDViewWindow::recordRecentFiles(void)
{
	int i;
	long maxRecentFiles = TCUserDefaults::longForKey(MAX_RECENT_FILES_KEY, 10,
		false);

	for (i = 1; i <= maxRecentFiles; i++)
	{
		char key[128];
		char *filename;

		sprintf(key, "%s/File%02d", RECENT_FILES_KEY, i);
		filename = recentFiles->stringAtIndex(i - 1);
		if (filename)
		{
			TCUserDefaults::setStringForKey(filename, key, false);
		}
		else
		{
			TCUserDefaults::removeValue(key, false);
		}
	}
}

void LDViewWindow::populateRecentFileMenuItems(void)
{
	int index = clearRecentFileMenuItems();

	if (index >= 0 && recentFiles->stringAtIndex(0))
	{
		int i;
		MENUITEMINFO itemInfo;
		long maxRecentFiles = TCUserDefaults::longForKey(MAX_RECENT_FILES_KEY,
			10, false);

		memset(&itemInfo, 0, sizeof(MENUITEMINFO));
		itemInfo.cbSize = sizeof(MENUITEMINFO);
		itemInfo.fMask = MIIM_TYPE;
		itemInfo.fType = MFT_SEPARATOR;
		InsertMenuItem(hFileMenu, index, TRUE, &itemInfo);
//		SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
		for (i = 0; i < maxRecentFiles; i++)
		{
			char *filename = recentFiles->stringAtIndex(i);

			if (filename)
			{
				char title[2048];
				char *partialFilename = copyString(filename);

				PathCompactPath(hdc, partialFilename, 250);
				if (i < 10)
				{
					sprintf(title, "%s&%d %s", i == 9 ? "1" : "", (i + 1) % 10,
						partialFilename);
				}
				else
				{
					strcpy(title, partialFilename);
				}
				itemInfo.fMask = MIIM_TYPE | MIIM_ID;
				itemInfo.fType = MFT_STRING;
				itemInfo.dwTypeData = title;
				itemInfo.wID = 31000 + i;
				InsertMenuItem(hFileMenu, index + i + 1, TRUE, &itemInfo);
				delete partialFilename;
			}
			else
			{
				break;
			}
		}
	}
}

void LDViewWindow::populateDisplayModeMenuItems(void)
{
	int i;
	HMENU viewMenu = GetSubMenu(GetMenu(hWindow), 2);
//	VideoModeT* currentVideoMode = getCurrentVideoMode();
	
	for (i = 0; i < numVideoModes; i++)
	{
		int count = GetMenuItemCount(viewMenu);
		VideoModeT videoMode = videoModes[i];
		HMENU bitDepthMenu = menuForBitDepth(hWindow, videoMode.depth);
		char title[128];
		MENUITEMINFO itemInfo;

		if (!bitDepthMenu)
		{
			memset(&itemInfo, 0, sizeof(MENUITEMINFO));
			sprintf(title, TCLocalStrings::get("NBitModes"), videoMode.depth);
			bitDepthMenu = CreatePopupMenu();
			itemInfo.cbSize = sizeof(MENUITEMINFO);
			itemInfo.fMask = MIIM_TYPE | MIIM_SUBMENU | MIIM_DATA;
			itemInfo.fType = MFT_STRING | MFT_RADIOCHECK;
			itemInfo.dwTypeData = title;
			itemInfo.dwItemData = videoMode.depth;
			itemInfo.hSubMenu = bitDepthMenu;
			InsertMenuItem(viewMenu, count, TRUE, &itemInfo);
		}
		sprintf(title, "%dx%d", videoMode.width, videoMode.height);
		memset(&itemInfo, 0, sizeof(MENUITEMINFO));
		itemInfo.cbSize = sizeof(MENUITEMINFO);
		itemInfo.fMask = MIIM_TYPE /*| MIIM_STATE*/ | MIIM_ID;
		itemInfo.fType = MFT_STRING | MFT_RADIOCHECK;
		itemInfo.dwTypeData = title;
		itemInfo.wID = 30000 + i;
/*
		if (currentVideoMode && videoMode.width == currentVideoMode->width &&
			videoMode.height == currentVideoMode->height &&
			videoMode.depth == currentVideoMode->depth)
		{
			itemInfo.fState = MFS_CHECKED;
		}
		else
		{
			itemInfo.fState = MFS_UNCHECKED;
		}
*/
		InsertMenuItem(bitDepthMenu, GetMenuItemCount(bitDepthMenu), TRUE,
			&itemInfo);
	}
}

void LDViewWindow::checkVideoMode(int width, int height, int depth)
{
	DEVMODE deviceMode;
	long result;

	memset(&deviceMode, 0, sizeof DEVMODE);
	deviceMode.dmSize = sizeof DEVMODE;
	deviceMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
	deviceMode.dmPelsWidth = width;
	deviceMode.dmPelsHeight = height;
	deviceMode.dmBitsPerPel = depth;
	result = ChangeDisplaySettings(&deviceMode, CDS_TEST | CDS_FULLSCREEN);
	if (result == DISP_CHANGE_SUCCESSFUL)
	{
		if (videoModes)
		{
			videoModes[numVideoModes].width = width;
			videoModes[numVideoModes].height = height;
			videoModes[numVideoModes].depth = depth;
			if (videoModes[numVideoModes].width == fsWidth &&
				videoModes[numVideoModes].height == fsHeight)
			{
				if (currentVideoModeIndex < 0 ||
					videoModes[numVideoModes].depth == fsDepth)
				{
					currentVideoModeIndex = numVideoModes;
				}
			}
		}
		numVideoModes++;
	}
}

void LDViewWindow::checkLowResModes(void)
{
	checkVideoMode(320, 240, 16);
	checkVideoMode(400, 300, 16);
	checkVideoMode(480, 360, 16);
	checkVideoMode(512, 384, 16);
	checkVideoMode(320, 240, 32);
	checkVideoMode(400, 300, 32);
	checkVideoMode(480, 360, 32);
	checkVideoMode(512, 384, 32);
}

void LDViewWindow::getAllDisplayModes(void)
{
	int i;
	int count;
	DEVMODE deviceMode;

	memset(&deviceMode, 0, sizeof DEVMODE);
	deviceMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
	numVideoModes = 0;
	videoModes = NULL;
	checkLowResModes();
	for (i = 0; EnumDisplaySettings(NULL, i, &deviceMode); i++)
	{
		if (deviceMode.dmBitsPerPel >= 15)
		{
			numVideoModes++;
		}
	}
	count = i;
	videoModes = new VideoModeT[numVideoModes];
	numVideoModes = 0;
	checkLowResModes();
	for (i = 0; i < count; i++)
	{
		EnumDisplaySettings(NULL, i, &deviceMode);
		if (deviceMode.dmBitsPerPel >= 15)
		{
			int j;
			BOOL found = NO;

			for (j = 0; j < numVideoModes && !found; j++)
			{
				VideoModeT videoMode = videoModes[j];

				if (videoMode.width == (int)deviceMode.dmPelsWidth &&
					videoMode.height == (int)deviceMode.dmPelsHeight &&
					videoMode.depth == (int)deviceMode.dmBitsPerPel)
				{
					found = YES;
				}
			}
			if (!found)
			{
				videoModes[numVideoModes].width = deviceMode.dmPelsWidth;
				videoModes[numVideoModes].height = deviceMode.dmPelsHeight;
				videoModes[numVideoModes].depth = deviceMode.dmBitsPerPel;
				if (videoModes[numVideoModes].width == fsWidth &&
					videoModes[numVideoModes].height == fsHeight)
				{
					if (currentVideoModeIndex < 0 ||
						videoModes[numVideoModes].depth == fsDepth)
					{
						currentVideoModeIndex = numVideoModes;
					}
				}
				numVideoModes++;
			}
		}
	}
}

LRESULT LDViewWindow::doCreate(HWND hWnd, LPCREATESTRUCT lpcs)
{
	LRESULT retVal;

	retVal = CUIWindow::doCreate(hWnd, lpcs);
	if (!videoModes)
	{
		getAllDisplayModes();
	}
	populateDisplayModeMenuItems();
	initPollingMenu();
	if (currentVideoModeIndex >= 0)
	{
		selectFSVideoModeMenuItem(currentVideoModeIndex);
	}
	return retVal;
}

void LDViewWindow::saveDefaultView(void)
{
	modelWindow->saveDefaultView();
}

void LDViewWindow::zoomToFit(void)
{
	LDrawModelViewer *modelViewer = modelWindow->getModelViewer();

	if (modelViewer)
	{
		setWaitCursor();
		modelViewer->zoomToFit();
		modelWindow->forceRedraw();
		setArrowCursor();
	}
}

void LDViewWindow::resetDefaultView(void)
{
	modelWindow->resetDefaultView();
}

void LDViewWindow::resetView(LDVAngle viewAngle)
{
//	lastViewAngle = viewAngle;
	modelWindow->resetView(viewAngle);
}

void LDViewWindow::printModel(void)
{
	if (modelIsLoaded())
	{
		modelWindow->print();
	}
}

void LDViewWindow::pageSetup(void)
{
	if (modelIsLoaded())
	{
		modelWindow->pageSetup();
	}
}

void LDViewWindow::saveSnapshot(void)
{
	if (modelIsLoaded())
	{
		modelWindow->getModelViewer()->pause();
		modelWindow->saveSnapshot();
		modelWindow->getModelViewer()->unpause();
	}
}

bool LDViewWindow::saveSnapshot(char *saveFilename)
{
	if (modelIsLoaded())
	{
		return modelWindow->saveSnapshot(saveFilename);
	}
	return false;
}

void LDViewWindow::openModel(const char* filename, bool skipLoad)
{
	if (filename && strlen(filename) > 0)
	{
		char* newFilename = NULL;
		char fullPathName[1024];

		if (!verifyLDrawDir())
		{
			return;
		}
		if (filename[0] == '"')
		{
			newFilename = copyString(filename + 1);
			int length = strlen(newFilename);

			if (newFilename[length - 1] == '"')
			{
				newFilename[length - 1] = 0;
			}
		}
		else
		{
			newFilename = copyString(filename);
		}
		if (ModelWindow::chDirFromFilename(newFilename, fullPathName))
		{
			modelWindow->setFilename(fullPathName);
			if (!skipLoad && modelWindow->loadModel())
			{
				updateModelMenuItems();
				setLastOpenFile(fullPathName);
			}
		}
		delete newFilename;
	}
	else
	{
		OPENFILENAME openStruct;
		char fileTypes[1024];
		char openFilename[1024] = "";
		char* initialDir = lastOpenPath();

		if (initialDir)
		{
			memset(fileTypes, 0, 2);
			addFileType(fileTypes, TCLocalStrings::get("LDrawFileTypes"),
				"*.ldr;*.dat;*.mpd");
			addFileType(fileTypes, TCLocalStrings::get("LDrawModelFileTypes"),
				"*.ldr;*.dat");
			addFileType(fileTypes, TCLocalStrings::get("LDrawMpdFileTypes"),
				"*.mpd");
			addFileType(fileTypes, TCLocalStrings::get("AllFilesTypes"), "*.*");
			memset(&openStruct, 0, sizeof(OPENFILENAME));
			openStruct.lStructSize = sizeof(OPENFILENAME);
			openStruct.hwndOwner = hWindow;
			openStruct.lpstrFilter = fileTypes;
			openStruct.nFilterIndex = 1;
			openStruct.lpstrFile = openFilename;
			openStruct.nMaxFile = 1024;
			openStruct.lpstrInitialDir = initialDir;
			openStruct.lpstrTitle = TCLocalStrings::get("SelectModelFile");
			openStruct.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST |
				OFN_HIDEREADONLY;
			openStruct.lpstrDefExt = "ldr";
			if (GetOpenFileName(&openStruct))
			{
				modelWindow->setFilename(openStruct.lpstrFile);
				setLastOpenFile(openStruct.lpstrFile);
				modelWindow->loadModel();
				updateModelMenuItems();
			}
			delete initialDir;
		}
	}
	populateRecentFileMenuItems();
	if (modelWindowIsShown())
	{
		// Don't activate if it hasn't been shown, because my activation handler
		// shows it, and this makes the window show up with command line image
		// generation.
		SetActiveWindow(hWindow);
	}
}

void LDViewWindow::addFileType(char* fileTypes, const char* description,
							   char* filter)
{
	char* spot = fileTypes;

	for (spot = fileTypes; spot[0] != 0 || spot[1] != 0; spot++)
		;
	if (spot != fileTypes)
	{
		spot++;
	}
	strcpy(spot, description);
	spot += strlen(description) + 1;
	strcpy(spot, filter);
	spot += strlen(filter) + 1;
	*spot = 0;
}

char* LDViewWindow::getLDrawDir(void)
{
	char* lDrawDir = TCUserDefaults::stringForKey(LDRAWDIR_KEY, NULL, false);

	if (!lDrawDir)
	{
		char buf[1024];

		if (GetPrivateProfileString("LDraw", "BaseDirectory",
			LDLModel::lDrawDir(), buf, 1024, "ldraw.ini"))
		{
			buf[1023] = 0;
			lDrawDir = copyString(buf);
		}
		else
		{
			// We shouldn't be able to get here, but just to be safe...
			lDrawDir = copyString(LDLModel::lDrawDir());
		}
	}
	stripTrailingPathSeparators(lDrawDir);
	LDLModel::setLDrawDir(lDrawDir);
	return lDrawDir;
}

void LDViewWindow::createLDrawDirWindow(void)
{
	hLDrawDirWindow = createDialog(IDD_LDRAWDIR);
}

int CALLBACK LDViewWindow::pathBrowserCallback(HWND hwnd, UINT uMsg,
											   LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_SELCHANGED)
	{
		char path[MAX_PATH+10];
		ITEMIDLIST* itemIdList = (ITEMIDLIST*)lParam;

		// For some reason, computers on the network are considered directories,
		// and the ok button is enabled for them.  We don't want to allow that,
		// and if one is selected, the following method will fail.
		if (!SHGetPathFromIDList(itemIdList, path))
		{
			SendMessage(hwnd, BFFM_ENABLEOK, 0, 0);
		}
	}
	else if (lpData && uMsg == BFFM_INITIALIZED)
	{
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}
	return 0;
}

BOOL LDViewWindow::promptForLDrawDir(void)
{
	BROWSEINFO browseInfo;
	char displayName[MAX_PATH];
	ITEMIDLIST* itemIdList;
	char *oldLDrawDir = getLDrawDir();

	browseInfo.hwndOwner = NULL; //hWindow;
	browseInfo.pidlRoot = NULL;
	browseInfo.pszDisplayName = displayName;
	browseInfo.lpszTitle = TCLocalStrings::get("LDrawDirPrompt");
	browseInfo.ulFlags = BIF_RETURNONLYFSDIRS;
	browseInfo.lpfn = pathBrowserCallback;
	browseInfo.lParam = (LPARAM)oldLDrawDir;
	browseInfo.iImage = 0;
	if ((itemIdList = SHBrowseForFolder(&browseInfo)) != NULL)
	{
		char path[MAX_PATH+10];

		delete oldLDrawDir;
		if (SHGetPathFromIDList(itemIdList, path))
		{
			stripTrailingPathSeparators(path);
			TCUserDefaults::setStringForKey(path, LDRAWDIR_KEY, false);
			LDLModel::setLDrawDir(path);
			return TRUE;
		}
		MessageBox(NULL/*hWindow*/, TCLocalStrings::get("InvalidDirSelected"),
			TCLocalStrings::get("Error"), MB_OK);
	}
	delete oldLDrawDir;
	return FALSE;
/*
	if (!hLDrawDirWindow)
	{
		createLDrawDirWindow();
	}
	if (hLDrawDirWindow)
	{
		runDialogModal(hLDrawDirWindow);
		if (userLDrawDir)
		{
			stripTrailingPathSeparators(userLDrawDir);
			TCUserDefaults::setStringForKey(userLDrawDir, LDRAWDIR_KEY, false);
			LDLModel::setLDrawDir(userLDrawDir);
			delete userLDrawDir;
			userLDrawDir = NULL;
			return TRUE;
		}
	}
	return FALSE;
*/
}

/*
void LDViewWindow::stripTrailingSlash(char* value)
{
	int end = strlen(value) - 1;

	if (end >= 0 && (value[end] == '/' || value[end] == '\\'))
	{
		value[end] = 0;
	}
}
*/

BOOL LDViewWindow::verifyLDrawDir(char* value)
{
//	int length = strlen(value);
	char currentDir[1024];
	char newDir[1024];
	BOOL found = FALSE;

//	stripTrailingPathSeparators(value);
	sprintf(newDir, "%s\\parts", value);
	GetCurrentDirectory(1024, currentDir);
	if (SetCurrentDirectory(newDir))
	{
		sprintf(newDir, "%s\\p", value);
		if (SetCurrentDirectory(newDir))
		{
			found = TRUE;
		}
		SetCurrentDirectory(currentDir);
	}
	return found;
}

BOOL LDViewWindow::verifyLDrawDir(bool forceChoose)
{
	char* lDrawDir = getLDrawDir();
	BOOL found = FALSE;

	if (!forceChoose && 
		(!TCUserDefaults::longForKey(VERIFY_LDRAW_DIR_KEY, 1, false) ||
		verifyLDrawDir(lDrawDir)))
	{
		delete lDrawDir;
		found = TRUE;
	}
	else
	{
		delete lDrawDir;
		while (!found)
		{
			if (promptForLDrawDir())
			{
				lDrawDir = getLDrawDir();
				if (verifyLDrawDir(lDrawDir))
				{
					found = TRUE;
				}
				else
				{
					MessageBox(NULL, TCLocalStrings::get("LDrawNotInDir"),
						TCLocalStrings::get("InvalidDir"),
						MB_OK | MB_ICONWARNING | MB_TASKMODAL);
				}
				delete lDrawDir;
			}
			else
			{
				break;
			}
		}
	}
	return found;
}

char* LDViewWindow::lastOpenPath(char* pathKey)
{
	if (!pathKey)
	{
		pathKey = LAST_OPEN_PATH_KEY;
	}
	if (verifyLDrawDir())
	{
		char* path = TCUserDefaults::stringForKey(pathKey, NULL, false);

		if (!path)
		{
			path = getLDrawDir();
		}
		return path;
	}
	else
	{
		return NULL;
	}
}

void LDViewWindow::setLastOpenFile(const char* filename, char* pathKey)
{
	if (filename)
	{
		char* spot = strrchr(filename, '\\');
		int index;

		if (!pathKey)
		{
			pathKey = LAST_OPEN_PATH_KEY;
		}
		if (spot)
		{
			int length = spot - filename;
			char* path = new char[length + 1];

			strncpy(path, filename, length);
			path[length] = 0;
			TCUserDefaults::setStringForKey(path, pathKey, false);
			delete path;
		}
		if (recentFiles && !stringHasCaseInsensitiveSuffix(filename, ".tmp"))
		{
			index = recentFiles->indexOfString(filename);
			recentFiles->insertString(filename);
			if (index >= 0)
			{
				// Insert before removal.  Since the one being removed could
				// have the same pointer value as the string in the array, we
				// could otherwise access a pointer after it had been deleted.
				recentFiles->removeString(index + 1);
			}
			recordRecentFiles();
		}
	}
}

LRESULT LDViewWindow::doShowWindow(BOOL showFlag, LPARAM status)
{
//	debugPrintf("LDViewWindow::doShowWindow\n");
	if (modelWindow && showFlag)
	{
		if (!modelWindowIsShown())
		{
			modelWindow->showWindow(SW_NORMAL);
		}
		// For some reason, creating the status bar prior to the window being
		// shown results in a status bar that doesn't update properly in XP, so
		// show it here instead of in initWindow.
		reflectStatusBar();
		reflectToolbar();
	}
	return CUIWindow::doShowWindow(showFlag, status);
}

bool LDViewWindow::modelWindowIsShown(void)
{
	if (modelWindow)
	{
		HWND hModelWindow = modelWindow->getHWindow();
		return IsWindowVisible(hModelWindow) != FALSE;
/*
		WINDOWPLACEMENT windowPlacement;

		windowPlacement.length = sizeof(WINDOWPLACEMENT);
		if (GetWindowPlacement(hModelWindow, &windowPlacement))
		{
			return windowPlacement.showCmd != SW_HIDE;
		}
*/
	}
	return false;
}

LRESULT LDViewWindow::doKeyDown(int keyCode, long keyData)
{
	if (modelWindow)
	{
		return modelWindow->processKeyDown(keyCode, keyData);
	}
	else
	{
		return 1;
	}
}

LRESULT LDViewWindow::doKeyUp(int keyCode, long keyData)
{
	if (modelWindow)
	{
		return modelWindow->processKeyUp(keyCode, keyData);
	}
	else
	{
		return 1;
	}
}

LRESULT LDViewWindow::doDrawItem(HWND /*hControlWnd*/,
								 LPDRAWITEMSTRUCT drawItemStruct)
{
	if (drawItemStruct->hwndItem == hStatusBar)
	{
		if (drawItemStruct->itemID == 0)
		{
			DWORD backgroundColor =
				LDViewPreferences::getColor(BACKGROUND_COLOR_KEY);
 			HBRUSH hBrush = CreateSolidBrush(RGB(backgroundColor & 0xFF,
				(backgroundColor >> 8) & 0xFF, (backgroundColor >> 16) & 0xFF));

			FillRect(drawItemStruct->hDC, &drawItemStruct->rcItem, hBrush);
			DeleteObject(hBrush);
			return TRUE;
		}
	}
	return FALSE;
}

void LDViewWindow::startLoading(void)
{
}

void LDViewWindow::stopLoading(void)
{
}

void LDViewWindow::setLoading(bool value)
{
	loading = value;
	if (loading)
	{
		startLoading();
	}
	else
	{
		stopLoading();
	}
}

void LDViewWindow::setMenuItemsEnabled(HMENU hMenu, bool enabled)
{
	int i;
	int count = GetMenuItemCount(hMenu);

	for (i = 0; i < count; i++)
	{
		setMenuEnabled(hMenu, i, enabled, TRUE);
	}
}

LRESULT LDViewWindow::doInitMenuPopup(HMENU hPopupMenu, UINT /*uPos*/,
									  BOOL /*fSystemMenu*/)
{
	if (hPopupMenu == hEdgesMenu)
	{
		updateEdgesMenu();
	}
	else
	{
		setMenuItemsEnabled(hPopupMenu, !loading);
		if (hPopupMenu == hFileMenu)
		{
			if (loading)
			{
				setMenuEnabled(hFileMenu, ID_FILE_CANCELLOAD, true);
			}
			else
			{
				setMenuEnabled(hFileMenu, ID_FILE_CANCELLOAD, false);
				updateModelMenuItems();
			}
		}
	}
	return 1;
}

BOOL LDViewWindow::doDialogGetMinMaxInfo(HWND hDlg, LPMINMAXINFO minMaxInfo)
{
	if (hDlg == hOpenGLInfoWindow)
	{
		calcSystemSizes();
		minMaxInfo->ptMaxSize.x = systemMaxWidth;
		minMaxInfo->ptMaxSize.y = systemMaxHeight;
		minMaxInfo->ptMinTrackSize.x = 250;
		minMaxInfo->ptMinTrackSize.y = 200;
		minMaxInfo->ptMaxTrackSize.x = systemMaxTrackWidth;
		minMaxInfo->ptMaxTrackSize.y = systemMaxTrackHeight;
		return TRUE;
	}
	return FALSE;
}

void LDViewWindow::reflectToolbar(void)
{
	if (fullScreen || screenSaver)
	{
		return;
	}
	if (showToolbar && !hToolbar)
	{
		addToolbar();
		setMenuCheck(hViewMenu, ID_VIEW_TOOLBAR, true);
	}
	else if (!showToolbar && hToolbar)
	{
		removeToolbar();
		setMenuCheck(hViewMenu, ID_VIEW_TOOLBAR, false);
	}
}

void LDViewWindow::reflectStatusBar(void)
{
	if (fullScreen || screenSaver)
	{
		return;
	}
	if ((showStatusBar || showStatusBarOverride) && !hStatusBar)
	{
		addStatusBar();
		setMenuCheck(hViewMenu, ID_VIEW_STATUSBAR, true);
	}
	else if (!showStatusBar && !showStatusBarOverride && hStatusBar)
	{
		removeStatusBar();
		setMenuCheck(hViewMenu, ID_VIEW_STATUSBAR, false);
	}
}

void LDViewWindow::reflectVideoMode(void)
{
	int i;

	for (i = 0; i < numVideoModes; i++)
	{
		if (videoModes[i].width == fsWidth &&
			videoModes[i].height == fsHeight &&
			videoModes[i].depth == fsDepth)
		{
			selectFSVideoModeMenuItem(i);
			break;
		}
	}
}

void LDViewWindow::toolbarCheckReflect(bool &value, bool prefsValue,
									   LPARAM commandID)
{
	if (value != prefsValue)
	{
		value = prefsValue;
		if (hToolbar)
		{
			BYTE state = (BYTE)SendMessage(hToolbar, TB_GETSTATE, commandID, 0);

			state &= ~TBSTATE_CHECKED;
			if (value)
			{
				state |= TBSTATE_CHECKED;
			}
			SendMessage(hToolbar, TB_SETSTATE, commandID, MAKELONG(state, 0));
		}
	}
}

void LDViewWindow::toolbarChecksReflect(void)
{
	toolbarCheckReflect(drawWireframe, prefs->getDrawWireframe(),
		IDC_WIREFRAME);
	toolbarCheckReflect(seams, prefs->getUseSeams() != 0, IDC_SEAMS);
	toolbarCheckReflect(edges, prefs->getShowsHighlightLines(), IDC_HIGHLIGHTS);
	toolbarCheckReflect(primitiveSubstitution,
		prefs->getAllowPrimitiveSubstitution(), IDC_PRIMITIVE_SUBSTITUTION);
	toolbarCheckReflect(lighting, prefs->getUseLighting(), IDC_LIGHTING);
	toolbarCheckReflect(bfc, prefs->getBfc(), IDC_BFC);
}

void LDViewWindow::applyPrefs(void)
{
	loadSettings();
	reflectViewMode();
	populateRecentFileMenuItems();
	reflectStatusBar();
	reflectToolbar();
	reflectTopmost();
	reflectPolling();
	reflectVideoMode();
	toolbarChecksReflect();
	if (TCUserDefaults::longForKey(WINDOW_MAXIMIZED_KEY, 0, false))
	{
		ShowWindow(hWindow, SW_MAXIMIZE);
	}
	else
	{
		ShowWindow(hWindow, SW_SHOWNORMAL);
	}
}

