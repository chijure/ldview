#ifndef __PREFERENCES_H__
#define __PREFERENCES_H__

#include <stdlib.h>
#include <LDLib/LDrawModelViewer.h>

class PreferencesPanel;
class ModelViewerWidget;
class QButton;
class QRangeControl;
class QString;
//class TCColorButton;
class QCheckBox;
class QRadioButton;
class QSlider;
class QSpinBox;
class LDPreferences;
class TCAlert;

typedef enum
{
	LDVViewExamine,
	LDVViewFlythrough
} LDVViewMode;

typedef enum
{
	LDVPollNone,
	LDVPollPrompt,
	LDVPollAuto,
	LDVPollBackground
} LDVPollMode;

class Preferences
{
public:
	Preferences(ModelViewerWidget *modelWidget = NULL);
	~Preferences(void);

	void doApply(void);
	void doCancel(void);
	void doResetGeneral(void);
	void doResetGeometry(void);
	void doResetEffects(void);
	void doResetPrimitives(void);
	void doResetUpdates(void);
	void doWireframeCutaway(bool value);
	void doLighting(bool value);
	void doProxyServer(bool value);
	void doUpdateMissingparts(bool value);
	void doStereo(bool value);
	void doWireframe(bool value);
	void doSortTransparency(bool value);
	void doStippleTransparency(bool value);
	void doBFC(bool value);
	void doEdgeLines(bool value);
	void doConditionalShow(bool value);
	void doPrimitiveSubstitution(bool value);
	void doTextureStuds(bool value);
    void doNewPreferenceSet(void);
    void doDelPreferenceSet(void);
    void doHotkeyPreferenceSet(void);
	bool doPrefSetSelected(bool);
	void doPrefSetsApply(void);
	void abandonChanges(void);
	void show(void);
	void doBackgroundColor();
	void doDefaultColor();
	bool getAllowPrimitiveSubstitution(void);
	void getRGB(int color, int &r, int &g, int &b);
	int getBackgroundColor(void);
	bool getShowErrors(void);

	void setShowError(int errorNumber, bool value);
	bool getShowError(int errorNumber);

	bool getStatusBar(void) { return statusBar; }
	void setStatusBar(bool value);
	bool getToolBar(void) { return toolBar; }
	void setToolBar(bool value);
	void setButtonState(QCheckBox *button, bool state);
	void setButtonState(QRadioButton *button, bool state);
	void setWindowSize(int width, int height);
	int getWindowWidth(void);
	int getWindowHeight(void);
	void setDrawWireframe(bool);
	bool getDrawWireframe(void);
	void setShowsHighlightLines(bool);
	bool getShowsHighlightLines(void);
	void setUseLighting(bool);
	bool getUseLighting(void);
	void setAllowPrimitiveSubstitution(bool);
	void setUseSeams(bool);
	bool getUseSeams(void);

	static char *getLastOpenPath(char *pathKey = NULL);
	static void setLastOpenPath(const char *path, char *pathKey = NULL);
	static char *getLDrawDir(void);
	static void setLDrawDir(const char *path);
	static long getMaxRecentFiles(void);
	static char *getRecentFile(int index);
	static void setRecentFile(int index, char *filename);
	static LDVPollMode getPollMode(void);
	static void setPollMode(LDVPollMode value);
	static LDVViewMode getViewMode(void);
	static void setViewMode(LDVViewMode value);
	void performHotKey(int);
    void setupPrefSetsList(void);
	void userDefaultChangedAlertCallback(TCAlert *alert);

protected:
	void doGeneralApply(void);
	void doGeometryApply(void);
	void doEffectsApply(void);
	void doPrimitivesApply(void);
	void doUpdatesApply(void);

	void loadSettings(void);
	void loadOtherSettings(void);

	void loadDefaultOtherSettings(void);

	void reflectSettings(void);
	void reflectGeneralSettings(void);
	void reflectGeometrySettings(void);
	void reflectWireframeSettings(void);
	void reflectBFCSettings(void);
	void reflectEffectsSettings(void);
	void reflectPrimitivesSettings(void);
	void reflectUpdatesSettings(void);

	void setRangeValue(QSpinBox *rangeConrol, int value);
	void setRangeValue(QSlider *rangeConrol, int value);

	void enableWireframeCutaway(void);
	void enableLighting(void);
	void enableStereo(void);
	void enableWireframe(void);
	void enableBFC(void);
	void enableEdgeLines(void);
	void enableConditionalShow(void);
	void enablePrimitiveSubstitution(void);
	void enableTextureStuds(void);
	void enableProxyServer(void);

	void disableWireframeCutaway(void);
	void disableLighting(void);
	void disableStereo(void);
	void disableWireframe(void);
	void disableBFC(void);
	void disableEdgeLines(void);
	void disableConditionalShow(void);
	void disablePrimitiveSubstitution(void);
	void disableTextureStuds(void);
	void disableProxyServer(void);
	void setupDefaultRotationMatrix(void);

	const char *getPrefSet(int);
	const char *getSelectedPrefSet(void);
	void selectPrefSet(const char *prefSet = NULL, bool force = false);	
	char *getHotKey(int);
	int getHotKey(const char*);
	int getCurrentHotKey(void);
	void saveCurrentHotKey(void);

	char *getErrorKey(int errorNumber);
	static const QString &getRecentFileKey(int index);

	ModelViewerWidget *modelWidget;
	LDrawModelViewer *modelViewer;
	LDPreferences *ldPrefs;
	PreferencesPanel *panel;

	bool checkAbandon;
	int hotKeyIndex;

	// Other Settings
	bool statusBar;
	bool toolBar;
	int windowWidth;
	int windowHeight;
};

#endif // __PREFERENCES_H__
