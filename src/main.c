#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include "network.h"
#include "rectangle.h"

/*
	When approximating rectangles from contours, if the area of the
	rectangle is less than this value it is discarded
*/
#define MIN_AREA 100
#define DEBUG

#define TARGET_ASPECT_RATIO 54.0/21.0
#define ACCEPTABLE_ERROR    .10

#define THRESHOLD_RED_MIN   125
#define THRESHOLD_RED_MAX   255

#define THRESHOLD_GREEN_MIN 125
#define THRESHOLD_GREEN_MAX 255

#define THRESHOLD_BLUE_MIN  125
#define THRESHOLD_BLUE_MAX  255

#define THRESHOLD_WHITE_MIN 200
#define THRESHOLD_WHITE_MAX 255

#define THRESHOLD_BLACK_MIN   0
#define THRESHOLD_BLACK_MAX  45

enum color_channels { CHANNEL_RED, CHANNEL_GREEN, CHANNEL_BLUE };
enum track_target   { TARGET_RED, TARGET_GREEN, TARGET_BLUE, TARGET_WHITE, TARGET_BLACK };

IplImage*    split_channel(IplImage* input, int channel);
rectangle_t* track(IplImage* image, int target, int* num_rects);
float        handle_times(IplImage* frame, int* start_time, int* end_time, int* received_frames);

int main(int argc, char* argv[]) {
	IplImage*     captured_frame;
	CvCapture*    capture;
	CvMemStorage* storage;

	socket_t* dashboard;
	char*     stream_url;
	float     prev_target_x;
	float     prev_target_y;

	int received_frames;
	int start_time;
	int end_time;

	printf("Connecting to dashboard...\n");
	dashboard = socket_open("10.10.14.42", "2000");
	while(dashboard == NULL) {
		sleep(2);
		dashboard = socket_open("10.10.14.42", "2000");
	}
	printf("Connected!\n");

	//avformat_network_init();
	stream_url = "rtsp://10.10.14.11:554/axis-media/media.amp?videocodec=h264&streamprofile=Bandwidth";
	capture = cvCaptureFromFile(stream_url);
	if(!capture) {
		printf("Unable to capture from URL\n");
		return -1;
	}

	prev_target_x     = 0;
	prev_target_y     = 0;
	start_time        = time(0);
	end_time          = start_time;

	received_frames   = 0
	storage           = cvCreateMemStorage(0);
	//yeah... while(true).. sue me...
	while(1) {
		CvSize image_size;

		rectangle_t* rectangles;
		int num_rects;
		float target_x;
		float target_y;
		float time_since_valid;
		float error;
		float best_error;

		target_x = 0;
		target_y = 0;
		captured_frame = cvQueryFrame(capture);
		time_since_valid = handle_times(captured_frame, &start_time, &end_time, &received_frames);

		if(time_since_valid < 0) {
			//Re-acquire the image stream
			cvReleaseCapture(&capture);
			capture = cvCaptureFromFile(stream_url);
			continue;
		} else {
			best_error = 100;

			image_size = cvGetSize(captured_frame);
			rectangles = track(captured_frame, TARGET_RED, &num_rects);
			int i;
			for(i = 0; i < num_rects; i++) {
				if(rectangles[i].width*rectangles[i].height < MIN_AREA) {
					continue;
				}
				rectangles[i] = normalize_bounds(rectangles[i], image_size);
				error = (rectangles[i].width/rectangles[i].height)-TARGET_ASPECT_RATIO;
				error /= TARGET_ASPECT_RATIO;
				if(error < 0)
					error = -error;
				if(error <= ACCEPTABLE_ERROR && error < best_error) {
					target_x   = rectangles[i].x+(rectangles[i].width/2);
					target_y   = rectangles[i].y+(rectangles[i].height/2);
					best_error = error;
				}
			}
			free(rectangles);
			if(best_error == 100)
				time_since_valid = -1;
		}

		socket_write_float(dashboard, target_x);
		socket_write_float(dashboard, target_y);
		socket_write_float(dashboard, time_since_valid);

		if(target_x != prev_target_x || target_y != prev_target_y) {
			printf("Changed target to: [x: %f, y: %f, time: %f]\n", target_x, target_y, time_since_valid);
		}

		prev_target_x = target_x;
		prev_target_y = target_y;
#ifdef DEBUG
		char key = cvWaitKey(1);
		if(key == 'q') {
			break;
		}
#endif
		cvClearMemStorage(storage);
	}
	socket_release(dashboard);
	cvReleaseMemStorage(&storage);
	cvReleaseCapture(&capture);
	return 0;
}
/*
	Takes a colour image and splits it into it's individual channels
	but also removes what is common between the requested channel
	and the others.
*/
IplImage* split_channel(IplImage* input, int channel) {
	IplImage* result = cvCreateImage(cvGetSize(input), 8, 1);

	IplImage* red_channel   = cvCreateImage(cvGetSize(input), 8, 1);
	IplImage* green_channel = cvCreateImage(cvGetSize(input), 8, 1);
	IplImage* blue_channel  = cvCreateImage(cvGetSize(input), 8, 1);
	cvSplit(input, blue_channel, green_channel, red_channel, NULL);
	if(channel == CHANNEL_BLUE) {
		cvAdd(red_channel, green_channel, green_channel, NULL);
		cvSub(blue_channel, green_channel, result, NULL);
	} else if(channel == CHANNEL_GREEN) {
		cvAdd(red_channel, blue_channel, blue_channel, NULL);
		cvSub(green_channel, blue_channel, result, NULL);
	} else {
		cvAdd(blue_channel, green_channel, green_channel, NULL);
		cvSub(red_channel, green_channel, result, NULL);
	}

	cvReleaseImage(&red_channel);
	cvReleaseImage(&green_channel);
	cvReleaseImage(&blue_channel);
	return result;
}

