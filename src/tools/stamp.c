#include "goxel.h"
#include "volume_stamp.h"

typedef struct {
    tool_t tool;
    uint8_t *pixels;
    int width, height;
    char name[256];
    volume_stamp_options_t options;
    float drag_center[3];
    bool preview;
    bool dirty;
    bool keep_aspect;
    uint64_t source_key;
    uint64_t bounds_key;
    int bounds[2][3];
    bool has_bounds;
    int changed;
} tool_stamp_t;

static void clear_preview(void)
{
    volume_delete(goxel.tool_volume);
    goxel.tool_volume = NULL;
}

static bool can_stamp(void)
{
    layer_t *layer = goxel.image->active_layer;
    return layer && layer->volume && layer->visible &&
           image_layer_can_edit(goxel.image, layer);
}

static bool get_bounds(tool_stamp_t *tool, int bbox[2][3])
{
    volume_t *volume;
    uint64_t key;

    if (!can_stamp()) return false;
    volume = goxel.image->active_layer->volume;
    key = volume_get_key(volume);
    if (key != tool->bounds_key) {
        tool->has_bounds = volume_get_bbox(volume, tool->bounds, true);
        tool->bounds_key = key;
    }
    memcpy(bbox, tool->bounds, sizeof(tool->bounds));
    return tool->has_bounds;
}

static void basis(const tool_stamp_t *tool, float plane[4][4])
{
    mat4_set_identity(plane);
    volume_stamp_basis(tool->options.axis, tool->options.side,
                       plane[0], plane[1], plane[2]);
    vec3_copy(tool->options.center, plane[3]);
}

static void center_on_layer(tool_stamp_t *tool, bool fit)
{
    int bbox[2][3], i, u = 0, v = 0;
    float plane[4][4], scale;

    if (!get_bounds(tool, bbox)) return;
    basis(tool, plane);
    for (i = 0; i < 3; i++) {
        tool->options.center[i] = (bbox[0][i] + (double)bbox[1][i]) / 2;
        if (plane[0][i]) u = i;
        if (plane[1][i]) v = i;
    }
    if (fit) {
        scale = min((bbox[1][u] - (double)bbox[0][u]) / tool->width,
                    (bbox[1][v] - (double)bbox[0][v]) / tool->height);
        tool->options.size[0] = clamp(round(tool->width * scale), 1, 65536);
        tool->options.size[1] = clamp(round(tool->height * scale), 1, 65536);
    }
    tool->preview = true;
    tool->dirty = true;
}

static bool load_image(tool_stamp_t *tool, const char *path)
{
    char *data;
    uint8_t *pixels;
    int size, width, height, bpp = 4;

    data = read_file(path, &size);
    if (!data) return false;
    pixels = img_read_from_mem(data, size, &width, &height, &bpp);
    free(data);
    if (!pixels) return false;
    free(tool->pixels);
    tool->pixels = pixels;
    tool->width = width;
    tool->height = height;
    path_basename(path, tool->name, sizeof(tool->name));
    tool->options.size[0] = width;
    tool->options.size[1] = height;
    tool->preview = true;
    tool->dirty = true;
    center_on_layer(tool, true);
    return true;
}

static void update_preview(tool_stamp_t *tool)
{
    volume_t *source;
    uint64_t key;

    if (!tool->pixels || !tool->preview || !can_stamp()) {
        clear_preview();
        tool->changed = 0;
        return;
    }
    source = goxel.image->active_layer->volume;
    key = volume_get_key(source);
    if (!tool->dirty && goxel.tool_volume && tool->source_key == key) return;
    if (!goxel.tool_volume) goxel.tool_volume = volume_new();
    tool->changed = volume_stamp(source, goxel.tool_volume, tool->pixels,
                                 tool->width, tool->height, &tool->options);
    tool->source_key = key;
    tool->dirty = false;
    if (tool->changed < 0) clear_preview();
}

