# $Id: config.py,v 1.3 2006/12/17 12:33:49 stephen Exp $

import os, sys
import urlparse

import rox
import rox.options
import rox.basedir
import rox.loading
import rox.mime
import rox.filer
# Get the uri handler from ROX-Lib, if available.
try:
	from rox import uri_handler
except ImportError:
	# Use our own copy
	import uri_handler
import helper

BROWSER=os.getenv('BROWSER')
if BROWSER:
	if '%s' in BROWSER:
		web_cmd=BROWSER.replace('%s', '"%@"')
	else:
		web_cmd=BROWSER+' "$@"'
else:
	web_cmd='firefox "$@""'

flag_line='# Set by Launch.  Delete this line if you edit this file\n'

def migrate():
	options=rox.options.OptionGroup('Launch', 'Options.xml')

	handlers={'http': web_cmd}

	http_cmd=rox.options.Option('http', web_cmd, options)
	
	schemes=rox.options.ListOption('schemes', ('http=%s' % web_cmd,),
				       options)

	options.notify()

	if http_cmd.value!=web_cmd:
		handlers['http']=http_cmd.value

	for s in schemes.list_value:
		scheme, action=s.split('=', 1)
		handlers[scheme]=action

	for scheme, action in handlers.iteritems():
		caction=uri_handler.get(scheme)
		if not caction:
			cmd=action.replace('%s', '"$@"')
			path=rox.basedir.save_config_path('rox.sourceforge.net', 'URI')
		
			f=file(os.path.join(path, scheme), 'w')
			f.write('#!/bin/sh\n')
			f.write(flag_line)
			f.write('exec %s\n' % cmd)
			f.close()
			os.chmod(f.name, 0700)

	fname=rox.choices.save('Launch', '.migrated', False)
	if fname:
		f=file(fname, 'w')
		f.close()

def initialize():
	path=rox.basedir.save_config_path('rox.sourceforge.net', 'URI')
	f=file(os.path.join(path, "http"), 'w')
	f.write('#!/bin/sh\n')
	f.write(flag_line)
	f.write('exec %s\n' % web_cmd)
	f.close()
	os.chmod(f.name, 0700)
		
class DropTarget(rox.loading.XDSLoader, rox.g.Frame):
	def __init__(self, parent):
		self.controller=parent
		rox.g.Frame.__init__(self, _('Drop program here'))
		rox.loading.XDSLoader.__init__(self, None)

		self.set_size_request(100, 100)
		self.set_shadow_type(rox.g.SHADOW_IN)
		self.set_border_width(4)

		vbox=rox.g.VBox(False, 8)
		self.add(vbox)
		self.img=rox.g.Image()
		vbox.pack_start(self.img, True, True)
		self.msg=rox.g.Label()
		align=rox.g.Alignment(0.5, 0.5, 1.0, 0.0)
		vbox.pack_start(align, False, False)
		align.add(self.msg)

	def set_action(self, path):
		if rox.isappdir(path):
			try:
				p=os.path.join(path, '.DirIcon')
				self.img.set_from_file(p)
			except:
				self.img.set_from_stock(rox.g.STOCK_DIALOG_QUESTION,
							rox.g.ICON_SIZE_DIALOG)	
		else:
			try:
				mtype=rox.mime.get_type(path)
				icon=mtype.get_icon(rox.mime.ICON_SIZE_LARGE)
				self.img.set_from_pixbuf(icon)
			except:
				self.img.set_from_stock(rox.g.STOCK_DIALOG_QUESTION,
							rox.g.ICON_SIZE_DIALOG)
				
		if os.path.islink(path):
			p=os.readlink(path)
			self.msg.set_text(os.path.basename(p))
		elif rox.isappdir(path):
			self.msg.set_text(os.path.basename(path))
		else:
			self.msg.set_text('')

	def clear_action(self):
		self.img.set_from_stock(rox.g.STOCK_DIALOG_QUESTION,
					rox.g.ICON_SIZE_DIALOG)
		self.msg.set_text('')

	def xds_load_from_file(self, path):
		self.controller.set_current_action(path)

