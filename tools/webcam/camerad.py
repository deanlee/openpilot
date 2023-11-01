import cv2
cap = cv2.VideoCapture(0)

ROAD_CAM_ID 
while True:
  (grabbed, frame) = cap.read()
  showimg = frame
  cv2.imshow('img1', showimg)  # display the captured image
  cv2.waitKey(1)