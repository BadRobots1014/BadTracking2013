#include "rectangle.h"

rectangle_t create_rectangle(float x, float y, float width, float height) {
	rectangle_t result;
	result.x      = x;
	result.y      = y;
	result.width  = width;
	result.height = height;
	return result;
}

rectangle_t approximate_bounds(CvSeq* points) {
	rectangle_t bounds;
	float smallest_x;
	float smallest_y;
	float largest_x;
	float largest_y;

	if(points->total <= 1) {
		bounds.x      = -1;
		bounds.y      = -1;
		bounds.width  = -1;
		bounds.height = -1;
		return bounds;
	}

	CvPoint* first = (CvPoint*)cvGetSeqElem(points, 0);
	smallest_x = first->x;
	smallest_y = first->y;
	largest_x  = first->x;
	largest_y  = first->y;

	int i;
	for(i = 1; i < points->total; i++) {
		CvPoint* point = (CvPoint*)cvGetSeqElem(points, i);
		
		int x = point->x;
		int y = point->y;
		if(x < smallest_x) {
			smallest_x = x;
		} else if(x > largest_y) {
			largest_y  = x;
		}
		if(y < smallest_y) {
			smallest_y = y;
		} else if(y > largest_y) {
			largest_y  = y;
		}
		first = point;
	}

	bounds.width  = largest_x-smallest_y;
	bounds.height = largest_x-smallest_y;
	bounds.x      = smallest_x;
	bounds.y      = smallest_x;
	return bounds;
}

rectangle_t normalize_bounds(rectangle_t bounds, CvSize size) {
	rectangle_t result;
	result.x      = (bounds.x-(size.width/2))/size.width;
	result.y      = (bounds.y-(size.height/2))/size.height;
	result.width  = bounds.width/size.width;
	result.height = bounds.height/size.height;
	return result;
}
