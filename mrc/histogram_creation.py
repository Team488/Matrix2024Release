import numpy as np
import cv2 as cv
import argparse
import os
import math
#from matplotlib import pyplot as plt


parser = argparse.ArgumentParser(description='image capture2')
parser.add_argument('--folder', help='uses images from indicated folder(input as path)', default='./example_imgs/')
parser.add_argument('--isBall', help='if images you are using are balls, say --isBall true so that the images arent cropped as circles', default=False)

parser.add_argument('--histName', help='required name to save histogram as', default='object')
parser.add_argument('--showHistogram', default=False)
args = parser.parse_args()

print(args.folder)

if args.histName == None:
    print('must specify file name to save the histogram to')
    exit()

def isWithinCircle(x, y, rows, columns):
    #     row#, column#, total rows, total columns
    #if you are having trouble, make sure x and y are lining up with columns and rows
    #row should = x, column should = y
    a = rows/2
    b= columns/2
    h = a
    k = b
    return ((((x-h)/a)**2 + ((y-k)/b)**2 ) <= 1)


try:
    os.mkdir('results')
except: 
    # file already exists
    pass

#create empty 3d histogram
histogram = None
#open folder of cropped images
#loop through the folder and for each image
for image in os.listdir(args.folder):
    #read, cvt to LAB, and split images
    image = cv.imread(os.path.join(args.folder, image))
    image = image.astype('uint8')
    lab = cv.cvtColor(image, cv.COLOR_BGR2LAB)
    #image values of lab are read in between 0-255 by the way
    splitChannels = cv.split(lab)
    #create an oval mask
    mask = None
    if args.isBall:
        mask = np.zeros(image.shape[:2], dtype=np.uint8)
        for i in range(mask.shape[0]):
            for j in range(mask.shape[1]):
                if isWithinCircle(i, j, mask.shape[0], mask.shape[1]):
                    mask[i][j] = 255
    
    #acumulate the values to the histogram
    localHist = cv.calcHist(splitChannels, [1, 2], mask, [40, 40], [0, 255, 0, 255])
    histogram = localHist if histogram is None else cv.add(histogram, localHist)

#normalize

cv.normalize(histogram, histogram, 0, 255, norm_type = cv.NORM_MINMAX)
#histogram *= 255
#histogram = histogram.astype('uint8')
np.save(args.histName, histogram)
print('saved your histogram as ' + args.histName + '.npy')

if args.showHistogram:
    showHist = cv.resize(histogram, (200, 200), interpolation = cv.INTER_NEAREST)
    cv.imshow('ABHist', showHist)
    cv.waitKey(0)

# ABhist = np.sum(histogram, axis = 0, dtype = np.uint8)
# cv.normalize(ABhist, ABhist, 0, 255, norm_type = cv.NORM_MINMAX)
# ABhist = cv.resize(ABhist, (200, 200), interpolation = cv.INTER_NEAREST)
# cv.imshow('A-B histogram', ABhist)
# cv.waitKey(0)

image_name = "image"
counter = 0
for image in os.listdir(args.folder):
    counter += 1
    file = image_name + str(counter) + ".jpg"
    #read, cvt to LAB, and split images
    image = cv.imread(os.path.join(args.folder, image))
    ball = cv.cvtColor(image, cv.COLOR_BGR2LAB)
    backproj = cv.calcBackProject([ball], [1, 2], histogram, [0, 256, 0, 256], scale=1)
    
    # apply threshold to the object:
    result, ret_threshold = cv.threshold(backproj, 35, 255, cv.THRESH_BINARY)
    ret_threshold = cv.bitwise_not(ret_threshold) # invert thresh image so object is white and background is black
    
    # morphological to 
    dts_mat = np.empty([5, 5]) # arbitrary array to store the matrix of morph
    kernel = np.ones((5,5),np.uint8)
    final = cv.morphologyEx(ret_threshold, cv.MORPH_OPEN, kernel, dts_mat, (-1, -1), 4)
    
    cv.imwrite(os.path.join('results', file), final)