#!/usr/bin/env python

# this makes a good starting point for the room schematic viewer
# looks nice running with the example1.xml which was built with the circle tool in utilities.

import sys
import pygtk
pygtk.require('2.0')
import gtk
import gobject
import gtk.gtkgl
from OpenGL.GL import *
from OpenGL.GLU import *
from OpenGL.GLUT import *
import xml.dom.minidom as minidom
import liblo
import math
import getopt





# create osc server, listening on port 8005
try:
	oscServer = liblo.Server(8005)
except liblo.ServerError, err:
	print str(err)
	sys.exit()

# make osc address and send the syn message to get comms
try:
	target = liblo.Address(8000)
except liblo.AddressError, err:
	print str(err)
	sys.exit()

# send message "/foo/message1" with int, float and string arguments
liblo.send(target, "/syn", 8005)

def draw_floor(x,z):
	x=x*0.5
	z=z*0.5
	# this would be better as a grid because the gllighting works better
	glBegin(GL_QUADS)
	glColor3f(0.3,0.3,0.3)
	glNormal3f( 0, 1, 0)
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

	glNormal3f( 0, 1, 0)
	glVertex3f( x, y,-z)
	glVertex3f(-x, y,-z)
	glVertex3f(-x, y, z)
	glVertex3f( x, y, z)

	glNormal3f( 0, -1, 0)
	glVertex3f( x,-y, z)
	glVertex3f(-x,-y, z)
	glVertex3f(-x,-y,-z)
	glVertex3f( x,-y,-z)

	glNormal3f( 0, 0, 1)
	glVertex3f( x, y, z)
	glVertex3f(-x, y, z)
	glVertex3f(-x,-y, z)
	glVertex3f( x,-y, z)

	glNormal3f( 0, 0, -1)
	glVertex3f( x,-y,-z)
	glVertex3f(-x,-y,-z)
	glVertex3f(-x, y,-z)
	glVertex3f( x, y,-z)

	glNormal3f( -1, 0, 1)
	glVertex3f(-x, y, z)
	glVertex3f(-x, y,-z)
	glVertex3f(-x,-y,-z)
	glVertex3f(-x,-y, z)

	glNormal3f( 1, 0, 1)
	glVertex3f( x, y,-z)
	glVertex3f( x, y, z)	
	glVertex3f( x,-y, z)		
	glVertex3f( x,-y,-z)		
	glEnd()				


def fallback(path, args, types, src):
	#print "Unknown message '%s' from '%s'" % (path, src.get_url())
	#for a, t in zip(args, types):
	#	print "argument of type '%s': %s" % (t, a)
	pass

def handle_osc_vu(path, args, types, src, data):
	#print "received message '%s'" % path
	#for a, t in zip(args, types):
	#	print "argument of type '%s': %s" % (t, a)
	data.rms = float(args[0])
	data.peak = float(args[1])
	data.margin = float(args[2])

class Loudspeaker():
	def __init__(self, node):
		# register osc callback on this
		self.id = str(node.attributes['id'].value)
		self.x = float(node.attributes['x'].value)
		self.y = float(node.attributes['y'].value)
		self.z = float(node.attributes['z'].value)
		self.peak = 0.0
		self.rms = 0.0
		self.margin = 0.0
		# speaker dimensions from genelec 1029
		self.height = 0.247
		self.width = 0.151
		self.depth = 0.191
		oscServer.add_method("/"+self.id, 'fff', handle_osc_vu, self)

	def render(self):
		try:
			glTranslate(self.x,self.y,self.z)


			glColor3f(0.2,0.2,0.2)
			if self.peak >= 0.3: glColor3f(1.0,0.2,0.2) #  	ffa500 - orange
			if self.peak >= 0.8: glColor3f(1,0,0) #
			draw_box(self.width, self.height,self.depth)
			glColor4f(0,1,0,0.1)
			if self.peak >= 0.3: glColor4f(1.0,0.2,0.2,0.1) #  	ffa500 - orange
			if self.peak >= 0.8: glColor4f(1,0,0,0.1) #
			glutSolidSphere(self.peak*3+0.2,10,10)

		except: 
			print "Render error"
			# careful here because the gl library throw exceptions
			# we have to catch them else the glPushMatrix causes a stack overflow
		

