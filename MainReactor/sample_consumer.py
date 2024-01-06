import MainReactor
import numpy as np
import sys
import cv2
import time

mat_name = "COLOR_0"
if len(sys.argv) == 2:
  mat_name = sys.argv[1]

prevFrameTime = 0
newFrameTime = 0
font = cv2.FONT_HERSHEY_SIMPLEX
sharedMat = MainReactor.SharedMat(mat_name)
while True:
    sharedMat.waitForFrame()
    frame = np.array(sharedMat.mat, copy=False)

    newFrameTime = time.time()
    fps = 1/(newFrameTime-prevFrameTime)
    prevFrameTime = newFrameTime
    fps = str(int(fps))
    cv2.putText(frame, fps, (7, 70), font, 3, (100, 255, 0), 3, cv2.LINE_AA)

    cv2.imshow("frame", frame)

    if cv2.waitKey(1) == 27:
        break
