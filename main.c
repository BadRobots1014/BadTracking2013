#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include "network.h"
#include "rectangle.h"

#define MIN_AREA 100
#define DEBUG

#define TARGET_AREA 9001
#define TARGET_ASPECT_RATIO 54.0/21.0
#define PERROR .45

enum color_channels { CHANNEL_BLUE, CHANNEL_GREEN, CHANNEL_RED };
enum track_target { TARGET_RED, TARGET_GREEN, TARGET_BLUE, TARGET_WHITE, TARGET_BLACK };

IplImage* splitChannel(IplImage* input, int channel);
rectangle_t* track(IplImage* image, int target, int* numRects);

int main(int argc, char* argv[]) {
	char* stream_url = "rtsp://10.10.14.11:554/axis-media/media.amp?videocodec=h264&streamprofile=Bandwidth";
	printf("Connecting to dashboard...\n");
	int dashboard = socket_open("10.10.14.42", "2000");
	if(dashboard < 0) {
		return -1;
	}
	printf("Connected!\n");

	avformat_network_init();
	CvCapture* capture = cvCaptureFromFile(stream_url);
	if(!capture) {
		printf("No capture!\n");
		return;
	}
	int lostFrames = 0;
	int receivedFrames = 0;
	int startTime = time(0);
	int endTime = startTime;
	IplImage* image;
	CvMemStorage* storage = cvCreateMemStorage(0);
	while(1) {
		image = cvQueryFrame(capture);
		
		if(!image) {
			printf("Retrieved %i frames before losing one!\n", receivedFrames);
			receivedFrames = 0;
			printf("No Frame!\n");
			lostFrames++;
			if(lostFrames >= 3) {
				socket_write_float(dashboard, 0);
				socket_write_float(dashboard, 0);
				socket_write_float(dashboard, -1); //Tell the robot to stop, need to grab our image stream again			
				cvReleaseCapture(&capture);
				capture = cvCaptureFromFile(stream_url);
				lostFrames = 0;
				endTime = time(0);
				printf("Waited %i seconds for new frame\n", (endTime-startTime));
			}		
			continue;
		} else {
			startTime = time(0);
		}
		receivedFrames++;

		int numRects = 0;
		rectangle_t* rectangles = track(image, TARGET_RED, &numRects);

		float targetX = 0;
		float targetY = 0;

		CvSize imageSize = cvGetSize(image);

		float bestError = -1;

		int i;
		for(i = 0; i < numRects; i++) {
			rectangles[i] = normalize_bounds(rectangles[i], imageSize);
			float error = ((rectangles[i].width/rectangles[i].height)-TARGET_ASPECT_RATIO)/TARGET_ASPECT_RATIO;
			if(error < 0) error = -error;
			if(error <= PERROR && error < bestError) {
				targetX = rectangles[i].x+(rectangles[i].width/2);
				targetY = rectangles[i].y+(rectangles[i].height/2);	
				bestError = error;		
			}
		}
		endTime = time(0);
		int duration = endTime-startTime;
		float timeSince = (float)duration; //Time since last valid frame (seconds)

		socket_write_float(dashboard, targetX);
		socket_write_float(dashboard, targetY);
		if(bestError = -1) {
			/*
				Unable to find a target, make sure the robot knows this information
				isn't valid
			*/
			socket_write_float(dashboard, -1);
		} else {
			socket_write_float(dashboard, timeSince);
		}		
		free(rectangles);

		char key = cvWaitKey(1);
		if(key == 'q') {
			break;
		}
		cvClearMemStorage(storage);
	}
	socket_release(dashboard);
	cvReleaseMemStorage(&storage);
	cvReleaseCapture(&capture);	
	return 0;
}

