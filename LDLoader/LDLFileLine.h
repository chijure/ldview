#ifndef __LDLFILELINE_H__
#define __LDLFILELINE_H__

#include <TCFoundation/TCObject.h>
#include <LDLoader/LDLError.h>
#include <TCFoundation/TCTypedObjectArray.h>

// The following is needed in order to declare the array below (which is used
// in the definition of LDLFileLine itself).
class LDLFileLine;

typedef TCTypedObjectArray<LDLFileLine> LDLFileLineArray;

typedef enum
{
	LDLLineTypeComment,
	LDLLineTypeModel,
	LDLLineTypeLine,
	LDLLineTypeTriangle,
	LDLLineTypeQuad,
	LDLLineTypeConditionalLine,
	LDLLineTypeEmpty,
	LDLLineTypeUnknown
} LDLLineType;

class LDLModel;

class LDLFileLine : public TCObject
{
public:
	operator const char *(void) const;
	const char *getLine(void) const { return m_line; }
	const char *getOriginalLine(void) const { return m_originalLine; }
	int getLineNumber(void) const { return m_lineNumber; }
	virtual bool parse(void) = 0;
	virtual LDLError *getError(void) { return m_error; }
	virtual void print(int indent) const;
	virtual LDLLineType getLineType(void) const = 0;
	virtual bool isActionLine(void) { return false; }
	virtual LDLModel *getParentModel(void) { return m_parentModel; }
	virtual bool isValid(void) { return m_valid; }
	virtual void forgetLine(void) { delete m_line; m_line = NULL; }
	virtual LDLFileLineArray *getReplacementLines(void);

	static LDLFileLine *initFileLine(LDLModel *parentModel, const char *line,
		int lineNumber);
protected:
	LDLFileLine(LDLModel *parentModel, const char *line, int lineNumber,
		const char *originalLine = NULL);
	LDLFileLine(const LDLFileLine &other);
	virtual ~LDLFileLine(void);
	virtual void dealloc(void);
	virtual void setError(LDLErrorType type, const char* format, ...);

	static bool lineIsEmpty(const char *line);
	static int scanLineType(const char *line);

	LDLModel *m_parentModel;
	char *m_line;
	char *m_originalLine;
	int m_lineNumber;
	LDLError *m_error;
	bool m_valid;
};

#endif // __LDLFILELINE_H__
