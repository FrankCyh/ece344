#include "point.h"

#include <assert.h>
#include <math.h>
#include "common.h"

void point_translate(struct point *p, double x, double y) {
    p->x += x;
    p->y += y;
}

double point_distance(const struct point *p1, const struct point *p2) {
    double distance = sqrt(pow(p1->x - p2->x, 2) + pow(p1->y - p2->y, 2));
    return distance;
}

int point_compare(const struct point *p1, const struct point *p2) {
    double p1Length = sqrt(pow(p1->x, 2) + pow(p1->y, 2));
    double p2Length = sqrt(pow(p2->x, 2) + pow(p2->y, 2));
	if(p1Length > p2Length)
        return 1;
	else if(p1Length == p2Length)
        return 0;
	else
		return -1;
}