static void apply(tool_stamp_t *tool)
{
    update_preview(tool);
    if (goxel.tool_volume && tool->changed > 0) {
        volume_set(goxel.image->active_layer->volume, goxel.tool_volume);
        image_history_push(goxel.image);
    }
    tool->preview = false;
    clear_preview();
}

static int on_drag(gesture3d_t *gest)
{
    tool_stamp_t *tool = gest->user;
    int i;
    float position;
    bool changed = !tool->preview;

    if (gest->state == GESTURE3D_STATE_BEGIN)
        vec3_copy(tool->options.center, tool->drag_center);
    if (!gest->snaped) return 0;
    for (i = 0; i < 3; i++) {
        if (i == tool->options.axis) continue;
        position = tool->drag_center[i] +
                   round(gest->pos[i] - gest->start_pos[i]);
        changed |= position != tool->options.center[i];
        tool->options.center[i] = position;
    }
    tool->preview = true;
    tool->dirty |= changed;
    return 0;
}

static int iter(tool_t *tool_, const painter_t *painter,
                const float viewport[4])
{
    tool_stamp_t *tool = (tool_stamp_t*)tool_;
    float plane[4][4], corners[4][3];
    int bbox[2][3], i, j, axis = tool->options.axis;
    const uint8_t color[4] = {255, 220, 80, 255};
    const int signs[4][2] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    gesture3d_t gesture = {
        .type = GESTURE3D_TYPE_DRAG,
        .snap_mask = SNAP_SHAPE_PLANE,
        .buttons_mask = GESTURE3D_FLAG_CTRL | GESTURE3D_FLAG_SHIFT,
        .callback = on_drag,
        .user = tool,
    };

    if (!can_stamp()) {
        clear_preview();
        goxel_add_hint(0, NULL, _("Select a visible, editable voxel layer"));
        return 0;
    }
    if (!tool->pixels) {
        goxel_add_hint(0, NULL, _("Choose an image in the Stamp tool"));
        return 0;
    }
    if (!get_bounds(tool, bbox)) {
        clear_preview();
        goxel_add_hint(0, NULL, _("Add voxels before stamping"));
        return 0;
    }
    basis(tool, plane);
    plane[3][axis] = (tool->options.side > 0 ? bbox[1][axis] : bbox[0][axis]) +
                     tool->options.side * 0.02f;
    mat4_copy(plane, gesture.snap_shape);
    goxel_gesture3d(&gesture);
    update_preview(tool);

    // Keep the outline visible even when the stamp is outside the model.
    for (i = 0; i < 3; i++) {
        if (i != axis) plane[3][i] = tool->options.center[i];
    }
    for (i = 0; i < 4; i++) {
        vec3_copy(plane[3], corners[i]);
        for (j = 0; j < 2; j++) {
            vec3_iaddk(corners[i], plane[j],
                        signs[i][j] * tool->options.size[j] * 0.5f);
        }
    }
    for (i = 0; i < 4; i++) {
        render_line(&goxel.rend, corners[i], corners[(i + 1) % 4], color,
                     EFFECT_NO_DEPTH_TEST);
    }
    goxel_add_hint(HINT_LARGE, GLYPH_MOUSE_LMB, _("Drag to position stamp"));
    return 0;
}

static void view_from_side(tool_stamp_t *tool)
{
    camera_t *camera = goxel.image->active_camera ?: goxel.image->cameras;
    float box[4][4];

    if (!camera || !can_stamp()) return;
    basis(tool, camera->mat);
    camera->dist = max(camera->dist, 32);
    mat4_itranslate(camera->mat, 0, 0, camera->dist);
    camera->ortho = true;
    volume_get_box(goxel.image->active_layer->volume, true, box);
    if (!box_is_null(box)) camera_fit_box(camera, box);
}

