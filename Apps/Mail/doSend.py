from sys import argv
import os

from gtk import *
from GDK import *

from SendFile import SendFile

try:
    window = SendFile(argv[1], argv[2])
except:
    window = SendFile(argv[1])

window.connect('destroy', mainquit)
window.show()

mainloop()
