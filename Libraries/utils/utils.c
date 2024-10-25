#include "utils.h"

static void reverseArray(uint32_t arr[], int start, int end) {
    while (start < end) {
        int temp = arr[start];
        arr[start] = arr[end];
        arr[end] = temp;
        start++;
        end--;
    }
}

void rotateArray_uint32(uint32_t arr[], int n, int k) {
    k %= n;
    reverseArray(arr, 0, n - k - 1);
    reverseArray(arr, n - k, n - 1);
    reverseArray(arr, 0, n - 1);
}

static void reverseArray_uint8(uint8_t arr[], int start, int end) {
    while (start < end) {
        uint8_t temp = arr[start];
        arr[start] = arr[end];
        arr[end] = temp;
        start++;
        end--;
    }
}

void rotateArray_uint8(uint8_t arr[], int n, int k) {
    k %= n;
    reverseArray_uint8(arr, 0, n - k - 1);
    reverseArray_uint8(arr, n - k, n - 1);
    reverseArray_uint8(arr, 0, n - 1);
}