static int gui(tool_t *tool_)
{
    tool_stamp_t *tool = (tool_stamp_t*)tool_;
    const char *path;
    const char *filters[] = {"*.png", "*.jpg", "*.jpeg", "*.bmp", NULL};
    const char *directions[] = {
        _("From +X"), _("From -X"), _("From +Y"),
        _("From -Y"), _("From +Z"), _("From -Z"),
    };
    const char *axes[] = {"X", "Y", "Z"};
    int direction = tool->options.axis * 2 + (tool->options.side < 0);
    int i;
    float position;
    bool changed = false;

    if (gui_button(_("Choose image..."), 1, ICON_IMAGE)) {
        path = sys_open_file_dialog(_("Stamp image"), NULL, filters,
                                    "PNG, JPEG, BMP");
        if (path && !load_image(tool, path))
            gui_alert(_("Stamp image"), _("Could not read this image."));
    }
    if (!tool->pixels) return 0;
    gui_text_wrapped("%s (%d x %d)", tool->name, tool->width, tool->height);
    gui_text(_("Direction"));
    if (gui_combo("##StampDirection", &direction, directions, 6)) {
        tool->options.axis = direction / 2;
        tool->options.side = direction % 2 ? -1 : 1;
        center_on_layer(tool, false);
        changed = true;
    }
    if (gui_button(_("View from stamp side"), 1, 0)) view_from_side(tool);

    gui_group_begin(_("Size in voxels"));
    gui_checkbox(_("Keep aspect ratio"), &tool->keep_aspect, NULL);
    if (gui_input_int(_("Width"), &tool->options.size[0], 0, 0)) {
        tool->options.size[0] = clamp(tool->options.size[0], 1, 65536);
        if (tool->keep_aspect) {
            tool->options.size[1] = clamp(round(
                tool->options.size[0] * (double)tool->height / tool->width),
                1, 65536);
        }
        changed = true;
    }
    if (gui_input_int(_("Height"), &tool->options.size[1], 0, 0)) {
        tool->options.size[1] = clamp(tool->options.size[1], 1, 65536);
        if (tool->keep_aspect) {
            tool->options.size[0] = clamp(round(
                tool->options.size[1] * (double)tool->width / tool->height),
                1, 65536);
        }
        changed = true;
    }
    if (gui_button(_("Fit to layer"), 1, 0)) center_on_layer(tool, true);
    if (gui_button(_("1 pixel per voxel"), 1, 0)) {
        tool->options.size[0] = tool->width;
        tool->options.size[1] = tool->height;
        changed = true;
    }
    gui_group_end();
    gui_group_begin(_("Position"));
    for (i = 0; i < 3; i++) {
        if (i == tool->options.axis) continue;
        position = tool->options.center[i];
        if (gui_input_float(axes[i], &position, 0.5, 0, 0, "%.1f")) {
            tool->options.center[i] = position;
            changed = true;
        }
    }
    gui_group_end();
    if (changed) {
        tool->preview = true;
        tool->dirty = true;
    }
    gui_checkbox(_("Preview"), &tool->preview, NULL);
    update_preview(tool);
    gui_text_wrapped(_("Paints the first visible voxel in the active layer."));
    if (tool->preview) gui_text(_("%d voxels to paint"), max(tool->changed, 0));
    gui_enabled_begin(can_stamp() && tool->preview && tool->changed > 0);
    if (gui_button(_("Apply stamp"), 1, 0)) apply(tool);
    gui_enabled_end();
    if (gui_button(_("Cancel preview"), 1, 0)) {
        tool->preview = false;
        clear_preview();
    }
    return 0;
}

static void init(tool_t *tool_)
{
    tool_stamp_t *tool = (tool_stamp_t*)tool_;
    tool->options.axis = 2;
    tool->options.side = 1;
    tool->options.size[0] = tool->options.size[1] = 32;
    tool->keep_aspect = true;
}

static void release(tool_t *tool_)
{
    tool_stamp_t *tool = (tool_stamp_t*)tool_;
    free(tool->pixels);
    tool->pixels = NULL;
}

TOOL_REGISTER(TOOL_STAMP, stamp, tool_stamp_t,
              .name = N_("Image Stamp"),
              .init_fn = init,
              .release_fn = release,
              .iter_fn = iter,
              .gui_fn = gui,
)
