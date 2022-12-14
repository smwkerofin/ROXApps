import findrox; findrox.version(2, 0, 2)
import rox, rox.choices

import sys
import os

__builtins__._ = rox.i18n.translation(os.path.join(rox.app_dir, 'Messages'))

#import rox.support

from mailers import Mailer
from my_support import *

fname=rox.choices.load('Mail', 'mailers.xml')
if fname!=None:
    mailers=Mailer.read_from(Mailer('dummy', ''), fname)
    mailer=mailers[0]
else:
    mailer=Mailer('elm', '/usr/local/bin/elm', read='xterm -e %s')
    fname=rox.choices.save('Mail', 'mailers.xml', 1)
    mailer.write_to([mailer], fname)

os.spawnv(os.P_WAIT, "/bin/sh", ('sh', '-c', mailer.read_command()))

