import os
for f in [resources/eyes/default_eye/default_eye_466.eye,resources/eyes/default_eye/default_eye_480.eye,resources/eyes/eagle/eagle_466.eye,resources/eyes/eagle/eagle_480.eye,resources/eyes/human_eye/human_eye_466.eye,resources/eyes/human_eye/human_eye_480.eye]: c=open(f).read(); c=c.replace(chr(10)+ \iSpin\: 0,+chr(10),chr(10)); open(f,w).write(c); print(f)
