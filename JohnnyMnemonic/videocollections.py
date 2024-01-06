import MainReactor
import numpy as np
import sys
import cv2


from datetime import datetime

now = datetime.now()

current_time = now.strftime("%M-%H-%d-%m-%y")
print(current_time)

fourcc = cv2.VideoWriter_fourcc(*'XVID')
out = cv2.VideoWriter(current_time + ".output.avi", fourcc, 20.0, (640,  480))

mat_name = "RS_COLOR_0"
if len(sys.argv) == 2:
  mat_name = sys.argv[1]

sharedMat = MainReactor.SharedMat(mat_name)
while True:
    sharedMat.waitForFrame()
    frame = np.array(sharedMat.mat, copy=False)
    cv2.imshow("frame", frame)
    out.write(frame)
  
    if cv2.waitKey(1) == 27:    
        break    
  
# Release everything when done

out.release()

    
  
