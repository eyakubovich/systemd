/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2015 Tom Gundersen

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include "util.h"

#include "bitmap.h"

struct Bitmap {
        long long unsigned *bitmaps;
        size_t n_bitmaps;
        size_t bitmaps_allocated;
};

/* Bitmaps are only meant to store relatively small numbers
 * (corresponding to, say, an enum), so it is ok to limit
 * the max entry. 64k should be plenty. */
#define BITMAPS_MAX_ENTRY 0xffff

/* This indicates that we reached the end of the bitmap */
#define BITMAP_END ((unsigned) -1)

#define BITMAP_NUM_TO_OFFSET(n)           ((n) / (sizeof(long long unsigned) * 8))
#define BITMAP_NUM_TO_REM(n)              ((n) % (sizeof(long long unsigned) * 8))
#define BITMAP_OFFSET_TO_NUM(offset, rem) ((offset) * sizeof(long long unsigned) * 8 + (rem))

Bitmap *bitmap_new(void) {
        return new0(Bitmap, 1);
}

void bitmap_free(Bitmap *b) {
        if (!b)
                return;

        free(b->bitmaps);
        free(b);
}

int bitmap_ensure_allocated(Bitmap **b) {
        Bitmap *a;

        if (*b)
                return 0;

        a = bitmap_new();
        if (!a)
                return -ENOMEM;

        *b = a;

        return 0;
}

int bitmap_set(Bitmap *b, unsigned n) {
        long long unsigned bitmask;
        unsigned offset;

        assert(b);

        /* we refuse to allocate huge bitmaps */
        if (n > BITMAPS_MAX_ENTRY)
                return -ERANGE;

        offset = BITMAP_NUM_TO_OFFSET(n);

        if (offset >= b->n_bitmaps) {
                if (!GREEDY_REALLOC0(b->bitmaps, b->bitmaps_allocated, offset + 1))
                        return -ENOMEM;

                b->n_bitmaps = offset + 1;
        }

        bitmask = 1ULL << BITMAP_NUM_TO_REM(n);

        b->bitmaps[offset] |= bitmask;

        return 0;
}

void bitmap_unset(Bitmap *b, unsigned n) {
        long long unsigned bitmask;
        unsigned offset;

        assert(b);

        offset = BITMAP_NUM_TO_OFFSET(n);

        if (offset >= b->n_bitmaps)
                return;

        bitmask = 1ULL << BITMAP_NUM_TO_REM(n);

        b->bitmaps[offset] &= ~bitmask;
}

bool bitmap_isset(Bitmap *b, unsigned n) {
        long long unsigned bitmask;
        unsigned offset;

        if (!b || !b->bitmaps)
                return false;

        offset = BITMAP_NUM_TO_OFFSET(n);

        if (offset >= b->n_bitmaps)
                return false;

        bitmask = 1ULL << BITMAP_NUM_TO_REM(n);

        return !!(b->bitmaps[offset] & bitmask);
}

bool bitmap_isclear(Bitmap *b) {
        unsigned i;

        assert(b);

        for (i = 0; i < b->n_bitmaps; i++)
                if (b->bitmaps[i])
                        return false;

        return true;
}

void bitmap_clear(Bitmap *b) {
        unsigned i;

        assert(b);

        for (i = 0; i < b->n_bitmaps; i++)
                b->bitmaps[i] = 0;
}

bool bitmap_iterate(Bitmap *b, Iterator *i, unsigned *n) {
        long long unsigned bitmask;
        unsigned offset, rem;

        if (!b || i->idx == BITMAP_END)
                return false;

        offset = BITMAP_NUM_TO_OFFSET(i->idx);
        rem = BITMAP_NUM_TO_REM(i->idx);
        bitmask = 1ULL << rem;

        for (; offset < b->n_bitmaps; offset ++) {
                if (b->bitmaps[offset]) {
                        for (; bitmask; bitmask <<= 1, rem ++) {
                                if (b->bitmaps[offset] & bitmask) {
                                        *n = BITMAP_OFFSET_TO_NUM(offset, rem);
                                        i->idx = *n + 1;

                                        return true;
                                }
                        }
                }

                rem = 0;
                bitmask = 1;
        }

        i->idx = BITMAP_END;

        return false;
}

bool bitmap_equal(Bitmap *a, Bitmap *b) {
        unsigned i;

        if (!a ^ !b)
                return false;

        if (!a)
                return true;

        if (a->n_bitmaps != b->n_bitmaps)
                return false;

        for (i = 0; i < a->n_bitmaps; i++)
                if (a->bitmaps[i] != b->bitmaps[i])
                        return false;

        return true;
}
