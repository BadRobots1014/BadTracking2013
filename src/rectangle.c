#include "rectangle.h"

rectangle_t create_rectangle(float x, float y, float width, float height) {
	rectangle_t result;
	result.x = x;
	result.y = y;
	result.width = width;
	result.height = height;
	return result;
}

rectangle_t approximate_bounds(CvSeq* points) {
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

rectangle_t normalize_bounds(rectangle_t bounds, CvSize size) {
	rectangle_t result;
	result.x = (bounds.x-(size.width/2))/size.width;
	result.y = (bounds.y-(size.height/2))/size.height;
	result.width  = bounds.width/size.width;
	result.height = bounds.height/size.height;
	return result;
}
