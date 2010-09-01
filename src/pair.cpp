/*
    Copyright (c) 2007-2010 iMatix Corporation

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../include/zmq.h"

#include "pair.hpp"
#include "err.hpp"
#include "pipe.hpp"

zmq::pair_t::pair_t (class ctx_t *parent_, uint32_t slot_) :
    socket_base_t (parent_, slot_),
    inpipe (NULL),
    outpipe (NULL),
    inpipe_alive (false),
    outpipe_alive (false),
    terminating (false)
{
    options.requires_in = true;
    options.requires_out = true;
}

zmq::pair_t::~pair_t ()
{
    zmq_assert (!inpipe);
    zmq_assert (!outpipe);
}

void zmq::pair_t::xattach_pipes (class reader_t *inpipe_,
    class writer_t *outpipe_, const blob_t &peer_identity_)
{
    zmq_assert (!inpipe && !outpipe);

    inpipe = inpipe_;
    inpipe_alive = true;
    inpipe->set_event_sink (this);

    outpipe = outpipe_;
    outpipe_alive = true;
    outpipe->set_event_sink (this);

    if (terminating) {
        register_term_acks (2);
        inpipe_->terminate ();
        outpipe_->terminate ();
    }
}

void zmq::pair_t::terminated (class reader_t *pipe_)
{
    zmq_assert (pipe_ == inpipe);
    inpipe = NULL;
    inpipe_alive = false;

    if (terminating)
        unregister_term_ack ();
}

void zmq::pair_t::terminated (class writer_t *pipe_)
{
    zmq_assert (pipe_ == outpipe);
    outpipe = NULL;
    outpipe_alive = false;

    if (terminating)
        unregister_term_ack ();
}

void zmq::pair_t::process_term ()
{
    zmq_assert (inpipe && outpipe);

    terminating = true;

    register_term_acks (2);
    inpipe->terminate ();
    outpipe->terminate ();

    socket_base_t::process_term ();
}

void zmq::pair_t::activated (class reader_t *pipe_)
{
    zmq_assert (!inpipe_alive);
    inpipe_alive = true;
}

void zmq::pair_t::activated (class writer_t *pipe_)
{
    zmq_assert (!outpipe_alive);
    outpipe_alive = true;
}

int zmq::pair_t::xsend (zmq_msg_t *msg_, int flags_)
{
    if (outpipe == NULL || !outpipe_alive) {
        errno = EAGAIN;
        return -1;
    }

    if (!outpipe->write (msg_)) {
        outpipe_alive = false;
        errno = EAGAIN;
        return -1;
    }

    if (!(flags_ & ZMQ_SNDMORE))
        outpipe->flush ();

    //  Detach the original message from the data buffer.
    int rc = zmq_msg_init (msg_);
    zmq_assert (rc == 0);

    return 0;
}

int zmq::pair_t::xrecv (zmq_msg_t *msg_, int flags_)
{
    //  Deallocate old content of the message.
    zmq_msg_close (msg_);

    if (!inpipe_alive || !inpipe || !inpipe->read (msg_)) {

        //  No message is available.
        inpipe_alive = false;

        //  Initialise the output parameter to be a 0-byte message.
        zmq_msg_init (msg_);
        errno = EAGAIN;
        return -1;
    }
    return 0;
}

bool zmq::pair_t::xhas_in ()
{
    if (!inpipe || !inpipe_alive)
        return false;

    inpipe_alive = inpipe->check_read ();
    return inpipe_alive;
}

bool zmq::pair_t::xhas_out ()
{
    if (!outpipe || !outpipe_alive)
        return false;

    outpipe_alive = outpipe->check_write ();
    return outpipe_alive;
}

