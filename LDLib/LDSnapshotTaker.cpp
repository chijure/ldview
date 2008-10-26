#include "LDSnapshotTaker.h"
#include "LDUserDefaultsKeys.h"
#include "LDrawModelViewer.h"
#include <TCFoundation/mystring.h>
#include <TCFoundation/TCImage.h>
#include <TCFoundation/TCAlertManager.h>
#include <TCFoundation/TCLocalStrings.h>
#include <TCFoundation/TCProgressAlert.h>
#include <TCFoundation/TCUserDefaults.h>
#include <TCFoundation/TCStringArray.h>
#include <LDLib/LDUserDefaultsKeys.h>
#include <LDLib/LDPreferences.h>
#include <LDLib/LDViewPoint.h>
#include <TRE/TREGLExtensions.h>
#include <gl2ps/gl2ps.h>

#ifdef WIN32
#if defined(_MSC_VER) && _MSC_VER >= 1400 && defined(_DEBUG)
#define new DEBUG_CLIENTBLOCK
#endif // _DEBUG
#endif // WIN32


class FBOHelper
{
public:
	FBOHelper(bool useFBO) : m_useFBO(useFBO)
	{
		if (m_useFBO)
		{
			glGenFramebuffersEXT(1, &m_fbo);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

			// Depth buffer
			glGenRenderbuffersEXT(1, &m_depthBuffer);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthBuffer);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT24,
				1024, 1024);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
				GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthBuffer);

			// Color buffer
			glGenRenderbuffersEXT(1, &m_colorBuffer);
			glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_colorBuffer);
			glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, 1024, 1024);
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
				GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_colorBuffer);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) !=
				GL_FRAMEBUFFER_COMPLETE_EXT)
			{
				debugPrintf("FBO Failed!\n");
			}
		}
	}
	~FBOHelper()
	{
		if (m_useFBO)
		{
			glDeleteFramebuffersEXT(1, &m_fbo);
			glDeleteRenderbuffersEXT(1, &m_depthBuffer);
			glDeleteRenderbuffersEXT(1, &m_colorBuffer);
			glReadBuffer(GL_BACK);
		}
	}
	bool m_useFBO;
	GLuint m_fbo;
	GLuint m_depthBuffer;
	GLuint m_colorBuffer;
};

LDSnapshotTaker::LDSnapshotTaker(void):
m_modelViewer(NULL),
m_imageType(ITPng),
m_trySaveAlpha(TCUserDefaults::boolForKey(SAVE_ALPHA_KEY, false, false)),
m_autoCrop(TCUserDefaults::boolForKey(AUTO_CROP_KEY, false, false)),
m_fromCommandLine(true),
m_commandLineSaveSteps(false),
m_commandLineStep(false),
m_step(-1),
m_grabSetupDone(false),
m_gl2psAllowed(TCUserDefaults::boolForKey(GL2PS_ALLOWED_KEY, false, false)),
m_useFBO(false)
{
}

LDSnapshotTaker::LDSnapshotTaker(LDrawModelViewer *m_modelViewer):
m_modelViewer(m_modelViewer),
m_imageType(ITPng),
m_trySaveAlpha(false),
m_autoCrop(false),
m_fromCommandLine(false),
m_commandLineSaveSteps(false),
m_commandLineStep(false),
m_step(-1),
m_grabSetupDone(false),
m_gl2psAllowed(TCUserDefaults::boolForKey(GL2PS_ALLOWED_KEY, false, false)),
m_useFBO(false)
{
}

LDSnapshotTaker::~LDSnapshotTaker(void)
{
}

void LDSnapshotTaker::dealloc(void)
{
	if (m_fromCommandLine)
	{
		TCObject::release(m_modelViewer);
	}
	TCObject::dealloc();
}

