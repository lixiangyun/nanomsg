/*
    Copyright (c) 2012-2013 250bpm s.r.o.

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

#include "xreq.h"

#include "../../nn.h"
#include "../../reqrep.h"

#include "../../utils/err.h"
#include "../../utils/cont.h"
#include "../../utils/fast.h"
#include "../../utils/alloc.h"

/*  Private functions. */
static void nn_xreq_destroy (struct nn_sockbase *self);

static const struct nn_sockbase_vfptr nn_xreq_sockbase_vfptr = {
    nn_xreq_destroy,
    nn_xreq_add,
    nn_xreq_rm,
    nn_xreq_in,
    nn_xreq_out,
    nn_xreq_send,
    nn_xreq_recv,
    nn_xreq_setopt,
    nn_xreq_getopt,
    nn_xreq_sethdr,
    nn_xreq_gethdr
};

void nn_xreq_init (struct nn_xreq *self, const struct nn_sockbase_vfptr *vfptr,
    int fd)
{
    nn_sockbase_init (&self->sockbase, vfptr, fd);
    nn_excl_init (&self->excl);
}

void nn_xreq_term (struct nn_xreq *self)
{
    nn_excl_term (&self->excl);
}

static void nn_xreq_destroy (struct nn_sockbase *self)
{
    struct nn_xreq *xreq;

    xreq = nn_cont (self, struct nn_xreq, sockbase);

    nn_xreq_term (xreq);
    nn_free (xreq);
}

int nn_xreq_add (struct nn_sockbase *self, struct nn_pipe *pipe)
{
    return nn_excl_add (&nn_cont (self, struct nn_xreq, sockbase)->excl, pipe);
}

void nn_xreq_rm (struct nn_sockbase *self, struct nn_pipe *pipe)
{
    nn_excl_rm (&nn_cont (self, struct nn_xreq, sockbase)->excl, pipe);
}

int nn_xreq_in (struct nn_sockbase *self, struct nn_pipe *pipe)
{
    return nn_excl_in (&nn_cont (self, struct nn_xreq, sockbase)->excl, pipe);
}

int nn_xreq_out (struct nn_sockbase *self, struct nn_pipe *pipe)
{
    return nn_excl_out (&nn_cont (self, struct nn_xreq, sockbase)->excl, pipe);
}

int nn_xreq_send (struct nn_sockbase *self, struct nn_msg *msg)
{
    int rc;

    /*  If request cannot be sent due to the pushback, drop it silenly. */
    rc = nn_excl_send (&nn_cont (self, struct nn_xreq, sockbase)->excl, msg);
    if (rc == -EAGAIN) {
        nn_msg_term (msg);
        return 0;
    }
    return rc;
}

int nn_xreq_recv (struct nn_sockbase *self, struct nn_msg *msg)
{
    int rc;

    rc = nn_excl_recv (&nn_cont (self, struct nn_xreq, sockbase)->excl, msg);
    if (rc == -EAGAIN)
        return -EAGAIN;
    errnum_assert (rc > 0, -rc);

    if (!(rc & NN_PIPE_PARSED)) {

        /*  Ignore malformed replies. */
        if (nn_slow (nn_chunkref_size (&msg->body) < sizeof (uint32_t))) {
            nn_msg_term (msg);
            return -EAGAIN;
        }

        /*  Split the message into the header and the body. */
        nn_assert (nn_chunkref_size (&msg->hdr) == 0);
        nn_chunkref_term (&msg->hdr);
        nn_chunkref_init (&msg->hdr, sizeof (uint32_t));
        memcpy (nn_chunkref_data (&msg->hdr), nn_chunkref_data (&msg->body),
            sizeof (uint32_t));
        nn_chunkref_trim (&msg->body, sizeof (uint32_t));
    }

    return 0;
}

int nn_xreq_setopt (struct nn_sockbase *self, int level, int option,
    const void *optval, size_t optvallen)
{
    return -ENOPROTOOPT;
}

int nn_xreq_getopt (struct nn_sockbase *self, int level, int option,
    void *optval, size_t *optvallen)
{
    return -ENOPROTOOPT;
}

int nn_xreq_sethdr (struct nn_msg *msg, const void *hdr, size_t hdrlen)
{
    /*  TODO */
    nn_assert (0);
}

int nn_xreq_gethdr (struct nn_msg *msg, void *hdr, size_t *hdrlen)
{
    /*  TODO */
    nn_assert (0);
}

static struct nn_sockbase *nn_xreq_create (int fd)
{
    struct nn_xreq *self;

    self = nn_alloc (sizeof (struct nn_xreq), "socket (xreq)");
    alloc_assert (self);
    nn_xreq_init (self, &nn_xreq_sockbase_vfptr, fd);
    return &self->sockbase;
}

static struct nn_socktype nn_xreq_socktype_struct = {
    AF_SP_RAW,
    NN_REQ,
    nn_xreq_create
};

struct nn_socktype *nn_xreq_socktype = &nn_xreq_socktype_struct;