class SetWindow(rox.Dialog):
	def __init__(self):
		rox.Dialog.__init__(self, title=_('Define URI handlers'),
				    buttons=(
			rox.g.STOCK_HELP, rox.g.RESPONSE_HELP,
			rox.g.STOCK_CLOSE, rox.g.RESPONSE_ACCEPT))

		self.ttips=rox.g.Tooltips()

		ebox=rox.g.EventBox()
		hbox=rox.g.HBox(False, 2)
		ebox.add(hbox)
		self.vbox.pack_start(ebox, False, False)

		hbox.pack_start(rox.g.Label(_('Scheme')), False, False)

		self.scheme=rox.g.combo_box_entry_new_text()
		self.scheme.connect('changed', self.select_scheme)
		hbox.pack_start(self.scheme, True, True)
		self.ttips.set_tip(ebox, _('Enter or select a scheme here.  The scheme is the part before : in a URI.'))

		self.drop_target=DropTarget(self)
		ebox=rox.g.EventBox()
		ebox.add(self.drop_target)
		self.vbox.pack_start(ebox, True, True)
		self.ttips.set_tip(ebox, _('Drag and drop a program or application here to set it as the handler for a URI scheme.'))

		self.cmd=rox.g.Entry()
		self.cmd.set_text(' "$@"')
		self.cmd.connect('activate', self.set_handler)
		self.cmd.set_activates_default(False)
		self.vbox.pack_start(self.cmd, False, False)
		self.ttips.set_tip(self.cmd,
				   _('Enter a command here.  The characters "$@" will be replaced with the URI when called.'))

		self.msg=rox.g.Label()
		self.vbox.pack_start(self.msg, False, False)

		self.load_schemes()

		self.vbox.show_all()
		self.connect('response', self.get_response)

	def get_response(self, window, response):
		if response==int(rox.g.RESPONSE_HELP):
			helper.show_help(parent=self)
		else:
			self.hide()
			self.destroy()

	def load_schemes(self):
		self.schemes=[]
		for path in rox.basedir.load_config_paths('rox.sourceforge.net', 'URI'):
			for s in os.listdir(path):
				if s=='file':
					continue 
				if s not in self.schemes:
					self.schemes.append(s)
		self.schemes.sort()

		for scheme in self.schemes:
			self.scheme.append_text(scheme)
			
		self.scheme.set_active(0)

	def select_scheme(self, *unused):
		scheme=self.scheme.get_active_text()
		if not scheme:
			self.cmd.set_text('')
			self.drop_target.clear_action()
			self.msg.set_text('')
			return
		action=uri_handler.get(scheme)
		#print action
		if not action:
			self.cmd.set_text('')
			self.drop_target.clear_action()
			self.msg.set_text(_('No action for %s') % scheme)
			return
		#print action
		if os.path.basename(action)=='AppRun' and rox.isappdir(os.path.dirname(action)):
			action=os.path.dirname(action)
		self.cmd.set_text(action)
		if os.access(action, os.F_OK) and not os.path.islink(action):
			#print action
			ours=False
			nl=0
			for line in file(action, 'r'):
				nl+=1
				if line==flag_line:
					ours=True
				elif line[:5]=='exec ' and ours:
					self.cmd.set_text(line[5:].strip())
				elif nl>3:
					break
		self.drop_target.set_action(action)
		self.msg.set_text('')

	def set_handler(self, *unused):
		scheme=self.scheme.get_active_text()
		action=self.cmd.get_text()
		#print scheme, action

		if scheme=='file':
			self.msg.set_text(_('Refusing to set handler for "file:" scheme'))
			return

		tdir=rox.basedir.save_config_path('rox.sourceforge.net', 'URI')
		if not self.check_overwrite(tdir, scheme):
			return
		
		if rox.isappdir(action):
			self.set_appdir(tdir, scheme, action)
		elif os.access(action, os.X_OK):
			self.set_program(tdir, scheme, action)
		else:
			self.set_command(tdir, scheme, action)

		model=self.scheme.get_model()
		if model:
			found=False
			for row in iter(model):
				#print row, row[0]
				if row[0]==scheme:
					found=True
					break
			if not found:
				self.scheme.append_text(scheme)

	def check_overwrite(self, tdir, scheme):
		current=os.path.join(tdir, scheme)

		if os.access(current, os.F_OK) and not os.path.islink(current):
			msg=_("Action for %s: already set, really change?") % scheme
			if not rox.confirm(msg, rox.g.STOCK_APPLY,
					   _('Change')):
				return False
			try:
				os.remove(current)
			except:
				rox.report_exception()
				rox.filer.open_dir(tdir)
				return False
			
		elif os.access(current, os.F_OK):
			# Link to valid object, remove it
			try:
				os.remove(current)
			except:
				rox.report_exception()
				return False
			
		elif os.path.islink(current):
			# Broken link, remove it
			try:
				os.remove(current)
			except:
				rox.report_exception()
				return False
			
		return True

	def set_appdir(self, tdir, scheme, app):
		path=os.path.join(tdir, scheme)
		os.symlink(app, path)
		self.msg.set_text(_('Set application for %s') % scheme)

	def set_program(self, tdir, scheme, prog):
		path=os.path.join(tdir, scheme)
		os.symlink(prog, path)
		self.msg.set_text(_('Set program for %s') % scheme)

	def set_command(self, tdir, scheme, cmd):
		if '$@' not in cmd:
			cmd+=' "$@"'
			self.cmd.set_text(cmd)
		path=os.path.join(tdir, scheme)
		f=file(path, 'w')
		f.write('#!/bin/sh\n')
		f.write(flag_line)
		f.write('exec %s\n' % cmd)
		f.close()
		os.chmod(f.name, 0700)
		self.msg.set_text(_('Set command for %s') % scheme)

	def set_current_action(self, path):
		self.cmd.set_text(path)
		self.drop_target.set_action(path)
		self.set_handler()

def main():
    win=SetWindow()
    win.show()
    rox.mainloop()

if (rox.choices.load('Launch', 'Options.xml') and
    not rox.choices.load('Launch', '.migrated')):
    migrate()
elif not rox.basedir.load_first_config('rox.sourceforge.net', 'URI'):
    initialize()

