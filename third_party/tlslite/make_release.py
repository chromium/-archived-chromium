
#When run on (my) windows box, this builds and cleans everything in
#preparation for a release.

import os
import sys

#Replace version strings
if len(sys.argv)>1:
    oldVersion = sys.argv[1]
    newVersion = sys.argv[2]
    query = raw_input("Replace %s with %s?: " % (oldVersion, newVersion))
    if query == "y":
        #First, scan through and make sure the replacement is possible
        for filename in ("setup.py", "tlslite\\__init__.py", "scripts\\tls.py", "scripts\\tlsdb.py"):
            s = open(filename, "rU").read()
            x = s.count(oldVersion)
            if filename.endswith("__init__.py"):
                if x != 2:
                    print "Error, old version appears in %s %s times" % (filename, x)
                    sys.exit()
            else:
                if x != 1:
                    print "Error, old version appears in %s %s times" % (filename, x)
                    sys.exit()

        #Then perform it
        for filename in ("setup.py", "tlslite\\__init__.py", "scripts\\tls.py", "scripts\\tlsdb.py"):
            os.system("copy %s .." % filename) #save a backup copy in case something goes awry
            s = open(filename, "r").read()
            f = open(filename, "w")
            f.write(s.replace(oldVersion, newVersion))
            f.close()


#Make windows installers
os.system("del installers\*.exe")

#Python 2.3
os.system("rmdir build /s /q")
os.system("python23 setup.py bdist_wininst -o")
os.system("copy dist\* installers")

#Python 2.4
os.system("rmdir build /s /q")
os.system("python24 setup.py bdist_wininst -o")
os.system("copy dist\* installers")


#Make documentation
os.system("python23 c:\\devtools\\python23\\scripts\\epydoc.py --html -o docs tlslite")

#Delete excess files
os.system("del tlslite\\*.pyc")
os.system("del tlslite\\utils\\*.pyc")
os.system("del tlslite\\integration\\*.pyc")
os.system("rmdir build /s /q")
os.system("rmdir dist /s /q")



