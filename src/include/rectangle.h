#ifndef RECTANGLE_H
#define RECTANGLE_H

#include <opencv/cv.h> //CvSeq, CvSize

typedef struct {
	float x;
	float y;
	float width;
	float height;
} rectangle_t;

//The create function is kinda unnecessary...
rectangle_t rectangle_create(float x, float y, float width, float height);
rectangle_t approximate_bounds(CvSeq* points);
rectangle_t normalize_bounds(rectangle_t bounds, CvSize size);

#endif