bool LDSnapshotTaker::saveImage(void)
{
	bool retValue = false;
	TCStringArray *unhandledArgs =
		TCUserDefaults::getUnhandledCommandLineArgs();

	if (unhandledArgs)
	{
		int i;
		int count = unhandledArgs->getCount();
		bool saveSnapshots = TCUserDefaults::boolForKey(SAVE_SNAPSHOTS_KEY,
			false, false);
		char *saveDir = NULL;
		const char *imageExt = NULL;
		int width = TCUserDefaults::longForKey(SAVE_WIDTH_KEY, 640, false);
		int height = TCUserDefaults::longForKey(SAVE_HEIGHT_KEY, 480, false);
		bool zoomToFit = TCUserDefaults::boolForKey(SAVE_ZOOM_TO_FIT_KEY, true,
			false);
		bool commandLineType = false;
		std::string snapshotSuffix =
			TCUserDefaults::commandLineStringForKey(SNAPSHOT_SUFFIX_KEY);

		if (TCUserDefaults::commandLineStringForKey(SAVE_IMAGE_TYPE_KEY).size()
			> 0)
		{
			m_imageType =
				(ImageType)TCUserDefaults::longForKey(SAVE_IMAGE_TYPE_KEY);
			commandLineType = true;
		}
		if (snapshotSuffix.size()
			> 0)
		{
			m_imageType = typeForFilename(snapshotSuffix.c_str(), m_gl2psAllowed);
			commandLineType = true;
		}
		if (TCUserDefaults::commandLineStringForKey(SAVE_STEPS_KEY).size() > 0)
		{
			m_commandLineSaveSteps = true;
		}
		if (TCUserDefaults::commandLineStringForKey(STEP_KEY).size() > 0)
		{
			m_commandLineStep = true;
		}
		if (saveSnapshots)
		{
			switch (TCUserDefaults::longForKey(SAVE_IMAGE_TYPE_KEY, 1, false))
			{
			case ITBmp:
				imageExt = ".bmp";
				break;
			case ITJpg:
				imageExt = ".jpg";
				break;
			case ITSvg:
				if (m_gl2psAllowed)
				{
					imageExt = ".svg";
					// NOTE: break is INTENTIONALLY inside the if statement.
					break;
				}
			case ITEps:
				if (m_gl2psAllowed)
				{
					imageExt = ".eps";
					// NOTE: break is INTENTIONALLY inside the if statement.
					break;
				}
			case ITPdf:
				if (m_gl2psAllowed)
				{
					imageExt = ".pdf";
					// NOTE: break is INTENTIONALLY inside the if statement.
					break;
				}
			default:
				imageExt = ".png";
				break;
			}
			saveDir = TCUserDefaults::stringForKey(SAVE_DIR_KEY, NULL, false);
			if (saveDir)
			{
				stripTrailingPathSeparators(saveDir);
			}
		}
		for (i = 0; i < count; i++)
		{
			char *arg = unhandledArgs->stringAtIndex(i);
			char newArg[1024];

			if (stringHasCaseInsensitivePrefix(arg, "-ca"))
			{
				float value;

				if (sscanf(arg + 3, "%f", &value) == 1)
				{
					sprintf(newArg, "-%s=%f", HFOV_KEY, value);
					TCUserDefaults::addCommandLineArg(newArg);
				}
			}
			else if (stringHasCaseInsensitivePrefix(arg, "-cg"))
			{
				sprintf(newArg, "-%s=%s", CAMERA_GLOBE_KEY, arg + 3);
				TCUserDefaults::addCommandLineArg(newArg);
				zoomToFit = true;
			}
		}
		for (i = 0; i < count && (saveSnapshots || !retValue); i++)
		{
			char *arg = unhandledArgs->stringAtIndex(i);
			
			if (arg[0] != '-' && arg[0] != 0)
			{
				char *imageFilename = NULL;

				if (saveSnapshots)
				{
					char *dotSpot = NULL;
					char *baseFilename = filenameFromPath(arg);

					if (saveDir)
					{
						imageFilename = new char[strlen(baseFilename) +
							strlen(imageExt) + strlen(saveDir) + 2];
						sprintf(imageFilename, "%s/%s", saveDir, baseFilename);
					}
					else
					{
						imageFilename = copyString(arg, strlen(imageExt));
					}
					// Note: we need there to be a dot in the base filename,
					// not the path before that.
					if (strchr(baseFilename, '.'))
					{
						dotSpot = strrchr(imageFilename, '.');
					}
					delete baseFilename;
					if (dotSpot)
					{
						*dotSpot = 0;
					}
					strcat(imageFilename, imageExt);
				}
				else
				{
					imageFilename = TCUserDefaults::stringForKey(
						SAVE_SNAPSHOT_KEY, NULL, false);
					if (imageFilename && !commandLineType)
					{
						m_imageType = typeForFilename(imageFilename,
							m_gl2psAllowed);
					}
				}
				if (imageFilename)
				{
					if (m_modelViewer && m_modelViewer->getFilename())
					{
						m_modelViewer->setFilename(arg);
						m_modelViewer->loadModel();
					}
					else if (m_modelViewer)
					{
						m_modelViewer->setFilename(arg);
					}
					else
					{
						m_modelFilename = arg;
					}
					retValue = saveImage(imageFilename, width, height,
						zoomToFit) || retValue;
					delete imageFilename;
				}
			}
		}
		delete saveDir;
	}
	return retValue;
}

