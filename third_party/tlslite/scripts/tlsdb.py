#! python

import sys
import os
import socket
import thread
import math

try:
    import cryptoIDlib
    cryptoIDlibLoaded = True
except:
    cryptoIDlibLoaded = False


if __name__ != "__main__":
    raise "This must be run as a command, not used as a module!"


from tlslite.api import *

if len(sys.argv) == 1 or (len(sys.argv)==2 and sys.argv[1].lower().endswith("help")):
    print ""
    print "Version: 0.3.8"
    print ""
    print "RNG: %s" % prngName
    print ""
    print "Modules:"
    if cryptlibpyLoaded:
        print "  cryptlib_py : Loaded"
    else:
        print "  cryptlib_py : Not Loaded"
    if m2cryptoLoaded:
        print "  M2Crypto    : Loaded"
    else:
        print "  M2Crypto    : Not Loaded"
    if pycryptoLoaded:
        print "  pycrypto    : Loaded"
    else:
        print "  pycrypto    : Not Loaded"
    if gmpyLoaded:
        print "  GMPY        : Loaded"
    else:
        print "  GMPY        : Not Loaded"
    if cryptoIDlibLoaded:
        print "  cryptoIDlib : Loaded"
    else:
        print "  cryptoIDlib : Not Loaded"
    print ""
    print "Commands:"
    print ""
    print "  createsrp       <db>"
    print "  createsharedkey <db>"
    print ""
    print "  add    <db> <user> <pass> [<bits>]"
    print "  del    <db> <user>"
    print "  check  <db> <user> [<pass>]"
    print "  list   <db>"
    sys.exit()

cmd = sys.argv[1].lower()

class Args:
    def __init__(self, argv):
        self.argv = argv
    def get(self, index):
        if len(self.argv)<=index:
            raise SyntaxError("Not enough arguments")
        return self.argv[index]
    def getLast(self, index):
        if len(self.argv)>index+1:
            raise SyntaxError("Too many arguments")
        return self.get(index)

args = Args(sys.argv)

def reformatDocString(s):
    lines = s.splitlines()
    newLines = []
    for line in lines:
        newLines.append("  " + line.strip())
    return "\n".join(newLines)

try:
    if cmd == "help":
        command = args.getLast(2).lower()
        if command == "valid":
            print ""
        else:
            print "Bad command: '%s'" % command

    elif cmd == "createsrp":
        dbName = args.get(2)

        db = VerifierDB(dbName)
        db.create()

    elif cmd == "createsharedkey":
        dbName = args.getLast(2)

        db = SharedKeyDB(dbName)
        db.create()

    elif cmd == "add":
        dbName = args.get(2)
        username = args.get(3)
        password = args.get(4)

        try:
            db = VerifierDB(dbName)
            db.open()
            if username in db:
                print "User already in database!"
                sys.exit()
            bits = int(args.getLast(5))
            N, g, salt, verifier = VerifierDB.makeVerifier(username, password, bits)
            db[username] = N, g, salt, verifier
        except ValueError:
            db = SharedKeyDB(dbName)
            db.open()
            if username in db:
                print "User already in database!"
                sys.exit()
            args.getLast(4)
            db[username] = password

    elif cmd == "del":
        dbName = args.get(2)
        username = args.getLast(3)

        try:
            db = VerifierDB(dbName)
            db.open()
        except ValueError:
            db = SharedKeyDB(dbName)
            db.open()

        del(db[username])

    elif cmd == "check":
        dbName = args.get(2)
        username = args.get(3)
        if len(sys.argv)>=5:
            password = args.getLast(4)
        else:
            password = None

        try:
            db = VerifierDB(dbName)
            db.open()
        except ValueError:
            db = SharedKeyDB(dbName)
            db.open()

        try:
            db[username]
            print "Username exists"

            if password:
                if db.check(username, password):
                    print "Password is correct"
                else:
                    print "Password is wrong"
        except KeyError:
            print "Username does not exist"
            sys.exit()

    elif cmd == "list":
        dbName = args.get(2)

        try:
            db = VerifierDB(dbName)
            db.open()
        except ValueError:
            db = SharedKeyDB(dbName)
            db.open()

        if isinstance(db, VerifierDB):
            print "Verifier Database"
            def numBits(n):
                if n==0:
                    return 0
                return int(math.floor(math.log(n, 2))+1)
            for username in db.keys():
                N, g, s, v = db[username]
                print numBits(N), username
        else:
            print "Shared Key Database"
            for username in db.keys():
                print username
    else:
        print "Bad command: '%s'" % cmd
except:
    raise
