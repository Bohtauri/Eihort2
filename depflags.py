#!/usr/bin/env python

# This script tries to find library flags using the following strategy:
# 	1. Try libname-config
# 	2. Try pkg-config libname
#		3. Fall back to -llibname
#
# Multiple libnames can be given. Aliases are supported via -or.

import subprocess

def _system(args):
	pipe = subprocess.Popen(args, stdin = None, stdout = subprocess.PIPE, stderr = subprocess.PIPE)
	stdout, stderr = pipe.communicate()
	return (None, stdout.decode())[pipe.returncode == 0]

def depflags_lib_config(libname):
	try:
		return _system(('%s-config' % libname, '--cflags', '--libs'))
	except OSError:
		pass

def depflags_pkg_config(libname):
	try:
		return _system(('pkg-config', '--cflags', '--libs', libname))
	except OSError:
		pass

def depflags_fallback(libname):
	return '-l%s' % libname

def depflags(*libnames):
	methods = (depflags_lib_config, depflags_pkg_config)
	for libname in libnames:
		for method in methods:
			flags = method(libname)
			if not flags == None:
				return flags
	return depflags_fallback(libnames[-1])

if __name__ == '__main__':
	import sys
	libnames = []
	gen = (arg for arg in sys.argv[1:])
	for arg in gen:
		if arg == '-or':
			libnames[-1].append(next(gen))
		else:
			libnames.append([arg])
	for libname in libnames:
		print((depflags(*libname)))