bool LDSnapshotTaker::shouldZoomToFit(bool zoomToFit)
{
	char *cameraGlobe = TCUserDefaults::stringForKey(CAMERA_GLOBE_KEY, NULL,
		false);
	bool retValue = false;

	if (zoomToFit)
	{
		retValue = true;
	}
	else if (cameraGlobe)
	{
		float globeRadius;

		if (sscanf(cameraGlobe, "%*f,%*f,%f", &globeRadius) == 1)
		{
			retValue = true;
		}
	}
	delete cameraGlobe;
	return retValue;
}

bool LDSnapshotTaker::saveImage(
	const char *filename,
	int imageWidth,
	int imageHeight,
	bool zoomToFit)
{
	bool steps = false;
	FBOHelper fboHelper(m_useFBO);

	if (!m_fromCommandLine || m_commandLineSaveSteps)
	{
		steps = TCUserDefaults::boolForKey(SAVE_STEPS_KEY, false, false);
	}
	if (steps || m_commandLineStep)
	{
		char *stepSuffix = TCUserDefaults::stringForKey(SAVE_STEPS_SUFFIX_KEY,
			"-Step", false);
		bool retValue = true;
		int numSteps;
		int origStep;
		LDViewPoint *viewPoint = NULL;

		if (!m_modelViewer)
		{
			grabSetup();
		}
		origStep = m_modelViewer->getStep();
		if (m_modelViewer->getMainModel() == NULL)
		{
			// This isn't very efficient, but it gets the job done.  A
			// number of things need to happen before we can do the initial
			// zoomToFit.  We need to load the model, create the rotation
			// matrix, and set up the camera.  Maybe other things need to be
			// done too.  This update makes sure that things are OK for the
			// zoomToFit to execute properly.
			renderOffscreenImage();
		}
		if (TCUserDefaults::boolForKey(SAVE_STEPS_SAME_SCALE_KEY, true, false)
			&& zoomToFit)
		{
			if (!m_fromCommandLine)
			{
				viewPoint = m_modelViewer->saveViewPoint();
			}
			numSteps = m_modelViewer->getNumSteps();
			m_modelViewer->setStep(numSteps);
			m_modelViewer->zoomToFit();
			zoomToFit = false;
		}
		if (steps)
		{
			numSteps = m_modelViewer->getNumSteps();
		}
		else
		{
			numSteps = 1;
		}
		for (int step = 1; step <= numSteps && retValue; step++)
		{
			std::string stepFilename;

			if (steps)
			{
				stepFilename = removeStepSuffix(filename, stepSuffix);
				stepFilename = addStepSuffix(stepFilename, stepSuffix, step,
					numSteps);
				m_step = step;
			}
			else
			{
				stepFilename = filename;
				m_step = TCUserDefaults::longForKey(STEP_KEY);
			}
			retValue = saveStepImage(stepFilename.c_str(), imageWidth,
				imageHeight, zoomToFit);
		}
		delete stepSuffix;
		m_modelViewer->setStep(origStep);
		if (viewPoint)
		{
			m_modelViewer->restoreViewPoint(viewPoint);
			viewPoint->release();
		}
		return retValue;
	}
	else
	{
		m_step = -1;
		return saveStepImage(filename, imageWidth, imageHeight, zoomToFit);
	}
}