/*
	Black doesn't quite work, the others should be fine
	as is or with tweaking to the threshold values
*/
rectangle_t* track(IplImage* image, int target, int* num_rects) {
	CvMemStorage* storage = cvCreateMemStorage(0);

	IplImage* threshold = cvCreateImage(cvGetSize(image), 8, 1);

	CvSeq* contours;
	if(target == TARGET_BLUE) {
		IplImage* blue_image = split_channel(image, CHANNEL_BLUE);
		cvThreshold(blue_image, threshold, THRESHOLD_BLUE_MIN, THRESHOLD_BLUE_MAX, CV_THRESH_OTSU);

		cvReleaseImage(&blue_image);
	} else if(target == TARGET_GREEN) {
		IplImage* green_image = split_channel(image, CHANNEL_GREEN);
		cvThreshold(green_image, threshold, THRESHOLD_GREEN_MIN, THRESHOLD_GREEN_MAX, CV_THRESH_OTSU);

		cvReleaseImage(&green_image);
	} else if(target == TARGET_RED) {
		IplImage* red_image = split_channel(image, CHANNEL_RED);
		cvThreshold(red_image, threshold, THRESHOLD_RED_MIN, THRESHOLD_RED_MAX, CV_THRESH_OTSU);

		cvReleaseImage(&red_image);
	} else if(target == TARGET_WHITE) {
		IplImage* gray_scale = cvCreateImage(cvGetSize(image), 8, 1);
		cvCvtColor(image, gray_scale, CV_RGB2GRAY);
		cvThreshold(gray_scale, threshold, THRESHOLD_WHITE_MIN, THRESHOLD_WHITE_MAX, CV_THRESH_BINARY);

		cvReleaseImage(&gray_scale);
	} else if(target == TARGET_BLACK) {
		IplImage* gray_scale = cvCreateImage(cvGetSize(image), 8, 1);
		cvCvtColor(image, gray_scale, CV_RGB2GRAY);
		cvThreshold(gray_scale, threshold, THRESHOLD_BLACK_MIN, THRESHOLD_BLACK_MAX, CV_THRESH_BINARY);

		cvReleaseImage(&gray_scale);
	}
#ifdef DEBUG
	cvShowImage("threshold", threshold);
#endif
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

	int num_contours = 0;
	CvSeq* i;
	for(i = contours; i != 0; i = i->h_next) {
		num_contours++;
	}
	rectangle_t* boundaries = (rectangle_t*)malloc(sizeof(rectangle_t) * num_contours);
	(*num_rects) = num_contours;
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
			if(bounds.width*bounds.height >= MIN_AREA) {
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

float handle_times(IplImage* frame, int* start_time, int* end_time, int* received_frames) {
	if(!frame) {
		printf("Received %i frames before losing one\n", (*received_frames));
		(*received_frames) = 0;
		(*start_time) = time(0);
		return -1;
	}
	(*end_time) = time(0);
	(*received_frames)++;
	return (*end_time)-(*start_time);
}
