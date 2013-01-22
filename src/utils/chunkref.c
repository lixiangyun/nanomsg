/*
    Copyright (c) 2013 250bpm s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom
    the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

#include "chunkref.h"
#include "err.h"

#include <string.h>

/*  nn_chunkref should be reinterpreted as this structure in case the first
    byte ('tag') is 0xff. */
struct nn_chunkref_chunk {
    uint8_t tag;
    struct nn_chunk *chunk;
};

/*  Check whether VSM are small enough for size to fit into the first byte
    of the structure. */
CT_ASSERT (NN_CHUNKREF_MAX < 255);

/*  Check whether nn_chunkref_chunk fits into nn_chunkref. */
CT_ASSERT (sizeof (struct nn_chunkref) >= sizeof (struct nn_chunkref_chunk));

void nn_chunkref_init (struct nn_chunkref *self, size_t size)
{
    struct nn_chunkref_chunk *ch;

    if (size < NN_CHUNKREF_MAX) {
        self->ref [0] = (uint8_t) size;
        return;
    }

    ch = (struct nn_chunkref_chunk*) self;
    ch->tag = 0xff;
    ch->chunk = nn_chunk_alloc (size, 0);
    alloc_assert (ch->chunk);
}

void nn_chunkref_init_chunk (struct nn_chunkref *self, struct nn_chunk *chunk)
{
    struct nn_chunkref_chunk *ch;

    ch = (struct nn_chunkref_chunk*) self;
    ch->tag = 0xff;
    ch->chunk = chunk;
}

void nn_chunkref_term (struct nn_chunkref *self)
{
    struct nn_chunkref_chunk *ch;

    if (self->ref [0] == 0xff) {
        ch = (struct nn_chunkref_chunk*) self;
        nn_chunk_free (ch->chunk);
    }
}

struct nn_chunk *nn_chunkref_getchunk (struct nn_chunkref *self)
{
    struct nn_chunkref_chunk *ch;
    struct nn_chunk *chunk;

    if (self->ref [0] == 0xff) {
        ch = (struct nn_chunkref_chunk*) self;
        self->ref [0] = 0;
        return ch->chunk;
    }

    chunk = nn_chunk_alloc (self->ref [0], 0);
    alloc_assert (chunk);
    memcpy (nn_chunk_data (chunk), &self->ref [1], self->ref [0]);
    self->ref [0] = 0;
    return chunk;
}

void nn_chunkref_mv (struct nn_chunkref *dst, struct nn_chunkref *src)
{
    memcpy (dst, src, src->ref [0] == 0xff ?
        sizeof (struct nn_chunkref_chunk) : src->ref [0] + 1);
}

void nn_chunkref_cp (struct nn_chunkref *dst, struct nn_chunkref *src)
{
    /*  TODO: At the moment, copy is made. Do it via reference count. */
    nn_chunkref_init (dst, nn_chunkref_size (src));
    memcpy (nn_chunkref_data (dst), nn_chunkref_data (src),
        nn_chunkref_size (src));
}

void *nn_chunkref_data (struct nn_chunkref *self)
{
    return self->ref [0] == 0xff ?
        nn_chunk_data (((struct nn_chunkref_chunk*) self)->chunk) :
        &self->ref [1];
}

size_t nn_chunkref_size (struct nn_chunkref *self)
{
    return self->ref [0] == 0xff ?
        nn_chunk_size (((struct nn_chunkref_chunk*) self)->chunk) :
        self->ref [0];
}

void nn_chunkref_trim (struct nn_chunkref *self, size_t n)
{
    struct nn_chunkref_chunk *ch;

    if (self->ref [0] == 0xff) {
        ch = (struct nn_chunkref_chunk*) self;
        ch->chunk = nn_chunk_trim (ch->chunk, n);
        return;
    }

    nn_assert (self->ref [0] >= n);
    memmove (&self->ref [1], &self->ref [1 + n], self->ref [0] - n);
    self->ref [0] -= n;
}

