olddll = open("cygwin1.dll.original", "rb")
str = olddll.read()
olddll.close()

str = str.replace("mounts v2", "mounts v0")
str = str.replace("mount registry: 2", "mount registry: 0")
str = str.replace("cygwin1S4", "cygwin0S4")

newdll = open("cygwin1.dll", "wb")
newdll.write(str)
newdll.close()
