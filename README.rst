============
vmod_cookie
============

----------------------
Varnish Cookie Module
----------------------

:Author: Lasse Karstensen
:Date: 2012-04-03
:Version: 1.0
:Manual section: 3

SYNOPSIS
========

import cookie;

DESCRIPTION
===========

Functions to handle the content of the Cookie header without complex use of
regular expressions.

Reads the req.http.cookie header, ie it only considers incoming cookies from
the client.

Any Set-Cookie header from the backend is currently ignored.

FUNCTIONS
=========

Prototyping stage, the current functionality is planned:

cookie.get_string()
cookie.get_int()

# future
# cookie.set_int("cookiename", 13);
# cookie.set_string("cookiename", "c is for..");
# set req.http.cookie = cookie.extract();


get_string
-----

Prototype
        ::

                get_string(STRING S)
Return value
	STRING
Description
	Get string value of cookie S.
Example
        ::

                set resp.http.X-sessionid = cookie.get_string("session_id")

INSTALLATION
============

This is an example skeleton for developing out-of-tree Varnish
vmods. It implements the "Hello, World!" as a vmod callback. Not
particularly useful in good hello world tradition, but demonstrates how
to get the glue around a vmod working.

The source tree is based on autotools to configure the building, and
does also have the necessary bits in place to do functional unit tests
using the varnishtest tool.

Usage::

 ./configure VARNISHSRC=DIR [VMODDIR=DIR]

`VARNISHSRC` is the directory of the Varnish source tree for which to
compile your vmod. Both the `VARNISHSRC` and `VARNISHSRC/include`
will be added to the include search paths for your module.

Optionally you can also set the vmod install directory by adding
`VMODDIR=DIR` (defaults to the pkg-config discovered directory from your
Varnish installation).

Make targets:

* make - builds the vmod
* make install - installs your vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``

In your VCL you could then use this vmod along the following lines::

        import example;

        sub vcl_deliver {
                # This sets resp.http.hello to "Hello, World"
                set resp.http.hello = example.hello("World");
        }

HISTORY
=======

This manual page was released as part of the libvmod-example package,
demonstrating how to create an out-of-tree Varnish vmod.

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-example project. See LICENSE for details.

* Copyright (c) 2011 Varnish Software
