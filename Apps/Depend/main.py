# $Id$

"""err"""

import sys

import findrox; findrox.version(1, 9, 12)
import rox

from DepWin import DepWin

if len(sys.argv)>1:
    for f in sys.argv[1:]:
        w=DepWin(f)
        w.show()
else:
    w=DepWin()
    w.show()

rox.mainloop()
