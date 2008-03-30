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

LDSnapshotTaker::LDSnapshotTaker(void):
m_modelViewer(NULL),
m_imageType(ITPng),
m_trySaveAlpha(TCUserDefaults::boolForKey(SAVE_ALPHA_KEY, false, false)),
m_autoCrop(TCUserDefaults::boolForKey(AUTO_CROP_KEY, false, false)),
m_fromCommandLine(true)
{
}

LDSnapshotTaker::LDSnapshotTaker(LDrawModelViewer *m_modelViewer):
m_modelViewer(m_modelViewer),
m_imageType(ITPng),
m_trySaveAlpha(false),
m_autoCrop(false),
m_fromCommandLine(false)
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
		char *imageExt = NULL;
		int width = TCUserDefaults::longForKey(SAVE_WIDTH_KEY, 640, false);
		int height = TCUserDefaults::longForKey(SAVE_HEIGHT_KEY, 480, false);
		bool zoomToFit = TCUserDefaults::boolForKey(SAVE_ZOOM_TO_FIT_KEY, true,
			false);
		bool commandLineType = false;
		TCStringArray *processed = TCUserDefaults::getProcessedCommandLine();

		if (processed && processed->getCount() > 0)
		{
			char prefix1[128];
			char prefix2[128];

			sprintf(prefix1, "%s=", SAVE_IMAGE_TYPE_KEY);
			sprintf(prefix2, "%s=", SNAPSHOT_SUFFIX_KEY);
			for (i = 0; i < processed->getCount() && !commandLineType; i++)
			{
				if (stringHasCaseInsensitivePrefix((*processed)[i], prefix1) ||
					stringHasCaseInsensitivePrefix((*processed)[i], prefix2))
				{
					commandLineType = true;
				}
			}
		}
		if (saveSnapshots)
		{
			switch (TCUserDefaults::longForKey(SAVE_IMAGE_TYPE_KEY, 1, false))
			{
			case 2:
				imageExt = ".bmp";
				break;
			case 3:
				imageExt = ".jpg";
				break;
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
			
			if (arg[0] != '-')
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
						if (stringHasCaseInsensitiveSuffix(imageFilename,
							".png"))
						{
							m_imageType = ITPng;
						}
						else if (stringHasCaseInsensitiveSuffix(imageFilename,
							".bmp"))
						{
							m_imageType = ITBmp;
						}
						else if (stringHasCaseInsensitiveSuffix(imageFilename,
							".jpg"))
						{
							m_imageType = ITJpg;
						}
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
	TCAlertManager::sendAlert(alertClass(), this, _UC("PreSave"));
	if (!m_modelViewer)
	{
		LDPreferences *prefs;
		GLint viewport[4];
		
		glGetIntegerv(GL_VIEWPORT, viewport);
		m_modelViewer = new LDrawModelViewer(viewport[2], viewport[3]);
		m_modelViewer->setFilename(m_modelFilename.c_str());
		m_modelFilename = "";
		m_modelViewer->loadModel();
		prefs = new LDPreferences(m_modelViewer);
		prefs->loadSettings();
		prefs->applySettings();
		prefs->release();
	}
	bool saveAlpha = false;
	TCByte *buffer = grabImage(imageWidth, imageHeight,
		shouldZoomToFit(zoomToFit), NULL, &saveAlpha);
	bool retValue = false;

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
		}
		delete buffer;
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
	if (message == NULL)
	{
		message = _UC("");
	}
	else
	{
		message = TCLocalStrings::get(message);
	}

	TCProgressAlert::send("LDSnapshotTaker", message, progress, &aborted);
	return !aborted;
}

bool LDSnapshotTaker::writeImage(
	const char *filename,
	int width,
	int height,
	TCByte *buffer,
	char *formatName,
	bool saveAlpha)
{
	TCImage *image = new TCImage;
	bool retValue;
	char comment[1024];

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
	m_modelViewer->update();
}

TCByte *LDSnapshotTaker::grabImage(
	int &imageWidth,
	int &imageHeight,
	bool zoomToFit,
	TCByte *buffer,
	bool *saveAlpha)
{
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
	bool canceled = false;
	bool bufferAllocated = false;

	if (zoomToFit)
	{
		m_modelViewer->setForceZoomToFit(true);
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
							smallBytesPerLine);
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
