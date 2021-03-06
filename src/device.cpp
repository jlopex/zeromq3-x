/*
    Copyright (c) 2007-2011 iMatix Corporation
    Copyright (c) 2007-2011 Other contributors as noted in the AUTHORS file

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stddef.h>

#include "platform.hpp"

#if defined ZMQ_FORCE_SELECT
#define ZMQ_POLL_BASED_ON_SELECT
#elif defined ZMQ_FORCE_POLL
#define ZMQ_POLL_BASED_ON_POLL
#elif defined ZMQ_HAVE_LINUX || defined ZMQ_HAVE_FREEBSD ||\
    defined ZMQ_HAVE_OPENBSD || defined ZMQ_HAVE_SOLARIS ||\
    defined ZMQ_HAVE_OSX || defined ZMQ_HAVE_QNXNTO ||\
    defined ZMQ_HAVE_HPUX || defined ZMQ_HAVE_AIX ||\
    defined ZMQ_HAVE_NETBSD
#define ZMQ_POLL_BASED_ON_POLL
#elif defined ZMQ_HAVE_WINDOWS || defined ZMQ_HAVE_OPENVMS ||\
     defined ZMQ_HAVE_CYGWIN
#define ZMQ_POLL_BASED_ON_SELECT
#endif

//  On AIX platform, poll.h has to be included first to get consistent
//  definition of pollfd structure (AIX uses 'reqevents' and 'retnevents'
//  instead of 'events' and 'revents' and defines macros to map from POSIX-y
//  names to AIX-specific names).
#if defined ZMQ_POLL_BASED_ON_POLL
#include <poll.h>
#endif

#include "../include/zmq.h"

#include "device.hpp"
#include "socket_base.hpp"
#include "likely.hpp"
#include "err.hpp"

int zmq::device (class socket_base_t *insocket_,
        class socket_base_t *outsocket_)
{
    msg_t msg;
    int rc = msg.init ();
    if (rc != 0)
        return -1;

    //  The algorithm below assumes ratio of requests and replies processed
    //  under full load to be 1:1.

    //  TODO: The current implementation drops messages when
    //  any of the pipes becomes full.

    int more;
    size_t moresz;
    zmq_pollitem_t items [] = {
        { insocket_, 0, ZMQ_POLLIN, 0 },
        { outsocket_,  0, ZMQ_POLLIN, 0 }
    };
    while (true) {
        //  Wait while there are either requests or replies to process.
        rc = zmq_poll (&items [0], 2, -1);
        if (unlikely (rc < 0))
            return -1;

        //  Process a request.
        if (items [0].revents & ZMQ_POLLIN) {
            while (true) {
                rc = insocket_->recv (&msg, 0);
                if (unlikely (rc < 0))
                    return -1;

                moresz = sizeof more;
                rc = insocket_->getsockopt (ZMQ_RCVMORE, &more, &moresz);
                if (unlikely (rc < 0))
                    return -1;

                rc = outsocket_->send (&msg, more? ZMQ_SNDMORE: 0);
                if (unlikely (rc < 0))
                    return -1;
                if (more == 0)
                    break;
            }
        }
        //  Process a reply.
        if (items [1].revents & ZMQ_POLLIN) {
            while (true) {
                rc = outsocket_->recv (&msg, 0);
                if (unlikely (rc < 0))
                    return -1;

                moresz = sizeof more;
                rc = outsocket_->getsockopt (ZMQ_RCVMORE, &more, &moresz);
                if (unlikely (rc < 0))
                    return -1;

                rc = insocket_->send (&msg, more? ZMQ_SNDMORE: 0);
                if (unlikely (rc < 0))
                    return -1;
                if (more == 0)
                    break;
            }
        }

    }
    return 0;
}
