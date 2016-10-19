#!/usr/bin/python

import pxr.Usdviewq as Usdviewq

from OpenGL import GL as gl
from OpenGL import GLX as glx
from OpenGL.raw._GLX import struct__XDisplay
from Xlib import X as x
from Xlib import display
from ctypes import *
import OpenImageIO as oiio
import sys, os
import array
import numpy
from math import *
from pxr import Work, Usd, UsdGeom, Ar, Sdf, UsdImagingGL, Gf, Glf, CameraUtil

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
    buf = oiio.ImageBuf(outspec)
    buf.set_pixels(oiio.ROI(0, width, 0, height, 0, 1, 0, 3), array.array('f', data.flatten().tolist()))
    buf_flipped = oiio.ImageBuf(outspec)
    oiio.ImageBufAlgo.flip(buf_flipped, buf)
    buf_flipped.write(output_filename)

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
        '--firstframe', '--ff', '-ff', dest = 'firstframe', type = int, action = 'store',
        default = 1, help = 'First frame of the rendered sequence'
    )
    parser.add_argument(
        '--lastframe', '--lf', '-lf', dest = 'lastframe', type = int, action = 'store',
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
        '--renderer', '-r', dest = 'renderer', type = str, action = 'store',
        choices = ['opt', 'simple'], default = 'opt', help = 'Which renderer to use.'
    )
    parser.add_argument(
        'usdfile', type = str, action = 'store',
        help = 'USD file to render.'
    )
    args = parser.parse_args()
    if args.renderer == 'simple':
        os.environ['HD_ENABLED'] = '0'


    Work.SetConcurrencyLimitArgument(args.numthreads)

    # TODO: don't load up all the stage upfront
    stage = Usd.Stage.Open(
        args.usdfile,
        Ar.GetResolver().CreateDefaultContextForAsset(args.usdfile),
        Usd.Stage.LoadAll
    )

    # this is mandatory
    camera_prim = stage.GetPrimAtPath(Sdf.Path(args.camera))
    render_root = stage.GetPrimAtPath(Sdf.Path(args.select))

    ctx, wid, dp = create_window_and_context(args.width, args.height)

    glx.glXMakeCurrent(dp, wid, ctx)
    gl.glViewport(0, 0, args.width, args.height)

    # initializing renderer
    Glf.GlewInit()
    Glf.RegisterDefaultDebugOutputMessageCallback()
    renderer = UsdImagingGL.GL()
    render_params = UsdImagingGL.GL.RenderParams()
    render_params.complexity = 1.0
    render_params.drawMode = UsdImagingGL.GL.DrawMode.DRAW_SHADED_SMOOTH
    render_params.showGuides = True
    render_params.showRenderGuides
    render_params.cullStyle = UsdImagingGL.GL.CullStyle.CULL_STYLE_BACK_UNLESS_DOUBLE_SIDED
    render_params.enableIdRender = False
    render_params.gammaCorrectColors = False
    render_params.enableSampleAlphaToCoverage = False
    render_params.highlight = False
    render_params.enableHardwareShading = True
    render_params.displayImagePlanes = False

    import random
    random.seed(42)

    target_aspect = float(args.width) / max(1.0, args.height)

    gl.glShadeModel(gl.GL_SMOOTH)

    gl.glEnable(gl.GL_DEPTH_TEST)
    gl.glDepthFunc(gl.GL_LESS)

    gl.glBlendFunc(gl.GL_SRC_ALPHA, gl.GL_ONE_MINUS_SRC_ALPHA)
    gl.glEnable(gl.GL_BLEND)
    gl.glColor3f(1.0, 1.0, 1.0)
    gl.glMaterialfv(gl.GL_FRONT_AND_BACK, gl.GL_AMBIENT, (0.2, 0.2, 0.2, 1.0))
    gl.glMaterialfv(gl.GL_FRONT_AND_BACK, gl.GL_SPECULAR, (0.5, 0.5, 0.5, 1.0))
    gl.glMaterialfv(gl.GL_FRONT_AND_BACK, gl.GL_SHININESS, 32.0)

    for frame in range(args.firstframe, args.lastframe + 1):
        render_params.frame = frame
        render_params.forceRefresh = True
        camera = UsdGeom.Camera(camera_prim).GetCamera(frame, False)
        CameraUtil.ConformWindow(camera, CameraUtil.MatchVertically, target_aspect)
        frustum = camera.frustum
        renderer.SetCameraState(
            frustum.ComputeViewMatrix(),
            frustum.ComputeProjectionMatrix(),
            Gf.Vec4d(0, 0, args.width, args.height)
        )
        render_params.clipPlanes = [Gf.Vec4d(i) for i in camera.clippingPlanes]
        cam_pos = frustum.position
        gl.glEnable(gl.GL_LIGHTING)
        gl.glEnable(gl.GL_LIGHT0)
        gl.glLightfv(gl.GL_LIGHT0, gl.GL_POSITION, (cam_pos[0], cam_pos[1], cam_pos[2], 1))
        gl.glClear(gl.GL_COLOR_BUFFER_BIT | gl.GL_DEPTH_BUFFER_BIT)

        renderer.SetLightingStateFromOpenGL()
        renderer.Render(render_root, render_params)

        gl.glFlush()

        export_image(args.output % frame, args.width, args.height)
        # swap buffers, by default things are double buffered
        glx.glXSwapBuffers(dp, wid)
