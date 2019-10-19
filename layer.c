#include <stdlib.h>
#include <string.h>
#include "private.h"

struct liftoff_layer *liftoff_layer_create(struct liftoff_output *output)
{
	struct liftoff_layer *layer;

	layer = calloc(1, sizeof(*layer));
	if (layer == NULL) {
		liftoff_log_errno(LIFTOFF_ERROR, "calloc");
		return NULL;
	}
	layer->output = output;
	liftoff_list_insert(output->layers.prev, &layer->link);
	return layer;
}

void liftoff_layer_destroy(struct liftoff_layer *layer)
{
	if (layer->output->composition_layer == layer) {
		layer->output->composition_layer = NULL;
	}
	free(layer->props);
	liftoff_list_remove(&layer->link);
	free(layer);
}

struct liftoff_layer_property *layer_get_property(struct liftoff_layer *layer,
						  enum liftoff_basic_property prop)
{
	return layer->basic_props[prop];
}

static void layer_invalidate_basic_props(struct liftoff_layer *layer)
{
	size_t i;
	ssize_t basic_prop_idx;

	memset(layer->basic_props, 0, sizeof(layer->basic_props));
	for (i = 0; i < layer->props_len; i++) {
		basic_prop_idx = basic_property_index(layer->props[i].name);
		if (basic_prop_idx >= 0) {
			layer->basic_props[basic_prop_idx] = &layer->props[i];
		}
	}
}

void liftoff_layer_set_property(struct liftoff_layer *layer, const char *name,
				uint64_t value)
{
	struct liftoff_layer_property *props;
	struct liftoff_layer_property *prop;
	ssize_t basic_prop_idx;
	size_t i;

	/* TODO: better error handling */
	if (strcmp(name, "CRTC_ID") == 0) {
		liftoff_log(LIFTOFF_ERROR,
			    "refusing to set a layer's CRTC_ID");
		return;
	}

	basic_prop_idx = basic_property_index(name);
	if (basic_prop_idx >= 0) {
		prop = layer_get_property(layer, basic_prop_idx);
	} else {
		prop = NULL;
		for (i = 0; i < layer->props_len; i++) {
			if (strcmp(layer->props[i].name, name) == 0) {
				prop = &layer->props[i];
				break;
			}
		}
	}

	if (prop == NULL) {
		props = realloc(layer->props, (layer->props_len + 1)
				* sizeof(struct liftoff_layer_property));
		if (props == NULL) {
			liftoff_log_errno(LIFTOFF_ERROR, "realloc");
			return;
		}
		if (layer->props != props) {
			layer->props = props;
			layer_invalidate_basic_props(layer);
		} else {
			layer->props = props;
		}

		prop = &layer->props[layer->props_len];
		memset(prop, 0, sizeof(*prop));
		strncpy(prop->name, name, sizeof(prop->name) - 1);
		layer->props_len++;

		if (basic_prop_idx >= 0) {
			layer->basic_props[basic_prop_idx] = prop;
		}

		prop->changed = true;
	} else {
		prop->changed = prop->value != value;
	}

	prop->value = value;
}

uint32_t liftoff_layer_get_plane_id(struct liftoff_layer *layer)
{
	if (layer->plane == NULL) {
		return 0;
	}
	return layer->plane->id;
}

void layer_get_rect(struct liftoff_layer *layer, struct liftoff_rect *rect)
{
	struct liftoff_layer_property *x_prop, *y_prop, *w_prop, *h_prop;

	x_prop = layer_get_property(layer, LIFTOFF_PROP_CRTC_X);
	y_prop = layer_get_property(layer, LIFTOFF_PROP_CRTC_Y);
	w_prop = layer_get_property(layer, LIFTOFF_PROP_CRTC_W);
	h_prop = layer_get_property(layer, LIFTOFF_PROP_CRTC_H);

	rect->x = x_prop != NULL ? x_prop->value : 0;
	rect->y = y_prop != NULL ? y_prop->value : 0;
	rect->width = w_prop != NULL ? w_prop->value : 0;
	rect->height = h_prop != NULL ? h_prop->value : 0;
}

bool layer_intersects(struct liftoff_layer *a, struct liftoff_layer *b)
{
	struct liftoff_rect ra, rb;

	layer_get_rect(a, &ra);
	layer_get_rect(b, &rb);

	return ra.x < rb.x + rb.width && ra.y < rb.y + rb.height &&
	       ra.x + ra.width > rb.x && ra.y + ra.height > rb.y;
}

void layer_mark_clean(struct liftoff_layer *layer)
{
	size_t i;

	for (i = 0; i < layer->props_len; i++) {
		layer->props[i].changed = false;
	}
}
