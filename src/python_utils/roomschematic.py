#!/usr/bin/env python

# this makes a good starting point for the room schematic viewer
# looks nice running with the example1.xml which was built with the circle tool in utilities.

import sys
import pygtk
pygtk.require('2.0')
import gtk
import gtk.gtkgl
from OpenGL.GL import *
from OpenGL.GLU import *
import xml.dom.minidom as minidom

def draw_floor(x,z):
	x=x*0.5
	z=z*0.5
	glBegin(GL_QUADS)
	glColor3f(0.3,0.3,0.3)
	glVertex3f( x, 0,-z)
	glVertex3f(-x, 0,-z)
	glVertex3f(-x, 0, z)
	glVertex3f( x, 0, z)
	glEnd()

def draw_box(x,y,z):
	x=x*.5
	y=y*.5
	z=z*.5
	glBegin(GL_QUADS)
	glColor3f(0.0,1.0,0.0)
	glVertex3f( x, y,-z)
	glVertex3f(-x, y,-z)
	glVertex3f(-x, y, z)
	glVertex3f( x, y, z)

	glColor3f(1.0,0.5,0.0)
	glVertex3f( x,-y, z)
	glVertex3f(-x,-y, z)
	glVertex3f(-x,-y,-z)
	glVertex3f( x,-y,-z)

	glColor3f(1.0,0.0,0.0)
	glVertex3f( x, y, z)
	glVertex3f(-x, y, z)
	glVertex3f(-x,-y, z)
	glVertex3f( x,-y, z)

	glColor3f(1.0,1.0,0.0)
	glVertex3f( x,-y,-z)
	glVertex3f(-x,-y,-z)
	glVertex3f(-x, y,-z)
	glVertex3f( x, y,-z)

	glColor3f(0.0,0.0,1.0)
	glVertex3f(-x, y, z)
	glVertex3f(-x, y,-z)
	glVertex3f(-x,-y,-z)
	glVertex3f(-x,-y, z)

	glColor3f(1.0,0.0,1.0)
	glVertex3f( x, y,-z)
	glVertex3f( x, y, z)	
	glVertex3f( x,-y, z)		
	glVertex3f( x,-y,-z)		
	glEnd()				


class ResoundXMLParser():
	def __init__(self, path):
		self.loudspeakers = []
		self.xmldoc = minidom.parse(path)
		resoundnvNode = self.xmldoc.firstChild
		#print resoundnvNode.toxml()
		for n in resoundnvNode.childNodes:
			if n.nodeName == "loudspeaker":
				#print node.toxml()
				self.loudspeakers.append(n)

g_resound = ResoundXMLParser('../calibration.xml')

class GLDrawingArea(gtk.DrawingArea, gtk.gtkgl.Widget):
	def __init__(self, glconfig):
		gtk.DrawingArea.__init__(self)

		self.set_gl_capability(glconfig)

		self.connect_after('realize',   self._on_realize)
		self.connect('configure_event', self._on_configure_event)
		self.connect('expose_event',	self._on_expose_event)

	def _on_realize(self, *args):
		glwidget = self.get_gl_drawable()
		glcontext = self.get_gl_context()

		if not glwidget.gl_begin(glcontext):
			return

		light_diffuse = [1.0, 1.0, 1.0, 1.0]
		light_position = [0.0, 5.0, 0.0, 1.0]

		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse)
		glLightfv(GL_LIGHT0, GL_POSITION, light_position)

		#glEnable(GL_LIGHTING)
		#glEnable(GL_LIGHT0)
		glEnable(GL_DEPTH_TEST)

		glClearColor(0.8, 0.8, 0.8, 1.0)
		glClearDepth(1.0)

		glMatrixMode(GL_PROJECTION)
		glLoadIdentity()
		gluPerspective(55.0, 1.0, 1.0, 100.0)

		glMatrixMode(GL_MODELVIEW)
		glLoadIdentity()
		gluLookAt(0.0, 5.0, -10.0,  0.0, 1.0, 0.0,  0.0, 1.0, 0.0)

		glwidget.gl_end()

	def _on_configure_event(self, *args):
		glwidget = self.get_gl_drawable()
		glcontext = self.get_gl_context()

		if not glwidget.gl_begin(glcontext):
			return False

		glViewport(0, 0, self.allocation.width, self.allocation.height)
		glwidget.gl_end()
		return False

	def _on_expose_event(self, *args):
		glwidget = self.get_gl_drawable()
		glcontext = self.get_gl_context()

		if not glwidget.gl_begin(glcontext):
			return False

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
		draw_floor(10,10)
		for n in g_resound.loudspeakers:
			x = float(n.attributes['x'].value)
			y = float(n.attributes['y'].value)
			z = float(n.attributes['z'].value)
			glPushMatrix()
			glTranslate(x,y,z)
			draw_box(0.1,0.2,0.15)
			glPopMatrix()

		if glwidget.is_double_buffered():
			glwidget.swap_buffers()
		else:
			glFlush()
		glwidget.gl_end()
		return False

class RoomSchematic(gtk.Window):
	def __init__(self):
		gtk.Window.__init__(self)

		self.set_title('Room Schematic')
		self.set_reallocate_redraws(True)
		self.connect('delete_event', gtk.main_quit)

		vbox = gtk.VBox()
		self.add(vbox)

		button = gtk.Button('Exit')
		button.connect('clicked', gtk.main_quit)
		vbox.pack_start(button, expand=False)

		display_mode = (gtk.gdkgl.MODE_RGB	| gtk.gdkgl.MODE_DEPTH  | gtk.gdkgl.MODE_DOUBLE)
		glconfig = gtk.gdkgl.Config(mode=display_mode)

		drawing_area = GLDrawingArea(glconfig)
		drawing_area.set_size_request(640, 480)
		vbox.pack_start(drawing_area)


class _Main(object):
	def __init__(self, app):
		self.app = app

	def run(self):
		self.app.show_all()
		gtk.main()

if __name__ == '__main__':
	_Main(RoomSchematic()).run()

