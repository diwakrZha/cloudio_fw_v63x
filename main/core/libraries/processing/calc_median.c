#include "calc_median.h"
#include <stdlib.h>

// Function to compare two numbers - used in qsort
static int compare(const void * a, const void * b) {
  return (*(const float*)a > *(const float*)b) - (*(const float*)a < *(const float*)b);
}

float calculate_median(const float values[], int n) {
    // Copy the array since qsort modifies the array
    float sorted[n];
    for (int i = 0; i < n; i++) {
        sorted[i] = values[i];
    }

    // Sort the copied array
    qsort(sorted, n, sizeof(float), compare);

    // Calculate median
    if (n % 2 == 0) {
        // If even number of elements, return average of middle two
        return (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
    } else {
        // If odd, return the middle element
        return sorted[n / 2];
    }
}
