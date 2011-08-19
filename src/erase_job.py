#!/usr/bin/env python
import telnetlib
import sys

tn = telnetlib.Telnet('localhost', 2120)
tn.write('ERSE\t%s'% sys.argv[1])
print tn.read_all()



