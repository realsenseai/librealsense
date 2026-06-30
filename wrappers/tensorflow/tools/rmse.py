# -*- coding: utf-8 -*-
import cv2 as cv
import numpy as np
import sys
import math
from matplotlib import pyplot as plt
import os

filename = "D:/dataset/gt-4622.png"
if (len(sys.argv) > 1):
    filename = str(sys.argv[1])

fname, file_extension = os.path.splitext(filename)

height = 0
width = 0

i = []
if file_extension.lower() == ".raw":
    f = open(filename,"r")
    i = np.fromfile(f, dtype='uint16', sep="")
    f.close()

    size = i.shape[0]
    if (size / 1280 == 720):
        height = 720
        width = 1280
    elif (size / 848 == 480):
        height = 480
        width = 848
    elif (size / 640 == 480):
        height = 480
        width = 640
    else:
        print("Unknown dimentions")
if file_extension.lower() == ".png":
    i = cv.imread(filename, -1).astype(np.uint16)
    width = i.shape[1]
    height = i.shape[0]

m = np.percentile(i, 5)
M = np.percentile(i, 95)

i = i.reshape([height, width])

orig = i.copy()

x0 = 0
y0 = 0
x1 = 0
y1 = 0
xx = 0
yy = 0
mx = 0
Mx = 0
my = 0
My = 0

def click_and_crop(event, x, y, flags, param):
    global x0
    global y0
    global x1
    global y1
    global xx
    global yy
    global orig
    global mx, my, Mx, My

    xx = x
    yy = y
        
    if event == cv.EVENT_LBUTTONDOWN:
        return
	# check to see if the left mouse button was released
    elif event == cv.EVENT_LBUTTONUP:
        if (x0 == 0):
            x0 = x
            y0 = y
        else:
            if (x1 == 0):
                x1 = x
                y1 = y

                mx = min(x0, x1)
                Mx = max(x0, x1)
                my = min(y0, y1)
                My = max(y0, y1)
            else:
                x0 = x
                y0 = y
                x1 = 0
                y1 = 0
                mx = 0
                Mx = 0
                my = 0
                My = 0
	# record the ending (x, y) coordinates and indicate that
        return

cv.namedWindow("image")
cv.setMouseCallback("image", click_and_crop)

i = np.divide(i, np.array([M - m], dtype=np.float)).astype(np.float)
i = (i - m).astype(np.float)

i8 = (i * 255.0).astype(np.uint8)

if i8.ndim == 3:
    i8 = cv.cvtColor(i8, cv.COLOR_BGRA2GRAY)

i8 = cv.equalizeHist(i8)

colorized = cv.applyColorMap(i8, cv.COLORMAP_JET)

colorized[i8 == int(m)] = 0

font = cv.FONT_HERSHEY_COMPLEX_SMALL
colorized = cv.putText(colorized, str(m) + " .. " + str(M), (20,50), font, 1, (255, 255, 255), 2, cv.LINE_AA)

while cv.getWindowProperty('image', 0) >= 0:
    im = colorized.copy()

    if (x0 > 0 and x1 == 0):
        im = cv.rectangle(im, (x0, y0), (xx, yy), (255, 255, 255), 2)
    if (x0 > 0 and x1 > 0):
        im = cv.rectangle(im, (x0, y0), (x1, y1), (255, 255, 255), 2)

    if (mx < Mx):
        crop = orig[int(my):int(My), int(mx):int(Mx)].astype(np.float)

        X = []
        Y = []
        Z = []

        Xcrop = np.zeros_like(crop).astype(np.float)
        Ycrop = np.zeros_like(crop).astype(np.float)
        Zcrop = np.zeros_like(crop).astype(np.float)

        for i in range(my, My):
            for j in range(mx, Mx):
                z = crop[i - my, j - mx] * 0.001
                x = (float(j) / width - 0.5) * z
                y = (float(i) / height - 0.5) * z
                if (z > 0):
                    X.append(x)
                    Y.append(y)
                    Z.append(z)
                Xcrop[i - my, j - mx] = x
                Ycrop[i - my, j - mx] = y
                Zcrop[i - my, j - mx] = z

        xyz = np.dstack((X, Y, Z))
        xyz = xyz.reshape(xyz.shape[0] * xyz.shape[1], xyz.shape[2])
        C_x = np.cov(xyz.T)
        eig_vals, eig_vecs = np.linalg.eig(C_x)

        variance = np.min(eig_vals)
        min_eig_val_index = np.argmin(eig_vals)
        direction_vector = eig_vecs[:, min_eig_val_index].copy()

        rmse = math.sqrt(variance)
        #print(math.sqrt(variance) * 100)

        normal = direction_vector / np.linalg.norm(direction_vector)

        point = np.mean(xyz, axis=0)

        #print(normal)
        #print(point)

        d = -np.dot(point, normal)
        #print(d)

        a = normal[0]
        b = normal[1]
        c = normal[2]
        e = math.sqrt(a * a + b * b + c * c)

        Dcrop = np.zeros_like(crop).astype(np.float)

        for i in range(my, My):
            for j in range(mx, Mx):
                x = Xcrop[i - my, j - mx]
                y = Ycrop[i - my, j - mx]
                z = Zcrop[i - my, j - mx]
                #print(x)
                dist = abs(a * x + b * y + c * z + d) / e
                if (z > 0):
                    #print(Dcrop[i - my, j - mx])
                    #print(dist)
                    Dcrop[i - my, j - mx] = dist

        Dcrop = np.divide(Dcrop, (3 * rmse) / 255).astype(np.float)
        Dcrop = np.clip(Dcrop, 0, 255).astype(np.uint8)
        Dmap = np.dstack((Dcrop, Dcrop, Dcrop))
        im[int(my):int(My), int(mx):int(Mx)] = Dmap

        rmse_mm = rmse * 1000
        im = cv.putText(im, "%.2f mm" % rmse_mm, (mx + 2,my + 16), font, 1, (255, 255, 255), 2, cv.LINE_AA)
    
    # display the image and wait for a keypress
    cv.imshow("image", im)
    key = cv.waitKey(100) & 0xFF

cv.destroyAllWindows()
