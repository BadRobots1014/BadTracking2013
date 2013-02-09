/*
	This code utilizes libfreenect in order to grab images from the Kinect and uses
	OpenCV to handle image processing. This code uses the synchronous freenect API
	which may occasionally lead to some lag. Likely this will be changed to instead
	use the asynchronous API if it becomes a problem. This code was pulled together
	in a matter of two days so don't expect it to be the most beautiful thing you've
	ever laid eyes on. 
	
	* Every call to cvShowImage has lead to a memory leak, so every time you test this
	expect a limited amount of time before you run out of RAM (my computer around 30-45 seconds).
	The good news here is if you remove these calls you shouldn't have any worries about running
	out of memory (should run fine on a Pi)

	* For whatever reason the green colour channel doesn't seem to pick up as well as the
	other two.

	* For all who venture further, may the source be with you!

	DEPENDENCIES: Libfreenect, OpenCV
	AUTHORS: Team 1014 ( http://dublinrobotics.blogspot.com/ )
	SEE: http://openkinect.org/wiki/Getting_Started
		 http://opencv.willowgarage.com/documentation/c/index.html
		 https://github.com/BadRobots1014
*/

#include <stdlib.h> //noah is awesome.
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>

//#include <libfreenect_sync.h>

#define MIN_AREA 100
#define DEBUG

#define TARGET_AREA 9001
#define TARGET_ASPECT_RATIO 4.5
#define PERROR 1

typedef struct {
	float x;
	float y;
	float width;
	float height;
} rectangle_t;

enum color_channels { CHANNEL_BLUE, CHANNEL_GREEN, CHANNEL_RED };
enum track_target { TARGET_RED, TARGET_GREEN, TARGET_BLUE, TARGET_WHITE, TARGET_BLACK };

IplImage* splitChannel(IplImage* input, int channel);
rectangle_t* track(IplImage* image, int target, int* numRects);
rectangle_t approximateBounds(CvSeq* points);
rectangle_t normalizeBounds(rectangle_t bounds, CvSize size);

int openConnection(char* hostname, char* port_str);
void writeFloat(int fd, float f);
float reverseFloat(float f);