bool LDSnapshotTaker::saveGl2psStepImage(
	const char *filename,
	int /*imageWidth*/,
	int /*imageHeight*/,
	bool zoomToFit)
{
	int bufSize;
	int state = GL2PS_OVERFLOW;
	FILE *file = fopen(filename, "wb");
	bool retValue = false;

	if (file != NULL)
	{
		bool origForceZoomToFit;
		LDViewPoint *viewPoint = NULL;

		grabSetup();
		origForceZoomToFit = m_modelViewer->getForceZoomToFit();
		if (zoomToFit)
		{
			viewPoint = m_modelViewer->saveViewPoint();
			m_modelViewer->setForceZoomToFit(true);
		}
#ifdef COCOA
		// For some reason, we get nothing in the feedback buffer on the Mac if
		// we don't reparse the model.  No idea why that is.
		m_modelViewer->reparse();
#endif // COCOA
		m_modelViewer->setGl2ps(true);
		m_modelViewer->setup();
		for (bufSize = 1024 * 1024; state == GL2PS_OVERFLOW; bufSize *= 2)
		{
			GLint format;
			GLint options = GL2PS_USE_CURRENT_VIEWPORT
				| GL2PS_OCCLUSION_CULL
				| GL2PS_BEST_ROOT
				| GL2PS_NO_PS3_SHADING;

			if (m_autoCrop)
			{
				options |= GL2PS_TIGHT_BOUNDING_BOX;
			}
			switch (m_imageType)
			{
			case ITEps:
				format = GL2PS_EPS;
				break;
			case ITPdf:
				format = GL2PS_PDF;
				options |= GL2PS_COMPRESS;
				break;
			default:
				format = GL2PS_SVG;
				break;
			}
			state = gl2psBeginPage(filename, "LDView", NULL, format,
				GL2PS_BSP_SORT,	options, GL_RGBA, 0, NULL, 0, 0, 0, bufSize,
				file, filename);
			if (state == GL2PS_ERROR)
			{
				debugPrintf("ERROR in gl2ps routine!");
			}
			else
			{
				m_modelViewer->update();
				glFinish();
				state = gl2psEndPage();
				if (state == GL2PS_ERROR)
				{
					debugPrintf("ERROR in gl2ps routine!");
				}
				else
				{
					retValue = true;
				}
			}
		}
		m_modelViewer->setGl2ps(false);
		if (zoomToFit)
		{
			m_modelViewer->setForceZoomToFit(origForceZoomToFit);
			m_modelViewer->restoreViewPoint(viewPoint);
		}
		fclose(file);
	}
	return retValue;
}

