c=open('include/default_eye_480.h').read()
c=c.replace("../eyes.h","eyes.h")
open('include/default_eye_480.h','w').write(c)