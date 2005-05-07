# $Id: main.py,v 1.1.1.1 2004/04/17 11:34:56 stephen Exp $

"""err"""

import sys

import findrox; findrox.version(2, 0, 0)
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