bool LDSnapshotTaker::saveStepImage(
	const char *filename,
	int imageWidth,
	int imageHeight,
	bool zoomToFit)
{
	bool retValue = false;

	if (m_imageType >= ITSvg && m_imageType <= ITPdf)
	{
		retValue = saveGl2psStepImage(filename, imageWidth, imageHeight,
			zoomToFit);
	}
	else
	{
		bool saveAlpha = false;
		TCByte *buffer = grabImage(imageWidth, imageHeight,
			shouldZoomToFit(zoomToFit), NULL, &saveAlpha);

		if (buffer)
		{
			switch (m_imageType)
			{
			case ITPng:
				retValue = writePng(filename, imageWidth, imageHeight, buffer,
					saveAlpha);
				break;
			case ITBmp:
				retValue = writeBmp(filename, imageWidth, imageHeight, buffer);
				break;
			case ITJpg:
				retValue = writeJpg(filename, imageWidth, imageHeight, buffer);
				break;
			default:
				// Get rid of warning
				break;
			}
			delete buffer;
		}
	}
	return retValue;
}

bool LDSnapshotTaker::staticImageProgressCallback(
	CUCSTR message,
	float progress,
	void* userData)
{
	return ((LDSnapshotTaker*)userData)->imageProgressCallback(message,
		progress);
}

bool LDSnapshotTaker::imageProgressCallback(CUCSTR message, float progress)
{
	bool aborted;
	ucstring newMessage;

	if (message != NULL)
	{
		char *filename = filenameFromPath(m_currentImageFilename.c_str());
		UCSTR ucFilename = mbstoucstring(filename);

		delete filename;
		if (stringHasCaseInsensitivePrefix(message, _UC("Saving")))
		{
			newMessage = TCLocalStrings::get(_UC("SavingPrefix"));
		}
		else
		{
			newMessage = TCLocalStrings::get(_UC("LoadingPrefix"));
		}
		newMessage += ucFilename;
		delete ucFilename;
	}

	TCProgressAlert::send("LDSnapshotTaker", newMessage.c_str(), progress,
		&aborted, this);
	return !aborted;
}

bool LDSnapshotTaker::writeImage(
	const char *filename,
	int width,
	int height,
	TCByte *buffer,
	const char *formatName,
	bool saveAlpha)
{
	TCImage *image = new TCImage;
	bool retValue;
	char comment[1024];

	m_currentImageFilename = filename;
	if (saveAlpha)
	{
		image->setDataFormat(TCRgba8);
	}
	image->setSize(width, height);
	image->setLineAlignment(4);
	image->setImageData(buffer);
	image->setFormatName(formatName);
	image->setFlipped(true);
	if (strcasecmp(formatName, "PNG") == 0)
	{
		strcpy(comment, "Software:!:!:LDView");
	}
	else
	{
		strcpy(comment, "Created by LDView");
	}
	if (m_productVersion.size() > 0)
	{
		strcat(comment, " ");
		strcat(comment, m_productVersion.c_str());
	}
	image->setComment(comment);
	if (m_autoCrop)
	{
		image->autoCrop((TCByte)m_modelViewer->getBackgroundR(),
			(TCByte)m_modelViewer->getBackgroundG(),
			(TCByte)m_modelViewer->getBackgroundB());
	}
	retValue = image->saveFile(filename, staticImageProgressCallback, this);
	image->release();
	return retValue;
}

bool LDSnapshotTaker::writeJpg(
	const char *filename,
	int width,
	int height,
	TCByte *buffer)
{
	return writeImage(filename, width, height, buffer, "JPG", false);
}

bool LDSnapshotTaker::writeBmp(
	const char *filename,
	int width,
	int height,
	TCByte *buffer)
{
	return writeImage(filename, width, height, buffer, "BMP", false);
}

bool LDSnapshotTaker::writePng(
	const char *filename,
	int width,
	int height,
	TCByte *buffer,
	bool saveAlpha)
{
	return writeImage(filename, width, height, buffer, "PNG", saveAlpha);
}

