# This file is part of Resound
# Copyright 2009 Dave Moore and James Mooney

import math
def circle_array(id, N, r, y, port, inOffset=1, outOffset=1):
	pi = 3.14159
	for n in range(0,N):
		a = n*(2.0*pi/N)
		x = math.sin(a) * r
		z = math.cos(a) * r
		print "<loudspeaker id=\"{id}{inp}\" type=\"Genelec 1029\" port=\"{portPrefix}{outp}\" x=\"{x:.2f}\" y=\"{y:.2f}\" z=\"{z:.2f}\"/>".format(id=id, inp=n+inOffset, outp=n+outOffset,x=x,y=y,z=z, portPrefix=port)

circle_array("G",8,3,0, 'pure_data_0:input',1,0)
