#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#else // WIN32
#include <netinet/in.h>
#endif // WIN32

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/GLUT.h>
#define APIENTRY
#else	// __APPLE__
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#endif	// __APPLE__

#ifdef WIN32
#include <GL/wglext.h>
#else // WIN32

//	WGL ext stuff
typedef void * (APIENTRY * PFNWGLALLOCATEMEMORYNVPROC) (int size, float readfreq, float writefreq, float priority);
typedef void (APIENTRY * PFNWGLFREEMEMORYNVPROC) (void *pointer);

//	ext stuff
typedef void (APIENTRY * PFNGLFLUSHVERTEXARRAYRANGENVPROC) (void);
typedef void (APIENTRY * PFNGLVERTEXARRAYRANGENVPROC) (GLsizei size, const GLvoid *pointer);
typedef void (APIENTRY * PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRY * PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRY * PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY * PFNGLBUFFERDATAARBPROC) (GLenum target, int size, const GLvoid *data, GLenum usage);

#define GL_ARRAY_BUFFER_ARB 0x8892
#define GL_STATIC_DRAW_ARB 0x88E4
typedef int GLsizeiptrARB;

#endif // WIN32
