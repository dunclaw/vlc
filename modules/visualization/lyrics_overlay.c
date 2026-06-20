/*****************************************************************************
 * lyrics_overlay.c: shared SYLT synchronized-lyrics overlay for visualizers
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_vout.h>
#include <vlc_subpicture.h>
#include <vlc_text_style.h>

#include <stdlib.h>
#include <string.h>

#include "lyrics_overlay.h"

typedef struct
{
    vlc_tick_t  i_start;
    char       *psz_text;
} lyric_entry_t;

struct lyrics_overlay_t
{
    vlc_object_t   *obj;
    int             width;
    int             height;
    bool            checked;       /* tried to load already */
    lyric_entry_t  *entries;
    size_t          count;
    ssize_t         current;       /* -1 if no entry is current */
};

lyrics_overlay_t *lyrics_overlay_New(vlc_object_t *obj, int width, int height)
{
    lyrics_overlay_t *o = calloc(1, sizeof(*o));
    if (!o)
        return NULL;
    o->obj = obj;
    o->width = width;
    o->height = height;
    o->current = -1;
    return o;
}

void lyrics_overlay_Delete(lyrics_overlay_t *o)
{
    if (!o)
        return;
    for (size_t i = 0; i < o->count; i++)
        free(o->entries[i].psz_text);
    free(o->entries);
    free(o);
}

/* Parse the "sylt-data" string emitted by SyltEmitMeta() in es.c.
 * Entries are packed as "ms\x1Ftext\x1E". */
static void lyrics_overlay_Load(lyrics_overlay_t *o)
{
    char *psz_data = var_InheritString(o->obj, "sylt-data");
    if (psz_data == NULL || *psz_data == '\0')
    {
        free(psz_data);
        return;
    }

    size_t cap = 0;
    const char *p = psz_data;
    while (*p != '\0')
    {
        const char *sep = strchr(p, '\x1F');
        const char *end = strchr(p, '\x1E');
        if (sep == NULL || end == NULL || sep >= end)
            break;
        char *endptr = NULL;
        long long ms = strtoll(p, &endptr, 10);
        if (endptr != sep)
            break;
        size_t text_len = end - (sep + 1);
        if (o->count == cap)
        {
            size_t new_cap = cap ? cap * 2 : 64;
            lyric_entry_t *grown = realloc(o->entries,
                                           new_cap * sizeof(*grown));
            if (!grown)
                break;
            o->entries = grown;
            cap = new_cap;
        }
        lyric_entry_t *e = &o->entries[o->count];
        e->i_start  = VLC_TICK_FROM_MS(ms);
        e->psz_text = strndup(sep + 1, text_len);
        if (!e->psz_text)
            break;
        o->count++;
        p = end + 1;
    }

    msg_Dbg(o->obj, "loaded %zu lyric entries for visualizer overlay",
            o->count);
    free(psz_data);
}

void lyrics_overlay_Update(lyrics_overlay_t *o, vout_thread_t *vout,
                           vlc_tick_t i_pts)
{
    if (!o || !vout)
        return;

    if (!var_InheritBool(o->obj, "visual-show-lyrics"))
    {
        /* Toggled off: drop any displayed lyric immediately. */
        if (o->current != -1)
        {
            vout_FlushSubpictureChannel(vout, VOUT_SPU_CHANNEL_OSD);
            o->current = -1;
        }
        return;
    }

    if (!o->checked)
    {
        lyrics_overlay_Load(o);
        o->checked = true;
    }
    if (o->count == 0 || i_pts == VLC_TICK_INVALID)
        return;

    /* Binary-search the last entry whose start <= i_pts */
    ssize_t lo = 0, hi = (ssize_t)o->count - 1, found = -1;
    while (lo <= hi)
    {
        ssize_t mid = (lo + hi) / 2;
        if (o->entries[mid].i_start <= i_pts)
        {
            found = mid;
            lo = mid + 1;
        }
        else
            hi = mid - 1;
    }
    if (found < 0 || found == o->current)
        return;
    o->current = found;

    const lyric_entry_t *e = &o->entries[found];
    if (!e->psz_text || *e->psz_text == '\0')
        return;

    subpicture_t *subpic = subpicture_New(NULL);
    if (!subpic)
        return;
    /* OSD channel auto-clears the previous subpicture, so each new lyric
     * replaces the prior one without an explicit flush. */
    subpic->i_channel = VOUT_SPU_CHANNEL_OSD;
    subpic->i_original_picture_width  = o->width;
    subpic->i_original_picture_height = o->height;
    subpic->i_start = vlc_tick_now();
    vlc_tick_t duration;
    if ((size_t)found + 1 < o->count)
        duration = o->entries[found + 1].i_start - e->i_start;
    else
        duration = VLC_TICK_FROM_SEC(8);
    if (duration < VLC_TICK_FROM_MS(200))
        duration = VLC_TICK_FROM_MS(200);
    subpic->i_stop = subpic->i_start + duration;
    subpic->b_ephemer = true;
    subpic->b_fade    = true;
    subpic->b_subtitle = false;

    subpicture_region_t *r = subpicture_region_NewText();
    if (!r)
    {
        subpicture_Delete(subpic);
        return;
    }
    r->fmt.i_sar_num = 1;
    r->fmt.i_sar_den = 1;
    r->p_text = text_segment_New(e->psz_text);
    r->i_align = SUBPICTURE_ALIGN_BOTTOM;
    r->text_flags |= SUBPICTURE_ALIGN_BOTTOM;
    r->b_absolute = false;
    r->b_in_window = false;
    r->i_x = 0;
    r->i_y = (int)(o->height * 0.05f);
    vlc_spu_regions_push(&subpic->regions, r);

    vout_PutSubpicture(vout, subpic);
}
