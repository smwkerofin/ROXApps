# $Id$

import rox, rox.options

allow_pw_change=rox.options.Option('allow_pw_change', True)
wait_for=rox.options.Option('wait_for', 3)
use_dl_manager=rox.options.Option('use_dl_manager', False)
block_size=rox.options.Option('block_size', 8192)
update_title=rox.options.Option('update_title', True)
install_path=rox.options.Option('install_path',
                                '%s/AppRun "$1"' % rox.app_dir)
set_type=rox.options.Option('set_type', True)
