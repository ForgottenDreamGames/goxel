#include "volume_stamp.h"
#include "uthash.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const int IMAGE_AXES[3][2] = {{1, 2}, {0, 2}, {0, 1}};

void volume_stamp_basis(int axis, int side,
                        float right[3], float up[3], float normal[3])
{
    memset(right, 0, 3 * sizeof(float));
    memset(up, 0, 3 * sizeof(float));
    memset(normal, 0, 3 * sizeof(float));
    if (axis < 0 || axis > 2 || (side != 1 && side != -1)) return;
    right[IMAGE_AXES[axis][0]] = axis == 1 ? -side : side;
    up[IMAGE_AXES[axis][1]] = 1;
    normal[axis] = side;
}

typedef struct stamp_column {
    UT_hash_handle hh;
    int key[2];
    int pos[3];
    int pixel[2];
} stamp_column_t;

int volume_stamp(const volume_t *src, volume_t *dst,
                  const uint8_t *pixels, int width, int height,
                  const volume_stamp_options_t *options)
{
    stamp_column_t *columns = NULL, *column, *tmp;
    volume_iterator_t iter;
    volume_accessor_t accessor;
    const uint8_t *pixel;
    uint8_t color[4], original[4];
    int pos[3], key[2], x, y, u, v, sign, i, alpha, changed = 0;
    double image_x, image_y;

    if (!src || !dst || src == dst || !pixels || !options ||
            width <= 0 || height <= 0 || options->axis < 0 ||
            options->axis > 2 ||
            (options->side != 1 && options->side != -1) ||
            options->size[0] <= 0 || options->size[1] <= 0)
        return -1;
    for (i = 0; i < 3; i++) {
        if (!isfinite(options->center[i])) return -1;
    }
    u = IMAGE_AXES[options->axis][0];
    v = IMAGE_AXES[options->axis][1];
    sign = options->axis == 1 ? -options->side : options->side;
    volume_set(dst, src);
    iter = volume_get_iterator(src,
            VOLUME_ITER_VOXELS | VOLUME_ITER_SKIP_EMPTY);
    while (volume_iter(&iter, pos)) {
        volume_get_at(src, &iter, pos, color);
        if (color[3] < 127) continue; // Same visibility rule as cube rendering.

        image_x = 0.5 + sign * (pos[u] + 0.5 - options->center[u]) /
                            options->size[0];
        image_y = 0.5 - (pos[v] + 0.5 - options->center[v]) /
                            options->size[1];
        if (image_x < 0 || image_x >= 1 || image_y < 0 || image_y >= 1)
            continue;
        x = (int)(image_x * width);
        y = (int)(image_y * height);
        if (!pixels[((size_t)y * width + x) * 4 + 3]) continue;

        // Keep one hit per voxel column, not per image pixel: enlarging one
        // pixel must paint every frontmost voxel under that pixel.
        key[0] = pos[u];
        key[1] = pos[v];
        HASH_FIND(hh, columns, key, sizeof(key), column);
        if (column) {
            if (options->side > 0 &&
                    pos[options->axis] <= column->pos[options->axis])
                continue;
            if (options->side < 0 &&
                    pos[options->axis] >= column->pos[options->axis])
                continue;
        } else {
            column = calloc(1, sizeof(*column));
            if (!column) {
                changed = -1;
                goto end;
            }
            memcpy(column->key, key, sizeof(key));
            HASH_ADD(hh, columns, key, sizeof(column->key), column);
        }
        memcpy(column->pos, pos, sizeof(pos));
        column->pixel[0] = x;
        column->pixel[1] = y;
    }

    accessor = volume_get_accessor(dst);
    HASH_ITER(hh, columns, column, tmp) {
        pixel = pixels + ((size_t)column->pixel[1] * width +
                          column->pixel[0]) * 4;
        volume_get_at(src, NULL, column->pos, original);
        memcpy(color, original, sizeof(color));
        alpha = pixel[3];
        for (i = 0; i < 3; i++) {
            color[i] = (pixel[i] * alpha + color[i] * (255 - alpha) + 127) /
                       255;
        }
        if (memcmp(color, original, sizeof(color)) == 0) continue;
        volume_set_at(dst, &accessor, column->pos, color);
        changed++;
    }
end:
    HASH_ITER(hh, columns, column, tmp) {
        HASH_DEL(columns, column);
        free(column);
    }
    return changed;
}
