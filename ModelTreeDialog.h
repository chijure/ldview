#ifndef __MODELTREEDIALOG_H__
#define __MODELTREEDIALOG_H__

class LDLMainModel;
class LDModelTree;
class CUIWindowResizer;
class ModelWindow;
class TCAlert;

#include <CUI/CUIDialog.h>
#include <TCFoundation/TCStlIncludes.h>
#include <LDLoader/LDLFileLine.h>

typedef std::vector<HWND> HWndVector;
typedef std::map<UINT, LDLLineType> UIntLineTypeMap;
typedef std::map<LDLLineType, UINT> LineTypeUIntMap;

class ModelTreeDialog: public CUIDialog
{
public:
	ModelTreeDialog(HINSTANCE hInstance, HWND hParentWindow);
	void show(ModelWindow *modelWindow, HWND hParentWnd = NULL);
protected:
	virtual LRESULT doClose(void);

	virtual ~ModelTreeDialog(void);
	virtual void dealloc(void);
	HTREEITEM addLine(HTREEITEM parent, const LDModelTree *tree);
	void addChildren(HTREEITEM parent, const LDModelTree *tree);
	void fillTreeView(void);
	void refreshTreeView(void);
	void setModel(LDLMainModel *model);
	void setModelWindow(ModelWindow *modelWindow);
	void modelAlertCallback(TCAlert *alert);
	void setupLineCheck(UINT checkId, LDLLineType lineType);
	void updateLineChecks(void);
	void hideOptions(void);
	void showOptions(void);
	void adjustWindow(int widthDelta);
	void swapWindowText(char oldChar, char newChar);

	virtual BOOL doInitDialog(HWND /*hKbControl*/);
	virtual LRESULT doNotify(int controlId, LPNMHDR notification);
	virtual LRESULT doSize(WPARAM sizeType, int newWidth, int newHeight);
	virtual LRESULT doCommand(int notifyCode, int commandId,
		HWND control);

	LRESULT doToggleOptions(void);
	virtual LRESULT doItemExpanding(LPNMTREEVIEW notification);
	LRESULT doLineCheck(UINT checkId, LDLLineType lineType);

	ModelWindow *m_modelWindow;
	LDLMainModel *m_model;
	LDModelTree *m_modelTree;
	HWND m_hTreeView;
	HWND m_hStatus;
	CUIWindowResizer *m_resizer;
	HWndVector m_lineChecks;
	UIntLineTypeMap m_checkLineTypes;
	LineTypeUIntMap m_checkIds;
	bool m_optionsShown;
	int m_optionsDelta;
};

#endif // __MODELTREEDIALOG_H__