class ResoundXMLParser():
	def __init__(self, path):

		self.loudspeakers = []
		self.xmldoc = minidom.parse(path)
		resoundnvNode = self.xmldoc.firstChild
		#print resoundnvNode.toxml()
		for n in resoundnvNode.childNodes:
			if n.nodeName == "loudspeaker":
				#print node.toxml()
				self.loudspeakers.append(Loudspeaker(n))


		# register a fallback for unhandled messages
		oscServer.add_method(None, None, fallback)
			

global g_resound;

# this is an on idle callback to check osc
def update_osc():
	oscServer.recv(10);
	return True

class GLDrawingArea(gtk.DrawingArea, gtk.gtkgl.Widget):
	def __init__(self, glconfig):
		gtk.DrawingArea.__init__(self)

		self.set_gl_capability(glconfig)

		self.connect_after('realize',   self._on_realize)
		self.connect('configure_event', self._on_configure_event)
		self.connect('expose_event',	self._on_expose_event)
		gobject.timeout_add(50,self.animate_callback)

		self.phasor = 0.0

	def _on_realize(self, *args):
		glwidget = self.get_gl_drawable()
		glcontext = self.get_gl_context()
		glutInit()
		# enable alpha blending, not that nice though beacuse we should draw in a certain order
		glEnable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		if not glwidget.gl_begin(glcontext):
			return

		light_diffuse = [1.0, 1.0, 1.0, 1.0]
		light_position = [0.0, 5.0, 0.0, 1.0]

		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse)
		glLightfv(GL_LIGHT0, GL_POSITION, light_position)
		#glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.01);

		glEnable(GL_COLOR_MATERIAL)
		# set material properties which will be assigned by glColor
		glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE)

		glEnable(GL_LIGHTING)
		glEnable(GL_LIGHT0)
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
		glMatrixMode(GL_MODELVIEW)
		glLoadIdentity()
		self.phasor += 0.005
		if self.phasor >= 1.0 : self.phasor = 0.0

		gluLookAt(math.sin(self.phasor * 2.0 * math.pi) * 10, 5.0, math.cos(self.phasor * 2.0 * math.pi) * 10,  0.0, 0.0, 0.0,  0.0, 1.0, 0.0)

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

		# must reposition the light here so it is affected by the viewing transform
		light_diffuse = [1.0, 1.0, 1.0, 1.0]
		light_position = [0.0, 5.0, 0.0, 1.0]

		glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse)
		glLightfv(GL_LIGHT0, GL_POSITION, light_position)

		glDisable(GL_LIGHTING)
		draw_floor(10,10)
		glEnable(GL_LIGHTING)

		for l in g_resound.loudspeakers:
			glPushMatrix()
			l.render()
			glPopMatrix()

		if glwidget.is_double_buffered():
			glwidget.swap_buffers()
		else:
			glFlush()
		glwidget.gl_end()
		return False

	def animate_callback(self):
		self.queue_draw()
		return True

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
	def usage():
		print "Loads a server monitoring application with visualisation.\n Usage:\nroommonitor.py --input=resoundfile.xml"
	def __init__(self, app):
		try:
			opts, args = getopt.getopt(sys.argv[1:], "hi:v", ["help", "input="])
		except getopt.GetoptError, err:
			print str(err) # print option errors
			self.usage()
			sys.exit(2)
		output = None
		verbose = False
		for o, a in opts:
			if o == "-v":
				verbose = True
			elif o in ("-h", "--help"):
				self.usage()
				sys.exit()
			elif o in ("-i", "--input"):
				if a=="":
					self.usage()
					sys.exit()
				self.path=a
			else:
				assert False, "unhandled option"
		global g_resound
		g_resound = ResoundXMLParser(self.path)
		self.app = app

	def run(self):
		gobject.idle_add(update_osc)
		self.app.show_all()
		gtk.main()

if __name__ == '__main__':
	_Main(RoomSchematic()).run()

