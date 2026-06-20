/*****************************************************************************
 * lyrics_overlay.h: shared SYLT synchronized-lyrics overlay for visualizers
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_VISUALIZATION_LYRICS_OVERLAY_H_
#define VLC_VISUALIZATION_LYRICS_OVERLAY_H_

#include <vlc_common.h>
#include <vlc_tick.h>

typedef struct lyrics_overlay_t lyrics_overlay_t;
typedef struct vout_thread_t vout_thread_t;

/* Create a lyrics overlay helper for a visualization plugin.
 * obj is the plugin's vlc object (used for logging and var inheritance).
 * width/height are the visualization frame dimensions used to position text. */
lyrics_overlay_t *lyrics_overlay_New(vlc_object_t *obj, int width, int height);

/* Call once per rendered visualization frame, after vout_PutPicture().
 * i_pts is the PTS of the audio block driving this frame. */
void lyrics_overlay_Update(lyrics_overlay_t *, vout_thread_t *vout,
                           vlc_tick_t i_pts);

void lyrics_overlay_Delete(lyrics_overlay_t *);

#endif
