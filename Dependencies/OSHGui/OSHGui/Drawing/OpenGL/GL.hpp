/*
 * OldSchoolHack GUI
 *
 * by KN4CK3R http://www.oldschoolhack.me
 *
 * See license in OSHGui.hpp
 */

#ifndef OSHGUI_DRAWING_OPENGL_HPP
#define OSHGUI_DRAWING_OPENGL_HPP

#ifndef __APPLE__
#   if (defined( __WIN32__ ) || defined( _WIN32 ))
#       include <windows.h>
#   endif
#   include <GL/gl.h>
#   include <GL/glu.h>
#else
#   include <OpenGL/gl.h>
#   include <OpenGL/glu.h>
#endif

#include <glm/glm.hpp>

#endif