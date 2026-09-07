/* Run from the repository root (MSYS2 MINGW64 on Windows):
 * gcc -std=gnu99 -Wall -Werror -Isrc -Iext_src/uthash tests/volume_stamp.c
 *     src/volume.c src/volume_stamp.c -lm -o /tmp/test_volume_stamp
 * (Join the two compiler command lines above.) Then /tmp/test_volume_stamp.
 */

#include "volume_stamp.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const uint8_t ORIGINAL[4] = {10, 20, 30, 255};
static const uint8_t RED[4] = {250, 0, 0, 255};

static void check_color(const volume_t *volume, const int pos[3],
                         const uint8_t expected[4])
{
    uint8_t actual[4];
    volume_get_at(volume, NULL, pos, actual);
    assert(memcmp(actual, expected, 4) == 0);
}

static void test_directions(void)
{
    // Distinct corners catch image mirroring and upside-down projection.
    const uint8_t pixels[4][4] = {
        {250, 0, 0, 255}, {0, 250, 0, 255},
        {0, 0, 250, 255}, {250, 250, 0, 255},
    };
    const int axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};
    volume_t *src = volume_new(), *dst = volume_new();
    volume_stamp_options_t options = {.size = {2, 2}};
    int axis, side, x, y, z, pos[3], pixel_x, pixel_y, sign;
    uint64_t key;

    for (x = -1; x <= 0; x++)
    for (y = -1; y <= 0; y++)
    for (z = -1; z <= 0; z++)
        volume_set_at(src, NULL, (int[]){x, y, z}, ORIGINAL);
    key = volume_get_key(src);
    for (axis = 0; axis < 3; axis++) {
        for (side = -1; side <= 1; side += 2) {
            options.axis = axis;
            options.side = side;
            assert(volume_stamp(src, dst, &pixels[0][0], 2, 2, &options) == 4);
            assert(volume_get_key(src) == key);
            sign = axis == 1 ? -side : side;
            for (x = -1; x <= 0; x++)
            for (y = -1; y <= 0; y++)
            for (z = -1; z <= 0; z++) {
                memcpy(pos, (int[]){x, y, z}, sizeof(pos));
                check_color(src, pos, ORIGINAL);
                pixel_x = pos[axes[axis][0]] + 1;
                if (sign < 0) pixel_x = 1 - pixel_x;
                pixel_y = -pos[axes[axis][1]];
                check_color(dst, pos, pos[axis] == (side > 0 ? 0 : -1) ?
                             pixels[pixel_y * 2 + pixel_x] : ORIGINAL);
            }
        }
    }
    volume_delete(src);
    volume_delete(dst);
}

static void test_occlusion_scale_and_pan(void)
{
    volume_t *src = volume_new(), *dst = volume_new();
    volume_stamp_options_t options = {
        .axis = 2, .side = 1, .center = {-16, 0, 0}, .size = {2, 2},
    };
    int x, y;

    // A single enlarged pixel must color four columns across a tile boundary.
    for (x = -17; x <= -16; x++)
    for (y = -1; y <= 0; y++) {
        volume_set_at(src, NULL, (int[]){x, y, -100}, ORIGINAL);
        volume_set_at(src, NULL, (int[]){x, y, 100}, ORIGINAL);
    }
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == 4);
    for (x = -17; x <= -16; x++)
    for (y = -1; y <= 0; y++) {
        check_color(dst, (int[]){x, y, 100}, RED);
        check_color(dst, (int[]){x, y, -100}, ORIGINAL);
    }
    // Removing a front voxel reveals the deeper one in that column.
    volume_set_at(src, NULL, (int[]){-17, -1, 100}, (uint8_t[4]){0});
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == 4);
    check_color(dst, (int[]){-17, -1, -100}, RED);
    check_color(dst, (int[]){-17, -1, 100}, (uint8_t[4]){0});

    // A new preview starts from src, not the previous painted preview.
    options.center[0] += 1;
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == 2);
    check_color(dst, (int[]){-17, -1, -100}, ORIGINAL);
    check_color(dst, (int[]){-16, -1, 100}, RED);
    options.center[0] += 100;
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == 0);
    check_color(dst, (int[]){-16, -1, 100}, ORIGINAL);
    volume_delete(src);
    volume_delete(dst);
}

static void test_alpha_and_sampling(void)
{
    volume_t *src = volume_new(), *dst = volume_new();
    volume_stamp_options_t options = {
        .axis = 2, .side = 1, .center = {0.5, 0.5, 0}, .size = {1, 1},
    };
    const uint8_t pixels[4][4] = {
        {200, 0, 0, 255}, {0, 200, 0, 255},
        {0, 0, 200, 255}, {110, 120, 130, 128},
    };
    const int pos[3] = {0, 0, 0};

    volume_set_at(src, NULL, pos, (uint8_t[]){10, 20, 30, 200});
    // Downsampling samples the pixel containing the voxel center.
    assert(volume_stamp(src, dst, &pixels[0][0], 2, 2, &options) == 1);
    check_color(dst, pos, (uint8_t[]){60, 70, 80, 200});
    assert(volume_stamp(src, dst, (uint8_t[]){255, 0, 0, 0},
                         1, 1, &options) == 0);
    check_color(dst, pos, (uint8_t[]){10, 20, 30, 200});
    // Invisible voxels cannot occlude a visible one or acquire occupancy.
    volume_set_at(src, NULL, (int[]){0, 0, 1}, (uint8_t[]){1, 2, 3, 126});
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == 1);
    check_color(dst, (int[]){0, 0, 1}, (uint8_t[]){1, 2, 3, 126});
    options.axis = 3;
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == -1);
    options.axis = 2;
    options.size[0] = 0;
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == -1);
    options.size[0] = 1;
    options.center[0] = NAN;
    assert(volume_stamp(src, dst, RED, 1, 1, &options) == -1);
    volume_delete(src);
    volume_delete(dst);
}

int main(void)
{
    test_directions();
    test_occlusion_scale_and_pan();
    test_alpha_and_sampling();
    puts("Image stamp tests passed: six directions, occlusion, scaling, "
         "panning, alpha, sampling, source preservation and input validation.");
    return 0;
}
