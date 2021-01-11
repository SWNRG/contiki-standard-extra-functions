#!/usr/bin/python
import os

file2read = "positions.dat"
file2write = './10n-320X320-2hr-after-1hr.dat'
try:
	file2read = open(file2read,"r")
	if os.path.exists(file2write):
		append_write = 'a' # append if already exists
	else:
		append_write = 'w' # make a new file if not
	file2write = open(file2write,append_write)
	
	for line in file2read:
		if ( line.startswith('0 ')):
			line="10 "+line[2:]
			print (line)
		file2write.write(line)
	file2write.truncate()

	file2read.close()
	file2write.close()
	
except Exception as (e):
	print("file not found...")
	



