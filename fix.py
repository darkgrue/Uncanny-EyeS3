c=open('include/default_eye_480.h').read()
c=c.replace('#include ". ./eyes.h",'#include "eyes.h")
open('include/default_eye_480.h','w').write(c)