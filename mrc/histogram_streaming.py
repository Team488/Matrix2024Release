import cv2 as cv
import numpy as np
import argparse
import os
from math import atan2, cos, sin, sqrt, pi, radians
import MainReactor
from threading import Thread
import json
import ntcore as nt
import time


class ObjectDetector():
    def __init__(self, color_mat_name, depth_mat_name, histogram, calibration_file):
        self.detector = self.createBlobDetector()
        self.histogram = histogram
        self.sharedColorMat = MainReactor.SharedMat(color_mat_name)
        self.sharedDepthMat = MainReactor.SharedMat(depth_mat_name)
        self.mtx, self.dist, fx, self.fy, self.cx, self.cy = self.retrieveCameraCalibration(calibration_file)
        
        self.frame = np.array(self.sharedColorMat.mat, copy=False)
        frame_height, frame_width = self.frame.shape[:2]
        self.new_camera_matrix, self.roi = cv.getOptimalNewCameraMatrix(self.mtx, self.dist, (frame_width, frame_height), 1, (frame_width, frame_height))

        
        # Timestamps
        self.currentTime = 0.0
        self.previousTime = 0.0
        self.newFrameTime = 0
        self.prevFrameTime = 0
        self.frameList = []
        
        # Network Table
        self.networktable = NetworkTable()
        
    def drawAxis(self, img, p_, q_, colour, scale):
        p = list(p_)
        q = list(q_)
        
        angle = atan2(p[1] - q[1], p[0] - q[0]) # angle in radians
        hypotenuse = sqrt((p[1] - q[1]) * (p[1] - q[1]) + (p[0] - q[0]) * (p[0] - q[0]))
        # Here we lengthen the arrow by a factor of scale
        q[0] = p[0] - scale * hypotenuse * cos(angle)
        q[1] = p[1] - scale * hypotenuse * sin(angle)
        cv.line(img, (int(p[0]), int(p[1])), (int(q[0]), int(q[1])), colour, 1, cv.LINE_AA)
        # create the arrow hooks
        p[0] = q[0] + 9 * cos(angle + pi / 4)
        p[1] = q[1] + 9 * sin(angle + pi / 4)
        cv.line(img, (int(p[0]), int(p[1])), (int(q[0]), int(q[1])), colour, 1, cv.LINE_AA)
        p[0] = q[0] + 9 * cos(angle - pi / 4)
        p[1] = q[1] + 9 * sin(angle - pi / 4)
        cv.line(img, (int(p[0]), int(p[1])), (int(q[0]), int(q[1])), colour, 1, cv.LINE_AA)

    def getOrientation(self, pts, img):
        sz = len(pts)
        data_pts = np.empty((sz, 2), dtype=np.float64)
        for i in range(data_pts.shape[0]):
            data_pts[i,0] = pts[i,0,0]
            data_pts[i,1] = pts[i,0,1]
        # Perform PCA analysis
        mean = np.empty((0))
        mean, eigenvectors, eigenvalues = cv.PCACompute2(data_pts, mean)
        # Store the center of the object
        cntr = (int(mean[0,0]), int(mean[0,1]))
        
        
        cv.circle(img, cntr, 3, (255, 0, 255), 2)
        p1 = (cntr[0] + 0.02 * eigenvectors[0,0] * eigenvalues[0,0], cntr[1] + 0.02 * eigenvectors[0,1] * eigenvalues[0,0])
        p2 = (cntr[0] - 0.02 * eigenvectors[1,0] * eigenvalues[1,0], cntr[1] - 0.02 * eigenvectors[1,1] * eigenvalues[1,0])
        self.drawAxis(img, cntr, p1, (0, 255, 0), 1)
        self.drawAxis(img, cntr, p2, (255, 255, 0), 5)
        angle = atan2(eigenvectors[0,1], eigenvectors[0,0]) # orientation in radians
        
        return angle

    def createBlobDetector(self):
        params = cv.SimpleBlobDetector_Params()

        params.minDistBetweenBlobs = 20

        # We want a low threshold cause our backprojected image has white background and black object
        params.minThreshold = 0
        params.maxThreshold = 10
        params.thresholdStep = 10
        params.minRepeatability = 1

        # Higher = Lighter Color
        params.filterByColor = True
        params.blobColor = 0 # 0 because our object is black
        
        # Area as in pixel area
        params.filterByArea = True
        params.minArea = 1000 # subject to change. 200 for now.
        params.maxArea = 240000

        # Higher = More Circularity, Lower = Triangle-like
        params.filterByCircularity = False
        # params.minCircularity = 0.1 
        # params.maxCircularity = 0.5 

        # Filter by Inertia
        params.filterByInertia = False
        # params.minInertiaRatio = 0.01
        # params.maxInertiaRatio = 0.80

        params.filterByConvexity = False

        detector = cv.SimpleBlobDetector_create(params)
        return detector

    # Load in the camera calibraiton json file and convert to np.array
    def retrieveCameraCalibration(self, calibration_file):
        with open(calibration_file) as settings_file:
            jsonData = json.load(settings_file)
            json_mtx = jsonData['CameraMatrix']
            json_dist = jsonData['DistortionCoeff']
            mtx = np.array(json.loads(json_mtx))
            dist = np.array(json.loads(json_dist))
            fx = mtx[0][0]
            fy = mtx[1][1]
            cx = mtx[0][2]
            cy = mtx[1][2]
            return (mtx, dist, fx, fy, cx, cy)
    
    def calculateBearing(self, img, cx, centroid_x):
        horizontal_fov = 69 # degrees
        horizontal_fov_ratio = horizontal_fov/img.shape[1]
        if (centroid_x > cx):
            # positive angle
            diff = centroid_x - cx
            bearing = diff * horizontal_fov_ratio
        else:
            # negative angle
            diff = cx - centroid_x
            bearing = -1 * diff * horizontal_fov_ratio
        
        return bearing

    def calculateAngle(self, bearing):
        return 90 - bearing

    def calculateHypotenuse(self, d, bearing):
        return d/sin(radians(90 - bearing))
    

        
    def detectObjects(self, color_frame, depth_frame):
        # Undistort Camera using Calibration Matrix
        # dst = cv.undistort(color_frame, mtx, dist, None, new_camera_matrix)
        # x, y, w, h = roi
        # color_frame = dst[y:y+h, x:x+w]
        
        # cv.imshow('calibrated', frame)
        
        lab = cv.cvtColor(color_frame, cv.COLOR_BGR2LAB)
        backproj = cv.calcBackProject([lab], [1, 2], self.histogram, [0, 256, 0, 256], scale=1)
        
        # Apply threshold to the object:
        result, ret_threshold = cv.threshold(backproj, 35, 255, cv.THRESH_BINARY)
        ret_threshold = cv.bitwise_not(ret_threshold)
        
        # Apply morphological transformation to the image to remove noise
        kernel = np.ones((5,5),np.uint8)
        final = cv.morphologyEx(ret_threshold, cv.MORPH_ELLIPSE, kernel, None, (-1, -1), 4)
        
        # Blob Detection:
        keypoints = self.detector.detect(final)
 
        im_with_keypoints = cv.drawKeypoints(color_frame, keypoints, np.array([]), (0,0,255), cv.DRAW_MATCHES_FLAGS_DRAW_RICH_KEYPOINTS)
        
        contours, _ = cv.findContours(final, cv.RETR_LIST, cv.CHAIN_APPROX_NONE)
        cv.drawContours(color_frame, contours, 0, (0, 0, 255), 2)
        for i, c in enumerate(contours):
            # Calculate area of each contour
            area = cv.contourArea(c)
            if area < 1e3 or 1e5 < area:
                continue
            # Draw each contour
            cv.drawContours(color_frame, contours, i, (0, 0, 255), 2)
            # self.getOrientation(c, color_frame)
        
        if len(keypoints) > 0:
            closestObjectRange = float('inf')
            closestObjectAngle = None
            for points in keypoints:
                x = int(points.pt[0])
                y = int(points.pt[1])
                dot_size = int(points.size / 20)
                depth_value = depth_frame[y][x]
                if (depth_value < 250):
                    depth_value = 0

                bearing = self.calculateBearing(color_frame, self.cx, x)
                h = self.calculateHypotenuse(depth_value, abs(bearing))
                
                cv.circle(im_with_keypoints, (x, y), dot_size, (255, 255, 255), -1)
                # Adds distance and angle as text
                cv.putText(im_with_keypoints, str(round(bearing,2)) + "deg " + str(int(depth_value)) + 'mm', (x, y), cv.FONT_HERSHEY_SIMPLEX, 1, 
                        (0, 0, 255), 2, cv.LINE_AA, False)
                
                if closestObjectRange > h:
                    closestObjectRange = h
                    closestObjectAngle = bearing
            
            # Network tables
            nt_time = nt._now()
            self.networktable.getEntry("angle").setDouble(closestObjectAngle, nt_time)
            self.networktable.getEntry("range").setDouble(closestObjectRange, nt_time)

        self.networktable.getEntry("found").setBoolean(True if len(keypoints) > 0 else False)
        self.networktable.getEntry("num_of_target").setInteger(len(keypoints))
        
        # cv.imshow("contours", color_frame)
        # cv.imshow("final", final)
        
        # Add framerate:
        self.newFrameTime = time.time()
        fps = 1/(self.newFrameTime-self.prevFrameTime)
        self.prevFrameTime = self.newFrameTime
        if fps < 20:
            print(fps)                     
        fps = str(int(fps))
        font = cv.FONT_HERSHEY_SIMPLEX
        cv.putText(im_with_keypoints, fps, (7, 70), font, 3, (100, 255, 0), 3, cv.LINE_AA)   
        
        cv.imshow("centroids", im_with_keypoints)


