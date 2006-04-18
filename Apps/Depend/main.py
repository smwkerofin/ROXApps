# $Id: main.py,v 1.2 2005/05/07 11:06:48 stephen Exp $

"""err"""

import sys, os

import findrox; findrox.version(2, 0, 0)
import rox, rox.i18n

__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))

from DepWin import DepWin

if len(sys.argv)>1:
    for f in sys.argv[1:]:
        w=DepWin(f)
        w.show()
else:
    w=DepWin()
    w.show()

rox.mainloop()
