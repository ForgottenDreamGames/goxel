# Image Stamp

The Image Stamp tool projects a PNG, JPEG, or BMP onto existing voxels in the
active layer. It paints the first visible voxel in each column viewed from
the selected direction. Voxels behind that surface are left unchanged.

1. Select a visible, editable voxel layer.
2. Open the Tools panel and select **Image Stamp** (the picture icon).
3. Click **Choose image...** and select an image file.
4. Choose **From +X**, **From -X**, **From +Y**, **From -Y**, **From +Z**, or
   **From -Z**. For example, **From +Z** stamps downward onto the top surface.
5. Use **View from stamp side** to align the camera with the projection.
6. Drag with the left mouse button in the viewport to position the stamp.
   Width and Height set its size in voxels; Position allows finer adjustment.
7. Click **Apply stamp** to commit, or **Cancel preview** to discard the preview.

New images initially fit inside the layer's projected bounds while retaining
their aspect ratio. **Fit to layer** repeats that placement. **1 pixel per
voxel** sets the stamp's voxel dimensions to the image's pixel dimensions.
Uncheck **Keep aspect ratio** to change width and height independently.

Pixels are sampled without smoothing. Transparent pixels leave the model
unchanged; partially transparent pixels blend their color with the voxel's
existing color. Stamping does not create or erase voxels. Each painted voxel
still has one color for all of its faces.

Preview changes do not modify the model. Each application is one undo step.
Normal saving and exporting preserve the applied colors, including `.gox`,
OBJ, PLY, glTF and GLB. The source image is not required to reopen the painted
model. Image selection and projection controls are session settings.

For X/Y projections, image up is world +Z. For Z projections, image up is
world +Y. Image left/right is oriented as viewed from the selected side.
Other layers do not participate in determining the first visible voxel.
