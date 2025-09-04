#include "rules.h"
#include <stdlib.h>

static inline int random_integer_between(int lower_bound, int upper_bound) {
    return rand() % (upper_bound - lower_bound + 1) + lower_bound;
}

static inline int acc_random_integer_between(int n, int lower_bound, int upper_bound) {
    int acc = 0;
    for (int i = 0; i < n; i++) {
        acc += random_integer_between(lower_bound, upper_bound);
    }
    return acc;
}

int rules_d4(int n) {
    return acc_random_integer_between(n, 1, 4);
}

int rules_d6(int n) {
    return acc_random_integer_between(n, 1, 6);
}

int rules_d8(int n) {
    return acc_random_integer_between(n, 1, 8);
}

int rules_d10(int n) {
    return acc_random_integer_between(n, 1, 10);
}

int rules_d12(int n) {
    return acc_random_integer_between(n, 1, 12);
}

int rules_d20(int n) {
    return acc_random_integer_between(n, 1, 20);
}

int rules_d100(int n) {
    return acc_random_integer_between(n, 1, 100);
}
