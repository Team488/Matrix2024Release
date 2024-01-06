

import shared_mat as sm
import numpy as np
import sys
import cv2
import argparse
import os

parser = argparse.ArgumentParser(description='image capture2')
parser.add_argument('--imageNames', help='Images will be stored as \'imageName\'(number).jpg', default='capture ')
parser.add_argument('--folder', help='saves images captured to this folder', default='captures')
parser.add_argument('--matName', help='from main reactor', default='COLOR_0')
args = parser.parse_args()


print('c to captrue image')
print('s to save cropped image')


#folder manage
if not os.path.exists(os.getcwd() + '/'+ args.folder):
    os.makedirs(os.getcwd() + '/'+ args.folder)
os.chdir(os.getcwd() + '/'+ args.folder)

mat_name = args.matName

#get images from main reactor
sharedMat = sm.SharedMat(mat_name)

#initialize
numPictures = 1
cropping = False
x_start, y_start, x_end, y_end = 0, 0, 0, 0
frame = None
frame_to_crop = None
cropped_frame = None
mouse_x = 0
mouse_y= 0

def mouse_crop(event, x, y, flags, param):
  # grab references to the global variables
  global x_start, y_start, x_end, y_end, cropping, frame, cropped_frame, mouse_x, mouse_y
  mouse_x = x
  mouse_y = y
  # if the left mouse button was DOWN, start RECORDING
  # (x, y) coordinates and indicate that cropping is being
  if event == cv2.EVENT_LBUTTONDOWN:
    x_start, y_start, x_end, y_end = x, y, x, y
    cropping = True

  # Mouse is Moving
  elif event == cv2.EVENT_MOUSEMOVE:
    if cropping == True:
      x_end, y_end = x, y

  # if the left mouse button was released
  elif event == cv2.EVENT_LBUTTONUP:
    # record the ending (x, y) coordinates
    x_end, y_end = x, y
    cropping = False # cropping is finished

    refPoint = [(x_start, y_start), (x_end, y_end)]
    area_of_rectange = (x_end - x_start) * (y_end - y_start)

    if len(refPoint) == 2 and area_of_rectange > 16: #when two points were found
      cropped_frame = frame_to_crop[refPoint[0][1]:refPoint[1][1], refPoint[0][0]:refPoint[1][0]]
      # do something with roi
      #cv2.destroyWindow("crop window")


cv2.namedWindow("crop window")
cv2.setMouseCallback("crop window", mouse_crop)



#fencepost
sharedMat.waitForFrame()
frame = np.array(sharedMat.mat, copy=False)
cv2.imshow('frame', frame)

while cv2.getWindowProperty('frame', cv2.WND_PROP_VISIBLE) >= 1:
  sharedMat.waitForFrame()
  frame = np.array(sharedMat.mat, copy=False)
  cv2.imshow('frame', frame)
  Key = cv2.waitKey(1)

  if Key & 0xFF == ord('c'):
    frame_to_crop = frame.copy()
    cv2.imshow('crop window', frame_to_crop)
    while cv2.getWindowProperty('crop window', cv2.WND_PROP_VISIBLE) >= 1:
      frame_to_show = frame_to_crop.copy()
      if not cropping:
        height=frame_to_show.shape[0]
        width = frame_to_show.shape[1]
        cv2.line(frame_to_show, (0,mouse_y),(width, mouse_y), (0, 0, 255), 1)
        cv2.line(frame_to_show, (mouse_x,0),(mouse_x, height), (0, 0, 255), 1)
        cv2.imshow("crop window", frame_to_show)

      elif cropping:
        cv2.rectangle(frame_to_show, (x_start, y_start), (x_end, y_end), (0, 0, 255), 1)
        cv2.imshow("crop window", frame_to_show)
      
      if cropped_frame is not None:
        cv2.imshow("cropped image", cropped_frame)
        while cv2.getWindowProperty('cropped image', cv2.WND_PROP_VISIBLE) >= 1:
          key = cv2.waitKey(1)
          if key & 0xFF == ord('s'):
            cv2.imwrite(args.imageNames+str(numPictures)+'.jpg', cropped_frame)
            numPictures+=1
            print('saved a picture!')
            cropped_frame = None
            cv2.destroyWindow("cropped image")
        cropped_frame = None


      keyCode = cv2.waitKey(1)

cv2.destroyAllWindows()
