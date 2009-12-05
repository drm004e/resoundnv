# This file is part of Resound
# Copyright 2009 Dave Moore and James Mooney

import math
def circle_array(id, N, r, y):
	pi = 3.14159
	for n in range(0,N):
		a = n*(2.0*pi/N)
		x = math.sin(a) * r
		z = math.cos(a) * r
		print "<loudspeaker id=\"{id}-{n}\" type=\"Genelec 1029\"x=\"{x}\" y=\"{y}\" z=\"{z}\" port=\"playback_{n}\"/>".format(id=id,n=n+1,x=x,y=y,z=z)

circle_array("circle1",20,2,0.1)
circle_array("circle2",8,1,1)
circle_array("circle3",15,2,3)
