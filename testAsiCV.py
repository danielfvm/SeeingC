#!/usr/bin/env python

# @author Daniel Schlögl
# @date   27.06.2022
#
# Useful links:
#   https://github.com/OpenPHDGuiding/phd2/blob/master/star_profile.cpp
#   https://github.com/OpenPHDGuiding/phd2/blob/master/star.cpp -> Star::Find

from IPython.display import display

import matplotlib.pyplot as plt
import zwoasi as asi
import numpy as np

import argparse
import time
import math
import sys
import os


__author__ = 'Daniel Schloegl'
__version__ = '0.1.0'
__license__ = 'MIT'


# Create figure and subplot
fig = plt.figure()

# OpenCV has to be imported after plot has been created
from cv2 import cv2


def distance(star1, star2):
    return math.sqrt(math.pow(star1[0]-star2[0], 2) + math.pow(star1[1]-star2[1], 2))

def colorToBrightness(color):
    return color[0] + color[1] + color[2]


def calcMaxRadius(stars, mainStar, maxSize):
    calcSize = maxSize

    for star in stars:

        # ignore star if it is our mainStar
        if star[0] == mainStar[0] and star[1] == mainStar[1]:
            continue

        disX = abs(mainStar[0]-star[0])
        disY = abs(mainStar[1]-star[1])
        disMin = max(disX, disY) - star[2] * 2

        if calcSize > disMin:
            calcSize = disMin

    return int(calcSize)

def calcProfile(image, mainStar, size):
    (starX, starY, _) = mainStar
    FULLW = size * 2 + 1

    horiz_profile = [0] * FULLW
    vert_profile  = [0] * FULLW

    for x in range(FULLW):
        for y in range(FULLW):
            brightness = image[starY - size + y][starX - size + x]
            horiz_profile[x] += brightness
            vert_profile[y] += brightness

    return (horiz_profile, vert_profile)

def smoothPixelValue(image, x, y):
    valCenter = image[y][x]
    valCorners = image[y-1][x-1] + image[y-1][x+1] + image[y+1][x-1] + image[y+1][x+1]
    valEdges = image[y-1][x] + image[y+1][x] + image[y][x+1] + image[y][x-1]

    return 4 * valCenter + 2 * valEdges + valCorners

def calcCenter(image, mainStar, size):
    (starX, starY, _) = mainStar

    max3 = [0] * 3
    peak_x = 0
    peak_y = 0
    peak_val = 0

    # find the peak value within the search region using a smoothing function also check for saturation
    for y in range(starY - size + 1, starY + size):
        for x in range(starX - size + 1, starX + size):
            p = image[y][x]

            val = smoothPixelValue(image, x, y)

            if val > peak_val:
                peak_val = val
                peak_x = x
                peak_y = y

            if p > max3[0]:
                max3[0], p = p, max3[0]
            if p > max3[1]:
                max3[1], p = p, max3[1]
            if p > max3[2]:
                max3[2], p = p, max3[2]

    PeakVal = max3[0]   # raw peak val
    peak_val /= 16      # smoothed peak value

    # meaure noise in the annulus with inner radius A and outer radius B
    A = 7    # inner radius
    B = 12   # outer radius
    A2 = A * A
    B2 = B * B


    # center window around peak value
    start_x = peak_x - B
    end_x = peak_x + B
    start_y = peak_y - B
    end_y = peak_y + B


    # find the mean and stdev of the background
    nbg = 0
    mean_bg = 0
    prev_mean_bg = 0
    sigma2_bg = 0
    sigma_bg = 0

    for i in range(9):
        summe = 0
        a = 0
        q = 0
        nbg = 0

        for y in range(start_y, end_y + 1):
            dy = y - peak_y
            dy2 = dy * dy

            for x in range(start_x, end_x + 1):
                dx = x - peak_x
                r2 = dx * dx + dy2

                # exclude points not in annulus
                if r2 <= A2 or r2 > B2:
                    continue

                val = image[y][x]

                if i > 0 and (val < mean_bg - 2 * sigma_bg or val > mean_bg + 2 * sigma_bg):
                    continue

                summe += val
                nbg += 1

                k = nbg
                a0 = a

                a += (val - a) / k
                q += (val - a0) * (val - a)

        if nbg < 10:
            print("too few background points!")
            break

        prev_mean_bg = mean_bg
        mean_bg = summe / nbg
        sigma2_bg = q / (nbg - 1)
        sigma_bg = math.sqrt(sigma2_bg)

        if i > 0 and abs(mean_bg - prev_mean_bg) < 0.5:
            break


    # find pixels over threshold within aperture; compute mass and centroid
    thresh = mean_bg + 3 * sigma_bg + 0.5
    cx = 0
    cy = 0
    mass = 0
    n = 0

    start_x = peak_x - A
    end_x = peak_x + A
    start_y = peak_y - A
    end_y = peak_y + A

    for y in range(start_y, end_y + 1):
        dy = y - peak_y
        dy2 = dy * dy
        if dy2 > A2:
            continue

        for x in range(start_x, end_x + 1):
            dx = x - peak_x

            # exclude points outside aperture
            if dx * dx + dy2 > A2:
                continue

            # exclude points below threshold
            val = image[y][x]

            if val < thresh:
                continue

            d = val - mean_bg

            cx += dx * d
            cy += dy * d

            mass += d
            n += 1

    if mass <= 0:
        print("Error: Star's mass is zero!")
        return (peak_x, peak_y)

    return (peak_x + cx / mass, peak_y + cy / mass)


