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

# we are creating an actual window here, and there is a good reason for that
# if you are using windowless opengl, you have to manually setup renderbuffers
# which can potentially mess up hydra's own renderbuffer usage
# so making that compatible with hydra is more hassle than creating a dummy
# window that just display the rendered frames while doing the rendered
# Note the glXCreateWindowContext failed with an invalid parameter
# so I'm just using the CreateNewContext directly
# http://stackoverflow.com/questions/14933584/how-do-i-use-the-xlib-and-opengl-modules-together-with-python
def create_window_and_context(width, height):
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
    context = glx.glXCreateNewContext(d, configs[0], glx.GLX_RGBA_TYPE, None, True)

    return (context, xid, d)

def export_image(output_filename, width, height):
    data = gl.glReadPixels(0, 0, args.width, args.height, gl.GL_RGB, gl.GL_FLOAT, outputType = None)

    outspec = oiio.ImageSpec(args.width, args.height, 3, oiio.FLOAT)
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

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description='''
    Rendering a sequence of images using usdImaging.
    ''')
    parser.add_argument(
        '--width', dest = 'width', type = int, action = 'store',
        default = 512, help = 'Width of the rendered image.'
    )
    parser.add_argument(
        '--height', dest = 'height', type = int, action = 'store',
        default = 512, help = 'Height of the rendered image.'
    )
    parser.add_argument(
        '--output', '-o', dest = 'output', type = str, action = 'store',
        default = 'output_%04d.png', help = 'Output path, including frame number formatting.'
    )
    parser.add_argument(
        '--firstframe', '-ff', dest = 'firstframe', type = int, action = 'store',
        default = 1, help = 'First frame of the rendered sequence'
    )
    parser.add_argument(
        '--lastframe', '-lf', dest = 'lastframe', type = int, action = 'store',
        default = 1, help = 'First frame of the rendered sequence'
    )
    parser.add_argument(
        '--numThreads', '-n', dest = 'numthreads', type = int, action = 'store',
        default = 0, help = 'Number of threads used for processing.'
    )
    parser.add_argument(
        '--select', '-s', dest = 'select', type = str, action = 'store',
        default = '/', help = 'Select a prim to render.'
    )
    parser.add_argument(
        '--camera', '-c', dest = 'camera', type = str, action = 'store',
        default = '', help = 'Camera to render from.'
    )
    parser.add_argument(
        '--complexity', dest = 'complexity', type = float, action = 'store',
        default = 1.0, help = 'Subdivision complexity. [1.0, 2.0]'
    )
    parser.add_argument(
        'usdfile', type = str, action = 'store',
        help = 'USD file to render.'
    )
    args = parser.parse_args()
    from pxr import Work
    Work.SetConcurrencyLimitArgument(args.numthreads)

    ctx, wid, dp = create_window_and_context(args.width, args.height)

    glx.glXMakeCurrent(dp, wid, ctx)
    gl.glViewport(0, 0, args.width, args.height)

    import random
    random.seed(42)

    for frame in range(args.firstframe, args.lastframe + 1):
        # drawing something for testing
        gl.glShadeModel(gl.GL_FLAT)
        gl.glClearColor(0.5, 0.5, 0.5, 1.0)
        gl.glMatrixMode(gl.GL_PROJECTION)
        gl.glLoadIdentity()
        gl.glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0)
        gl.glClear(gl.GL_COLOR_BUFFER_BIT)
        gl.glColor3f(1.0, 1.0, 0.0)
        gl.glRectf(random.uniform(-1.0, 0.0), random.uniform(-1.0, 0.0),
                   random.uniform(0.0, 1.0), random.uniform(0.0, 1.0))

        export_image(args.output % frame, args.width, args.height)
        # swap buffers, by default things are double buffered
        glx.glXSwapBuffers(dp, wid)