class FrameReader():
    
    def __init__(self, sharedColorMat, sharedDepthMat, frame_callback):
        self.sharedColorMat = sharedColorMat
        self.sharedDepthMat = sharedDepthMat
        self.frame_callback = frame_callback
        self.thread = Thread(target=self.readFrames)

    def readFrames(self):
        while True:
            # Process MainReactor mats.
            self.sharedDepthMat.waitForFrame()
            depth_frame = np.array(self.sharedDepthMat.mat, copy=False)
            # minMaxLoc = cv.minMaxLoc(depth_frame)
            # minimum = minMaxLoc[0]
            # maximum = minMaxLoc[1]
            # if maximum > 10000:
            #     maximum = 1            
            # depth_compressed_frame = depth_frame.astype(float)
            # depth_compressed_frame *= 255.0/(maximum - minimum)
            # depth_compressed_frame = depth_compressed_frame.astype(np.uint8)
            self.sharedColorMat.waitForFrame()
            color_frame = np.array(self.sharedColorMat.mat, copy=False)


            self.frame_callback(color_frame, depth_frame)

            if cv.waitKey(1) == ord('q'):
                break
        cv.destroyAllWindows()
        exit()

    def start(self):
        self.thread.start()

    def join(self):
        self.thread.join()
        
class NetworkTable():
    
    def __init__(self):
        instance = nt.NetworkTableInstance.getDefault()
        instance.startClient4("example client")
        instance.setServerTeam(488)        
        self.table = instance.getTable("SmartDashboard")
        
        self.writeTable = self.table.getSubTable("BlackMesa")
        
        self.entries = dict()
        self.createEntry(self.writeTable, "range", "range_in_milimeters")
        self.createEntry(self.writeTable, "angle", "angle_in_degrees")
        self.createEntry(self.writeTable, "found", "targets_found")
        self.createEntry(self.writeTable, "num_of_target", "numbers_of_targets")
        self.createEntry(self.table.getSubTable("Field"), "position", "Robot")
        
        
    # gets the X, Y, heading of the Robot from Network Tables as a tuple
    def getRobotPosition(self):
        position = self.entries["position"].getDoubleArray("NaN")
        if position != "NaN":
            x, y, heading = position
            return (x, y, heading)
        else:
            return None
    
    def createEntry(self, table, dictName, entryName):
        self.entries[dictName] = table.getEntry(entryName) 
        
        
    def getEntry(self, entryName):
        return self.entries[entryName]
    

def main():
    color_mat_name="RS_COLOR_0"
    depth_mat_name="RS_DEPTH_0"
    histogram = np.load('awbcones.npy')
    objectDetector = ObjectDetector(color_mat_name, depth_mat_name, histogram, calibration_file='camera_calib.json')

    frame_callback=lambda color_frame, depth_frame: objectDetector.detectObjects(color_frame, depth_frame)
    frameReader = FrameReader(objectDetector.sharedColorMat, objectDetector.sharedDepthMat, frame_callback)
    frameReader.start()
    frameReader.join()

if __name__ == '__main__':
    main()