int main(int argc, char* argv[]) {
	char* stream_url = "rtsp://10.10.14.11:554/axis-media/media.amp?videocodec=h264&streamprofile=Bandwidth";
	int dashboard = openConnection("10.10.14.42", "2000");
	printf("%i\n", dashboard);

	avformat_network_init();
	CvCapture* capture = cvCaptureFromFile(stream_url);
	if(!capture) {
		printf("No capture!\n");
		return;
	}
	int lostFrames = 0;
	int receivedFrames = 0;
	IplImage* image;
	CvMemStorage* storage = cvCreateMemStorage(0);
	while(1) {
		/*printf("Grabbing frame..\n");
		int index = cvGrabFrame(capture);
		printf("Retrieving frame...\n");
		image = cvRetrieveFrame(capture, index);*/
		image = cvQueryFrame(capture);
		if(!image) {
			printf("Retrieved %i frames before losing one!\n", receivedFrames);
			receivedFrames = 0;
			printf("No Frame!\n");
			lostFrames++;
			if(lostFrames >= 3) {
				cvReleaseCapture(&capture);
				capture = cvCaptureFromFile(stream_url);
				lostFrames = 0;
			}		
			continue;
		}
		receivedFrames++;
		//printf("Retrieved frame!\n");
		//cvShowImage("image", image);

		int numRects = 0;
		rectangle_t* rectangles = track(image, TARGET_RED, &numRects);

		float targetX = 0;
		float targetY = 0;

		CvSize imageSize = cvGetSize(image);

		rectangle_t bestFit;
		float bestError = 1;

		//printf("numRects = %i\n", numRects);
		int i;
		for(i = 0; i < numRects; i++) {
			rectangles[i] = normalizeBounds(rectangles[i], imageSize);
			//writeFloat(dashboard, rectangles[i].x);
			//writeFloat(dashboard, rectangles[i].y);
			float error = ((rectangles[i].width/rectangles[i].height)-TARGET_ASPECT_RATIO)/TARGET_ASPECT_RATIO;
			if(error < 0) error = -error;
			if(error <= PERROR && error < bestError) {
				targetX = rectangles[i].x+(rectangles[i].width/2);
				targetY = rectangles[i].y+(rectangles[i].height/2);			
			}
		}

		//printf("(%f, %f)\n", targetX, targetY);
		writeFloat(dashboard, targetX);
		writeFloat(dashboard, targetY);
		free(rectangles);

		char key = cvWaitKey(1);
		if(key == 'q') {
			break;
		}
		//cvReleaseImage(&image);
		cvClearMemStorage(storage);
	}
	close(dashboard);
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

rectangle_t approximateBounds(CvSeq* points) {
	rectangle_t bounds;
	float smallestX;
	float smallestY;
	float largestX;
	float largestY;

	if(points->total <= 1) {
		bounds.x = -1;
		bounds.y = -1;
		bounds.width = -1;
		bounds.height = -1;
		return bounds;
	}

	CvPoint* first = (CvPoint*)cvGetSeqElem(points, 0);
	smallestX = first->x;
	smallestY = first->y;
	largestX = first->x;
	largestY = first->y;

	int i;
	for(i = 1; i < points->total; i++) {
		CvPoint* point = (CvPoint*)cvGetSeqElem(points, i);
		
		int x = point->x;
		int y = point->y;
		if(x < smallestX) {
			smallestX = x;
		} else if(x > largestX) {
			largestX = x;
		}
		if(y < smallestY) {
			smallestY = y;
		} else if(y > largestY) {
			largestY = y;
		}
		first = point;
	}

	bounds.width = largestX-smallestX;
	bounds.height = largestY-smallestY;
	bounds.x = smallestX;
	bounds.y = smallestY;
	return bounds;
}

rectangle_t normalizeBounds(rectangle_t bounds, CvSize size) {
	rectangle_t result;
	result.x = (bounds.x-(size.width/2))/size.width;
	result.y = (bounds.y-(size.height/2))/size.height;
	result.width = (bounds.width/size.width);
	result.height = (bounds.height/size.height);
	return result;
}

rectangle_t* track(IplImage* image, int target, int* numRects) {
	CvMemStorage* storage = cvCreateMemStorage(0);

	//cvDilate(image, image, NULL, 2);
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
		cvThreshold(grayScale, threshold, 200, 255, CV_THRESH_BINARY);

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
			rectangle_t bounds = approximateBounds(hull_points);
			//printf("Saving rect %i of %i\n", pos, numContours);
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

int openConnection(char* hostname, char* port_str) {
	struct addrinfo* info;
	struct addrinfo hints;
	
	int port = atoi(port_str);
	hints.ai_family = AF_UNSPEC;
	hints.ai_protocol = 0;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;;

	int r = getaddrinfo(hostname, port_str, &hints, &info);
	if(r != 0) {
		printf("Unable to get host...\n");
		printf("%s\n", gai_strerror(r));
		return -1;
	}
	
	int fd = socket(info->ai_family, info->ai_socktype,
						info->ai_protocol);
	printf("Connecting...\n");
	if(connect(fd, info->ai_addr, info->ai_addrlen) == -1) {
		printf("Unable to connect, fd = %i\n", fd);
		return -1;
	}
	printf("Connected!");

	freeaddrinfo(info);

	return fd;
}
void writeFloat(int fd, float f) {
	f = reverseFloat(f);
	write(fd, &f, sizeof(f));
}
float reverseFloat(float f) {
	float ret;
	char* buffer = (char*)&f;
	char* resultBuffer = (char*)&ret;

	resultBuffer[0] = buffer[3];
	resultBuffer[1] = buffer[2];
	resultBuffer[2] = buffer[1];
	resultBuffer[3] = buffer[0];

	return ret;
}
