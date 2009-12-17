import math

A = [0.,0.,0.]
B = [1.,0.,0.]
C = [1.,1.,0.]

D = [-20,-30,0]

def distance(a,b):
	c = [ a[0]-b[0], a[1]-b[1], a[2]-b[2] ]
	return math.sqrt(c[0]**2 + c[1]**2 + c[2]**2);

r1 = distance(D,A)
r2 = distance(D,B)
r3 = distance(D,C)
print r1, r2, r3

# eq1: x = (r1^2 - r2^2 + d^2) / 2d
#if d = 1 - ie. 1 meter gap in x axis
# x= (r1^2 - r2^2 + 1) / 2
x = (r1**2 - r2**2 + 1.0) / 2.0
print x

y = (r1**2 - r3**2 + 2.0) / 2.0 - x
print y

z = math.sqrt(r1**2 - x**2 - y**2)
print z


