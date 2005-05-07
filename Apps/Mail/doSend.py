from sys import argv
import os

import findrox; findrox.version(2, 0, 0)
from rox import g

from SendFile import SendFile

try:
    window = SendFile(argv[1], argv[2])
except:
    window = SendFile(argv[1])

window.connect('destroy', g.mainquit)
window.show()

g.mainloop()