void LDSnapshotTaker::calcTiling(
	int desiredWidth,
	int desiredHeight,
	int &bitmapWidth,
	int &bitmapHeight,
	int &numXTiles,
	int &numYTiles)
{
	if (desiredWidth > bitmapWidth)
	{
		numXTiles = (desiredWidth + bitmapWidth - 1) / bitmapWidth;
	}
	else
	{
		numXTiles = 1;
	}
	bitmapWidth = desiredWidth / numXTiles;
	if (desiredHeight > bitmapHeight)
	{
		numYTiles = (desiredHeight + bitmapHeight - 1) / bitmapHeight;
	}
	else
	{
		numYTiles = 1;
	}
	bitmapHeight = desiredHeight / numYTiles;
}

bool LDSnapshotTaker::canSaveAlpha(void)
{
	if (m_trySaveAlpha && m_imageType == ITPng)
	{
		GLint alphaBits;

		glGetIntegerv(GL_ALPHA_BITS, &alphaBits);
		return alphaBits > 0;
	}
	return false;
}

void LDSnapshotTaker::renderOffscreenImage(void)
{
	TCAlertManager::sendAlert(alertClass(), this, _UC("MakeCurrent"));
	if (m_modelViewer->getMainModel() == NULL)
	{
		m_modelViewer->loadModel();
	}
	m_modelViewer->update();
}

void LDSnapshotTaker::grabSetup(void)
{
	if (m_grabSetupDone)
	{
		return;
	}
	TCAlertManager::sendAlert(alertClass(), this, _UC("PreSave"));
	m_grabSetupDone = true;
	if (!m_modelViewer)
	{
		LDPreferences *prefs;
		GLint viewport[4];
		
		glGetIntegerv(GL_VIEWPORT, viewport);
		m_modelViewer = new LDrawModelViewer(viewport[2], viewport[3]);
		m_modelViewer->setFilename(m_modelFilename.c_str());
		m_modelFilename = "";
		prefs = new LDPreferences(m_modelViewer);
		prefs->loadSettings();
		prefs->applySettings();
		prefs->release();
	}
}

