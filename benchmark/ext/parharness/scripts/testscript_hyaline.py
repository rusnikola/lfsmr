#!/usr/bin/python


from os.path import dirname, realpath, sep, pardir
import sys
import os

# path loading ----------
#print dirname(realpath(__file__))
os.environ['PATH'] = dirname(realpath(__file__))+\
":" + os.environ['PATH'] # scripts
os.environ['PATH'] = dirname(realpath(__file__))+\
"/..:" + os.environ['PATH'] # bin
os.environ['PATH'] = dirname(realpath(__file__))+\
"/../../../bin:" + os.environ['PATH'] # metacmd

#"/../../cpp_harness:" + os.environ['PATH'] # metacmd

# execution ----------------
for i in range(0,5):
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -r 1"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/hashmap_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -r 2"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/list_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -r 3"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/bonsai_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -r 4"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/natarajan_result.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -c -r 1"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/hashmap_result_retired.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -c -r 2"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/list_result_retired.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -c -r 3"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/bonsai_result_retired.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 3 -v -c -r 4"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/natarajan_result_retired.csv"
	os.system(cmd)

for i in range(0,5):
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -r 1"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/hashmap_result_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -r 2"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/list_result_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -r 3"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/bonsai_result_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -r 4"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/natarajan_result_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -c -r 1"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/hashmap_result_retired_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -c -r 2"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/list_result_retired_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -c -r 3"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/bonsai_result_retired_read.csv"
	os.system(cmd)
	cmd = "metacmd.py main -i 10 -demptyf=120 -m 2 -v -c -r 4"+\
	" --meta t:1:9:18:27:36:45:54:63:72:81:90:99:108:117:126:135:144"+\
	" --meta d:tracker=NIL:tracker=RCU:tracker=Range_new:tracker=HE:tracker=Hazard:tracker=HyalineEL:tracker=HyalineOEL:tracker=HyalineSEL:tracker=HyalineOSEL"+\
	" -o data/final/natarajan_result_retired_read.csv"
	os.system(cmd)
