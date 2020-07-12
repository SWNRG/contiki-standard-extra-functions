#!/usr/bin/python

import os
from os import system
import fileinput
import shutil
import sys
import argparse

# PARAMETERS TO CHANGE:
# PERIOD 60 means 60 packets per hour
# PERIOD 6 means 600 packets per hour

# changeParams.py -mp $mobPer -fp $fixPer -fi $fid -mi $mobId

def ParseCommandLine():
    parser = argparse.ArgumentParser(description="Changing project-conf parameters dynamically")

    # Altering freaquency of messages
    parser.add_argument('-per', '--Period', help='Period for sending UDP packets from stations',required=False)

    parser.add_argument('-dqw', '--DixonQWindow', help='Window memory (How many values to remember for Dixon Q-Test)',required=True)

    parser.add_argument('-dconf', '--DixonConfidence', help='Dixon confidence from 0-2',required=True)
        
    args = parser.parse_args()

    # print("Stations Period: {}".format(args.Period))

    return args

if __name__ == '__main__':
	args = ParseCommandLine()

	textForConfidence = "#define confidence_level "
	textForPERIOD = "#define PERIOD "
	textForDixonQ = "#define dixon_n_vals "

	newPeriodText = textForPERIOD + str(args.Period)
	newDixonQ = textForDixonQ + str(args.DixonQWindow)
	newConfidence = textForConfidence + str(args.DixonConfidence)
	 
#=================== delete all old compiled code. Force fresh compilation ===
	try:
		files = os.listdir(".") #scan the current directory for all files
		for file in files:
			if file.endswith(".z1"):
				print("Deleting file: " + str(file))
				os.remove(file)
				try:
					os.remove("contiki-z1.map")
					print("removed contiki-z1.map")
				except:
					print("contiki-z1.map NOT existed")

				try:
					os.remove("symbols.c")
					print("removed symbols.c")
					os.remove("symbols.h")
					print("removed symbols.h")
				except:
					print("One of symbols.* NOT existed")

				try: 
					os.remove("contiki-z1.a")
					print("removed contiki-z1.a")
				except:
					print("contiki-z1.a NOT existed")
				try:
					shutil.rmtree("./obj_z1")
				except:
					print("./obj_z1 NOT existed")
	except:
		print("\nNo file(s) found to delete")

	print("\n---- Altering project-conf file---------\n")

#=============== OPENING FILES TO ALTER PARAMETERS ===========
	files = os.listdir(".")  # scan the current directory AGAIN for all files
	for file in files:
		if not os.path.isdir(file):  # exclude directories
			if file.endswith("project-conf.h"):
				
				print("Opening file: "+str(file))
				tempFile = open(file, 'r+')
				for line in fileinput.input(file):
					if textForConfidence in line:
						print("Added " + newConfidence)
						line = newConfidence + "\n"
					tempFile.write(line)
				tempFile.close()
				


             #else:
					#continue