TCByte *LDSnapshotTaker::grabImage(
	int &imageWidth,
	int &imageHeight,
	bool zoomToFit,
	TCByte *buffer,
	bool *saveAlpha)
{
	grabSetup();

	GLenum bufferFormat = GL_RGB;
	bool origForceZoomToFit = m_modelViewer->getForceZoomToFit();
	TCVector origCameraPosition = m_modelViewer->getCamera().getPosition();
	TCFloat origXPan = m_modelViewer->getXPan();
	TCFloat origYPan = m_modelViewer->getYPan();
	int origWidth = m_modelViewer->getWidth();
	int origHeight = m_modelViewer->getHeight();
	bool origAutoCenter = m_modelViewer->getAutoCenter();
	GLint viewport[4];
	int newWidth, newHeight;
	int numXTiles, numYTiles;
	int xTile;
	int yTile;
	TCByte *smallBuffer;
	int bytesPerPixel = 3;
	int bytesPerLine;
	int smallBytesPerLine;
	int reallySmallBytesPerLine;
	bool canceled = false;
	bool bufferAllocated = false;

	if (m_step > 0)
	{
		m_modelViewer->setStep(m_step);
	}
	glGetIntegerv(GL_VIEWPORT, viewport);
	newWidth = viewport[2];
	newHeight = viewport[3];
	m_modelViewer->setWidth(newWidth);
	m_modelViewer->setHeight(newHeight);
	m_modelViewer->perspectiveView();
	calcTiling(imageWidth, imageHeight, newWidth, newHeight, numXTiles,
		numYTiles);
	m_modelViewer->setWidth(newWidth);
	m_modelViewer->setHeight(newHeight);
	if (zoomToFit)
	{
		m_modelViewer->setForceZoomToFit(true);
		m_modelViewer->perspectiveView();
	}
	m_modelViewer->setup();
	if (canSaveAlpha())
	{
		bytesPerPixel = 4;
		bufferFormat = GL_RGBA;
		m_modelViewer->setSaveAlpha(true);
		if (saveAlpha)
		{
			*saveAlpha = true;
		}
	}
	else
	{
		if (saveAlpha)
		{
			*saveAlpha = false;
		}
	}
	imageWidth = newWidth * numXTiles;
	imageHeight = newHeight * numYTiles;
	smallBytesPerLine = TCImage::roundUp(newWidth * bytesPerPixel, 4);
	reallySmallBytesPerLine = newWidth * bytesPerPixel;
	bytesPerLine = TCImage::roundUp(imageWidth * bytesPerPixel, 4);
	if (!buffer)
	{
		buffer = new TCByte[bytesPerLine * imageHeight];
		bufferAllocated = true;
	}
	if (numXTiles == 1 && numYTiles == 1)
	{
		smallBuffer = buffer;
	}
	else
	{
		smallBuffer = new TCByte[smallBytesPerLine * newHeight];
	}
	m_modelViewer->setNumXTiles(numXTiles);
	m_modelViewer->setNumYTiles(numYTiles);
	TCAlertManager::sendAlert(alertClass(), this, _UC("PreRender"));
	for (yTile = 0; yTile < numYTiles; yTile++)
	{
		m_modelViewer->setYTile(yTile);
		for (xTile = 0; xTile < numXTiles && !canceled; xTile++)
		{
			m_modelViewer->setXTile(xTile);
			renderOffscreenImage();
			TCProgressAlert::send("LDSnapshotTaker",
				TCLocalStrings::get(_UC("RenderingSnapshot")),
				(float)(yTile * numXTiles + xTile) / (numYTiles * numXTiles),
				&canceled);
			if (!canceled)
			{
				glFinish();
				TCAlertManager::sendAlert(alertClass(), this,
					_UC("RenderDone"));
				glReadPixels(0, 0, newWidth, newHeight, bufferFormat,
					GL_UNSIGNED_BYTE, smallBuffer);
				if (smallBuffer != buffer)
				{
					int y;

					for (y = 0; y < newHeight; y++)
					{
						int smallOffset = y * smallBytesPerLine;
						int offset = (y + (numYTiles - yTile - 1) * newHeight) *
							bytesPerLine + xTile * newWidth * bytesPerPixel;

						memcpy(&buffer[offset], &smallBuffer[smallOffset],
							reallySmallBytesPerLine);
					}
					// We only need to zoom to fit on the first tile; the
					// rest will already be correct.
					m_modelViewer->setForceZoomToFit(false);
				}
			}
			else
			{
				canceled = true;
			}
		}
	}
	m_modelViewer->setXTile(0);
	m_modelViewer->setYTile(0);
	m_modelViewer->setNumXTiles(1);
	m_modelViewer->setNumYTiles(1);
	m_modelViewer->setWidth(origWidth);
	m_modelViewer->setHeight(origHeight);
	m_modelViewer->setSaveAlpha(false);
	if (canceled && bufferAllocated)
	{
		delete buffer;
		buffer = NULL;
	}
	if (smallBuffer != buffer)
	{
		delete smallBuffer;
	}
	if (zoomToFit)
	{
		m_modelViewer->setForceZoomToFit(origForceZoomToFit);
		m_modelViewer->getCamera().setPosition(origCameraPosition);
		m_modelViewer->setXYPan(origXPan, origYPan);
		m_modelViewer->setAutoCenter(origAutoCenter);
	}
	return buffer;
}

bool LDSnapshotTaker::doCommandLine(void)
{
	LDSnapshotTaker *snapshotTaker = new LDSnapshotTaker;
	bool retValue = snapshotTaker->saveImage();

	snapshotTaker->release();
	return retValue;
}