def save_control_values(filename, settings):
    filename += '.txt'
    with open(filename, 'w') as f:
        for k in sorted(settings.keys()):
            f.write('%s: %s\n' % (k, str(settings[k])))
    print('Camera settings saved to %s' % filename)


def autoExposure(camera, controls):
    print('Enabling auto-exposure mode')
    camera.set_control_value(asi.ASI_EXPOSURE, controls['Exposure']['DefaultValue'], auto=True)

    if 'Gain' in controls and controls['Gain']['IsAutoSupported']:
        print('Enabling automatic gain setting')
        camera.set_control_value(asi.ASI_GAIN, controls['Gain']['DefaultValue'], auto=True)

    # Keep max gain to the default but allow exposure to be increased to its maximum value if necessary
    camera.set_control_value(controls['AutoExpMaxExpMS']['ControlType'], controls['AutoExpMaxExpMS']['MaxValue'])

    print('Waiting for auto-exposure to compute correct settings ...')
    sleep_interval = 0.100
    df_last = None
    gain_last = None
    exposure_last = None
    matches = 0
    while True:
        time.sleep(sleep_interval)
        settings = camera.get_control_values()
        df = camera.get_dropped_frames()
        gain = settings['Gain']
        exposure = settings['Exposure']

        if df != df_last:
            print('   Gain {gain:d}  Exposure: {exposure:f} Dropped frames: {df:d}'
                  .format(gain=settings['Gain'],
                          exposure=settings['Exposure'],
                          df=df))
            if gain == gain_last and exposure == exposure_last:
                matches += 1
            else:
                matches = 0
            if matches >= 5:
                break
            df_last = df
            gain_last = gain
            exposure_last = exposure


def runCapture(camera):
    low = np.array([130])
    high = np.array([255])

    while True:
        imgInput = camera.capture_video_frame()
        imgGray = cv2.cvtColor(imgInput, cv2.COLOR_BGR2GRAY)
        masked = cv2.inRange(imgGray, low, high)

        # detect circles in the image
        stars = cv2.HoughCircles(masked, cv2.HOUGH_GRADIENT, 1, 30, param1=30, param2=6, minRadius=0, maxRadius=40)

        # ensure at least some circles were found
        if stars is not None:
            # convert the (x, y) coordinates and radius of the circles to integers
            stars = np.round(stars[0, :]).astype("int")
            mainStar = (0, 0, 0)
            print(len(stars))

            # loop over the (x, y) coordinates and radius of the circles
            for (x, y, r) in stars:
                # Draw red circle arround estimated star position
                cv2.circle(imgInput, (x, y), r, (0, 0, 255), 1)

                if mainStar[2] < r:
                    mainStar = (x, y, r)

            if mainStar[2] > 0:
                (x, y, r) = mainStar

                # calculate scan box size, maximum is currently twice the radius of the star
                size = calcMaxRadius(stars, mainStar, r * 4)

                # Visualize scan box (green box)
                cv2.rectangle(imgInput, (x - size, y - size), (x + size, y + size), (0, 255, 0), 1)

                # Calculate star profile
                (horiz_profile, vert_profile) = calcProfile(imgGray, mainStar, size)


                # Visualize average peak (yellow point)
                if len(horiz_profile) > 0 and len(vert_profile) > 0:
                    hpa = np.array(horiz_profile)
                    vpa = np.array(vert_profile)
                    (offsetX,) = np.unravel_index(hpa.argmax(), hpa.shape)
                    (offsetY,) = np.unravel_index(vpa.argmax(), vpa.shape)
                    cv2.circle(imgInput, (x - size + offsetX, y - size + offsetY), 0, (0, 255, 255), 1)

                    # Calculate min and max value of horiz_profile and calculate fwhm
                    min_profile = np.min(horiz_profile)
                    max_profile = np.max(horiz_profile)
                    if min_profile < max_profile:
                        mid_profile = (max_profile - min_profile) / 2 + min_profile

                        # find the index of next best mid_profile value
                        x1 = 0
                        x2 = 0
                        for i in range(1, len(horiz_profile)):
                            profval_pre = horiz_profile[i-1]
                            profval = horiz_profile[i]
                            if profval_pre <= mid_profile and profval >= mid_profile:
                                x1 = i
                            elif profval_pre >= mid_profile and profval <= mid_profile:
                                x2 = i

                        if x1 != 0 and x2 != 0:
                            profval = horiz_profile[x1]
                            profval_pre = horiz_profile[x1 - 1]
                            f1 = x1 - (profval - mid_profile) / (profval - profval_pre)
                            profval = horiz_profile[x2]
                            profval_pre = horiz_profile[x2 - 1]
                            f2 = x2 - (profval_pre - mid_profile) / (profval_pre - profval)
                            fwhm = f2 - f1

                # Visualize calculated center (blue point)
                (peak_x, peak_y) = calcCenter(imgGray, mainStar, size)
                print(peak_x, peak_y)
                cv2.circle(imgInput, (int(peak_x), int(peak_y)), 0, (255, 0, 0), 1)

                # Show data in plot
                plt.cla()
                plt.plot(range(len(horiz_profile)), horiz_profile)
                plt.plot(range(len(vert_profile)), vert_profile)
                plt.legend(["Horizontal Profile", "Vertical Profile"])
                plt.title('Sternprofil')
                plt.xlabel('width/height in px')
                display(plt)
                plt.pause(0.2)


        #cv2.imshow("PreviewScan", masked)
        cv2.imshow("Preview", imgInput)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
    cv2.destroyAllWindows()


