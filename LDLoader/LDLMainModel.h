#ifndef __LDLMAINMODEL_H__
#define __LDLMAINMODEL_H__

#include <LDLoader/LDLModel.h>

class LDLMainModel : public LDLModel
{
public:
	LDLMainModel(void);
	LDLMainModel(const LDLMainModel &other);
	TCObject *copy(void);
	virtual bool load(const char *filename);
	virtual TCDictionary* getLoadedModels(void);
	virtual void print(void);
	virtual TCULong getHighlightColorNumber(TCULong colorNumber);

	// Flags
	void setLowResStuds(bool value) { m_mainFlags.lowResStuds = value; }
	bool getLowResStuds(void) const { return m_mainFlags.lowResStuds; }
	void setBlackEdgeLines(bool value) { m_mainFlags.blackEdgeLines = value; }
	bool getBlackEdgeLines(void) { return m_mainFlags.blackEdgeLines; }
protected:
	virtual void dealloc(void);

	TCDictionary *m_loadedModels;
	struct
	{
		// Public flags
		bool lowResStuds:1;
		bool blackEdgeLines:1;
	} m_mainFlags;
};

#endif // __LDLMAINMODEL_H__
