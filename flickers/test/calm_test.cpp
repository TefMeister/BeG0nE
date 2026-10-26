// calm_test.cpp -- the turn the calm pass uses to find a pixel in the previous picture (fb_calm.h, pure CPU).
#include <math.h>
#include <stdio.h>
#include "../src/core/fb_calm.h"

static int fails = 0, checks = 0;
static void close_to(float a, float b, const char* what) {
    checks++;
    if (fabsf(a - b) > 1e-4f) { fails++; printf("FAIL %s: %f vs %f\n", what, a, b); }
}
static XrQuaternionf yaw(float deg) {     // a turn about the up axis (+Y), left is positive
    const float h = deg * 3.14159265f / 360.0f;
    return XrQuaternionf{0, sinf(h), 0, cosf(h)};
}
static void apply(const float r[3][4], const float d[3], float o[3]) {
    for (int i = 0; i < 3; i++) o[i] = r[i][0] * d[0] + r[i][1] * d[1] + r[i][2] * d[2];
}

using fb::calm_turn;

int main() {
    float r[3][4], o[3];
    const float ahead[3] = {0, 0, -1};
    calm_turn(yaw(30), yaw(30), r);       // no turn: every direction stays where it is
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) close_to(r[i][j], i == j ? 1.0f : 0.0f, "same pose is no turn");
    calm_turn(yaw(10), yaw(0), r);        // the head turned 10 degrees left since the previous picture
    apply(r, ahead, o);                   // straight ahead now was 10 degrees to the left before
    close_to(o[0], -sinf(10 * 3.14159265f / 180), "turned left: ahead now was left of centre before (x)");
    close_to(o[2], -cosf(10 * 3.14159265f / 180), "turned left: ahead now was left of centre before (z)");
    close_to(o[1], 0, "a yaw does not move things up or down");
    calm_turn(yaw(0), yaw(10), r);        // and back the other way
    apply(r, ahead, o);
    close_to(o[0], sinf(10 * 3.14159265f / 180), "turned right: ahead now was right of centre before");
    printf("calm_test: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