if __name__ == '__main__':
    env_filename = os.getenv('ZWO_ASI_LIB')

    parser = argparse.ArgumentParser(description='Process and save images from a camera')
    parser.add_argument('filename', nargs='?', help='SDK library filename')
    args = parser.parse_args()

    # Initialize zwoasi with the name of the SDK library
    if args.filename:
        asi.init(args.filename)
    elif env_filename:
        asi.init(env_filename)
    else:
        print('The filename of the SDK library is required (or set ZWO_ASI_LIB environment variable with the filename)')
        sys.exit(1)

    num_cameras = asi.get_num_cameras()
    if num_cameras == 0:
        print('No cameras found')
        sys.exit(0)

    cameras_found = asi.list_cameras()  # Models names of the connected cameras
    camera_id = 0
    camera = asi.Camera(camera_id)
    camera_info = camera.get_camera_property()

    print('Using camera: %s' % cameras_found[camera_id])

    # Get all of the camera controls
    controls = camera.get_controls()

    # Use maximum USB bandwidth permitted
    camera.set_control_value(asi.ASI_BANDWIDTHOVERLOAD, camera.get_controls()['BandWidth']['MaxValue'])

    # Set some sensible defaults. They will need adjusting depending upon
    # the sensitivity, lens and lighting conditions used.
    camera.disable_dark_subtract()

    camera.set_control_value(asi.ASI_GAIN, 150)
    camera.set_control_value(asi.ASI_EXPOSURE, 30000)
    camera.set_control_value(asi.ASI_WB_B, 99)
    camera.set_control_value(asi.ASI_WB_R, 75)
    camera.set_control_value(asi.ASI_GAMMA, 50)
    camera.set_control_value(asi.ASI_BRIGHTNESS, 50)
    camera.set_control_value(asi.ASI_FLIP, 0)

    camera.set_roi_format(1944, 1224, 1, asi.ASI_IMG_RGB24)

    print('Enabling stills mode')
    try:
        # Force any single exposure to be halted
        camera.stop_video_capture()
        camera.stop_exposure()
    except (KeyboardInterrupt, SystemExit):
        raise
    except:
        pass

    print('Enabling video mode')
    camera.start_video_capture()

    # Restore all controls to default values except USB bandwidth
    for c in controls:
        if controls[c]['ControlType'] == asi.ASI_BANDWIDTHOVERLOAD:
            continue
        camera.set_control_value(controls[c]['ControlType'], controls[c]['DefaultValue'])

    # Can autoexposure be used?
    k = 'Exposure'
    if 'Exposure' in controls and controls['Exposure']['IsAutoSupported']:
        autoExposure(camera, controls)
        pass

    # Set the timeout, units are ms
    timeout = (camera.get_control_value(asi.ASI_EXPOSURE)[0] / 1000) * 2 + 500 + 10000
    camera.default_timeout = timeout

    runCapture(camera)
    camera.capture_video_frame()
