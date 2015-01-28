import os, sys, zipfile
arch = zipfile.ZipFile(sys.argv[1], "w", zipfile.ZIP_DEFLATED)

def items(src):
	if not os.path.isdir(src):
		yield src
	else:
		for root, dirs, files in os.walk(src):
			yield root
			for child in dirs + files:
				yield os.path.join(root, child)

for src in sys.argv[2:]:
	parent = os.path.normpath(os.path.join(src, ".."))
	for item in items(src):
		arch.write(item, os.path.relpath(item, parent))