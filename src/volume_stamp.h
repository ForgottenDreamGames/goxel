/* Image projection onto the first visible voxel of each volume column. */

#ifndef VOLUME_STAMP_H
#define VOLUME_STAMP_H

#include "volume.h"

typedef struct {
    int axis;           // X, Y or Z: 0, 1 or 2.
    int side;           // Project from the positive (+1) or negative (-1) side.
    float center[3];    // Center of the image in voxel coordinates.
    int size[2];        // Image width and height in voxels.
} volume_stamp_options_t;

// Image right, image up and outward normal, viewed from the stamping side.
// Z is up for X/Y projection; Y is up for Z projection.
void volume_stamp_basis(int axis, int side,
                        float right[3], float up[3], float normal[3]);

// Copy src to dst, then paint its frontmost visible voxels. Pixels are RGBA,
// top row first, sampled by nearest neighbor at voxel centers. Image alpha
// blends RGB only; voxel occupancy/alpha is preserved. src must differ from
// dst. Return the number of changed voxels, or -1 for invalid input.
int volume_stamp(const volume_t *src, volume_t *dst,
                  const uint8_t *pixels, int width, int height,
                  const volume_stamp_options_t *options);

#endif // VOLUME_STAMP_H
