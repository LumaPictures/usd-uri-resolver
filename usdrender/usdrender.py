#!/usr/bin/python

import pxr.Usdviewq as Usdviewq

from OpenGL import GL as gl
from OpenGL import GLX as glx
from OpenGL.raw._GLX import struct__XDisplay
from Xlib import X as x
from Xlib import display
from ctypes import *
import time
import OpenImageIO as oiio
import sys
import array
import numpy

# http://stackoverflow.com/questions/14933584/how-do-i-use-the-xlib-and-opengl-modules-together-with-python
if __name__ == '__main__':
    #from pxr import Work
    #Work.SetConcurrencyLimitArgument(0)
    # we are creating an actual window here, and there is a good reason for that
    # if you are using windowless opengl, you have to manually setup renderbuffers
    # which can potentially mess up hydra's own renderbuffer usage
    # so making that compatible with hydra is more hassle than creating a dummy
    # window that just display the rendered frames while doing the rendered
    width = 512
    height = 512
    output_filename = '/home/palm/test.png'

    pd = display.Display()
    sc = pd.screen()
    pw = sc.root.create_window(0, 0, width, height, 2,
                               sc.root_depth, x.InputOutput, x.CopyFromParent,
                               background_pixel = sc.white_pixel,
                               colormap = x.CopyFromParent)
    pw.map()
    pd.sync()
    xid = pw.__resource__()

    xlib = cdll.LoadLibrary("libX11.so")
    xlib.XOpenDisplay.argtypes = [c_char_p]
    xlib.XOpenDisplay.restype = POINTER(struct__XDisplay)
    d = xlib.XOpenDisplay("")

    elements = c_int()
    configs = glx.glXChooseFBConfig(d, 0, None, byref(elements))
    # this will fail with an invalid parameter all the time for some reason
    #w = glx.glXCreateWindow(d, configs[0], c_ulong(xid), None)
    context = glx.glXCreateNewContext(d, configs[0], glx.GLX_RGBA_TYPE, None, True)
    #glx.glXMakeContextCurrent(d, w, w, context)
    glx.glXMakeCurrent(d, xid, context)

    gl.glViewport(0, 0, width, height)

    # test python opengl code
    gl.glShadeModel(gl.GL_FLAT)
    gl.glClearColor(0.5, 0.5, 0.5, 1.0)
    gl.glViewport(0, 0, 200, 200)
    gl.glMatrixMode(gl.GL_PROJECTION)
    gl.glLoadIdentity()
    gl.glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0)
    gl.glClear(gl.GL_COLOR_BUFFER_BIT)
    gl.glColor3f(1.0, 1.0, 0.0)
    gl.glRectf(-0.8, -0.8, 0.8, 0.8)

    data = gl.glReadPixels(0, 0, width, height, gl.GL_RGB, gl.GL_FLOAT, outputType = None)

    outspec = oiio.ImageSpec(width, height, 3, oiio.FLOAT)
    output = oiio.ImageOutput.create(output_filename)
    if output == None:
        print 'Error creating the output for ', output_filename
        sys.exit(0)

    ok = output.open(output_filename, outspec, oiio.Create)
    if not ok:
        print 'Could not open ', output_filename
        sys.exit(0)

    output.write_image(array.array('f', data.flatten().tolist()))
    output.close()

    # swap buffers, by default things are double buffered
    glx.glXSwapBuffers(d, xid)

    time.sleep(5)