IplImage* splitChannel(IplImage* input, int channel) {
	IplImage* result = cvCreateImage(cvGetSize(input), 8, 1);
	
	IplImage* redChannel = cvCreateImage(cvGetSize(input), 8, 1);
	IplImage* greenChannel = cvCreateImage(cvGetSize(input), 8, 1);
	IplImage* blueChannel = cvCreateImage(cvGetSize(input), 8, 1);
	cvSplit(input, blueChannel, greenChannel, redChannel, NULL);	
	if(channel == CHANNEL_BLUE) {
		cvAdd(redChannel, greenChannel, greenChannel, NULL);
		cvSub(blueChannel, greenChannel, result, NULL);
	} else if(channel == CHANNEL_GREEN) {
		cvAdd(redChannel, blueChannel, blueChannel, NULL);
		cvSub(greenChannel, blueChannel, result, NULL);
	} else {
		cvAdd(blueChannel, greenChannel, greenChannel, NULL);
		cvSub(redChannel, greenChannel, result, NULL);
	}

	cvReleaseImage(&redChannel);
	cvReleaseImage(&greenChannel);
	cvReleaseImage(&blueChannel);
	return result;
}

rectangle_t* track(IplImage* image, int target, int* numRects) {
	CvMemStorage* storage = cvCreateMemStorage(0);

	IplImage* threshold = cvCreateImage(cvGetSize(image), 8, 1);

	CvSeq* contours;
	if(target == TARGET_BLUE) {
		IplImage* blueImage = splitChannel(image, CHANNEL_BLUE);
		cvThreshold(blueImage, threshold, 75, 255, CV_THRESH_OTSU);

		cvReleaseImage(&blueImage);
	} else if(target == TARGET_GREEN) {
		IplImage* greenImage = splitChannel(image, CHANNEL_GREEN);
		cvThreshold(greenImage, threshold, 75, 255, CV_THRESH_OTSU);

		cvReleaseImage(&greenImage);
	} else if(target == TARGET_RED) {
		IplImage* redImage = splitChannel(image, CHANNEL_RED);
		cvThreshold(redImage, threshold, 75, 255, CV_THRESH_OTSU);

		cvReleaseImage(&redImage);
	} else if(target == TARGET_WHITE) {
		IplImage* grayScale = cvCreateImage(cvGetSize(image), 8, 1);
		cvCvtColor(image, grayScale, CV_RGB2GRAY);
		cvThreshold(grayScale, threshold, 175, 255, CV_THRESH_BINARY);

		cvReleaseImage(&grayScale);
	} else if(target == TARGET_BLACK) {
		IplImage* grayScale = cvCreateImage(cvGetSize(image), 8, 1);
		cvCvtColor(image, grayScale, CV_RGB2GRAY);
		cvThreshold(grayScale, threshold, 0, 55, CV_THRESH_BINARY);

		cvReleaseImage(&grayScale);
	}
	cvShowImage("threshold", threshold);
	cvFindContours(threshold, storage, &contours,
			sizeof(CvContour), CV_RETR_EXTERNAL,
			CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
#ifdef DEBUG
	cvShowImage("contours", threshold);
	if(contours) {
		cvDrawContours(image, contours,
			CV_RGB(0,0,255), CV_RGB(0,0,255),
			100, 3, CV_AA, cvPoint(0,0));
	}
#endif

	int numContours = 0;
	CvSeq* i;
	for(i = contours; i != 0; i = i->h_next) {
		numContours++;
	}
	rectangle_t* boundaries = (rectangle_t*)malloc(sizeof(rectangle_t) * numContours);
	(*numRects) = numContours;
	int pos = 0;

	for(i = contours; i != 0; i = i->h_next) {
		CvSeq* hull_points = cvConvexHull2(i, storage, CV_CLOCKWISE, 1);
		if(hull_points) {
#ifdef DEBUG			
			cvDrawContours(image, hull_points,
				CV_RGB(255,0,0), CV_RGB(255,0,0),
				1000, CV_FILLED, CV_AA, cvPoint(0,0));			
#endif
			//Guess a bounding rectangle and make pretty pictures with it
			rectangle_t bounds = approximate_bounds(hull_points);
			boundaries[pos] = bounds;
			pos++;			
			if(bounds.width*bounds.height > MIN_AREA) {
#ifdef DEBUG
				cvRectangle(image, cvPoint(bounds.x, bounds.y),
					cvPoint(bounds.x+bounds.width, bounds.y+bounds.height),
					CV_RGB(0,255,0), 3, 8, 0);				
#endif			
			}
		}
	}

#ifdef DEBUG
	cvShowImage("contours", image);
#endif

	cvReleaseImage(&threshold);
	cvReleaseMemStorage(&storage);
	return boundaries;
}
