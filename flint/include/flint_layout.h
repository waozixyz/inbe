#ifndef FLINT_LAYOUT_H
#define FLINT_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Set the view dimensions (should be called when window/viewport changes)
void flint_set_view_size(int width, int height);

// Get the current view width
int flint_view_width(void);

// Get the current view height
int flint_view_height(void);

// Calculate centered column dimensions
// Returns x position and width via pointers if not NULL
void flint_centered_column(int max_w, int side_pad, int *x, int *w);

// Calculate page side padding based on current view width
int flint_page_side_padding(void);

#ifdef __cplusplus
}
#endif

#endif // FLINT_LAYOUT_H