// Note: static method
std::string LDSnapshotTaker::removeStepSuffix(
	const std::string &filename,
	const std::string &stepSuffix)
{
	if (stepSuffix.size() == 0)
	{
		return filename;
	}
	char *dirPart = directoryFromPath(filename.c_str());
	char *filePart = filenameFromPath(filename.c_str());
	std::string fileString = filePart;
	size_t suffixLoc;
	std::string tempSuffix = stepSuffix;
	std::string newString;

	newString = dirPart;
	delete dirPart;
#if defined(WIN32) || defined(__APPLE__)
	// case-insensitive file systems
	convertStringToLower(&fileString[0]);
	convertStringToLower(&tempSuffix[0]);
#endif // WIN32 || __APPLE__
	suffixLoc = fileString.rfind(tempSuffix);
	if (suffixLoc < fileString.size())
	{
		size_t i;

		for (i = suffixLoc + tempSuffix.size(); isdigit(fileString[i]); i++)
		{
			// Don't do anything
		}
#if defined(WIN32) || defined(__APPLE__)
		// case-insensitive file systems
		// Restore filename to original case
		fileString = filePart;
		delete filePart;
#endif // WIN32 || __APPLE__
		fileString.erase(suffixLoc, i - suffixLoc);
	}
	else
	{
		delete filePart;
		return filename;
	}
	if (newString.size() > 0)
	{
		newString += "/";
	}
	newString += fileString;
	filePart = cleanedUpPath(newString.c_str());
	newString = filePart;
	delete filePart;
	return newString;
}

// Note: static method
std::string LDSnapshotTaker::addStepSuffix(
	const std::string &filename,
	const std::string &stepSuffix,
	int step,
	int numSteps)
{
	size_t dotSpot = filename.rfind('.');
	std::string newString;
	char format[32];
	char buf[32];
	int digits = 1;

	while ((numSteps = numSteps / 10) != 0)
	{
		digits++;
	}
	sprintf(format, "%%0%dd", digits);
	sprintf(buf, format, step);
	newString = filename.substr(0, dotSpot);
	newString += stepSuffix;
	newString += buf;
	if (dotSpot < filename.size())
	{
		newString += filename.substr(dotSpot);
	}
	return newString;
}

// Note: static method
LDSnapshotTaker::ImageType LDSnapshotTaker::typeForFilename(
	const char *filename,
	bool gl2psAllowed)
{
	if (stringHasCaseInsensitiveSuffix(filename, ".png"))
	{
		return ITPng;
	}
	else if (stringHasCaseInsensitiveSuffix(filename, ".bmp"))
	{
		return ITBmp;
	}
	else if (stringHasCaseInsensitiveSuffix(filename, ".jpg"))
	{
		return ITJpg;
	}
	else if (stringHasCaseInsensitivePrefix(filename, ".svg") && gl2psAllowed)
	{
		return ITSvg;
	}
	else if (stringHasCaseInsensitivePrefix(filename, ".eps") && gl2psAllowed)
	{
		return ITEps;
	}
	else if (stringHasCaseInsensitivePrefix(filename, ".pdf") && gl2psAllowed)
	{
		return ITPdf;
	}
	else
	{
		// PNG is the default;
		return ITPng;
	}
}

// Note: static method
std::string LDSnapshotTaker::extensionForType(
	ImageType type,
	bool includeDot /*= false*/)
{
	if (includeDot)
	{
		std::string retValue(".");
		
		retValue += extensionForType(type, false);
		return retValue;
	}
	switch (type)
	{
	case ITPng:
		return "png";
	case ITBmp:
		return "bmp";
	case ITJpg:
		return "jpg";
	case ITSvg:
		return "svg";
	case ITEps:
		return "eps";
	case ITPdf:
		return "pdf";
	default:
		return "png";
	}
}

// Note: static method
LDSnapshotTaker::ImageType LDSnapshotTaker::lastImageType(void)
{
	if (TCUserDefaults::boolForKey(GL2PS_ALLOWED_KEY, false, false))
	{
		return ITLast;
	}
	else
	{
		return ITJpg;
	}
}

