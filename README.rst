============
vmod_cookie
============

----------------------
Varnish Cookie Module
----------------------

:Author: Lasse Karstensen
:Date: 2013-07-12
:Version: 1.0
:Manual section: 3

SYNOPSIS
========

import cookie;

DESCRIPTION
===========

Functions to handle the content of the Cookie header without complex use of
regular expressions.

Parses a cookie header into an internal data store, where per-cookie
get/set/delete functions are available. A filter_except() method removes all
but a set comma-separated list of cookies.

FUNCTIONS
=========

parse
-----

Prototype
        ::

                parse(STRING S)
Return value
	VOID
Description
	Parse the cookie string in string S. Implicit clean() if run twice
	during one request.
Example
        ::

		sub vcl_recv {
			cookie.parse(req.http.Cookie);
		}


clean
-----

Prototype
        ::

                clean()
Return value
	VOID
Description
	Clean up all previously parse()-d cookies. Probably of limited
	use. It is not necessary to run clean() in normal operation.
Example
        ::

		sub vcl_recv {
			cookie.clean();
		}

get
-----

Prototype
        ::

                get(STRING cookiename)
Return value
	STRING
Description
	Get the value of a cookie, as stored in internal vmod storage.

Example
        ::

		import std;
		sub vcl_recv {
			cookie.parse("cookie1: value1; cookie2: value2;");
			std.log("cookie1 value is: " + cookie.get("cookie1"));
		}

set
----

Prototype
        ::

                set(STRING cookiename, STRING cookievalue)
Return value
	VOID
Description
	Set the internal vmod storage value for a cookie to a value.

Example
        ::

		sub vcl_recv {
			cookie.set("cookie1", "value1");
			std.log("cookie1 value is: " + cookie.get("cookie1"));
		}


delete
------

Prototype
        ::

                delete(STRING cookiename)
Return value
	VOID
Description
	Delete a cookie from internal vmod storage if it exists.

Example
        ::

		sub vcl_recv {
			cookie.parse("cookie1: value1; cookie2: value2;");
			cookie.delete("cookie2");
			// get_string() will now yield "cookie1: value1";
		}


filter_except
-------------

Prototype
        ::

                filter_except(STRING cookienames)
Return value
	VOID
Description
	Delete all cookies from internal vmod storage that is not in the
	comma-separated argument cookienames.

Example
        ::

		sub vcl_recv {
			cookie.parse("cookie1: value1; cookie2: value2; cookie3: value3");
			cookie.filter_except("cookie1,cookie2");
			// get_string() will now yield
			// "cookie1: value1; cookie2: value2;";
		}



get_string
----------

Prototype
        ::

                get_string()
Return value
	STRING
Description
	Get a Cookie string value with all cookies in internal vmod storage.
Example
        ::

		sub vcl_recv {
			cookie.parse(req.http.cookie);
			cookie.filter_except("SESSIONID,PHPSESSID");
			set req.http.cookie = cookie.get_string();
		}

INSTALLATION
============

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
* make install - installs the vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``

In your VCL you could then use this vmod along the following lines::

	import cookie;
	sub vcl_recv {
		cookie.parse(req.http.cookie);
		cookie.filter_except("SESSIONID,PHPSESSID");
		set req.http.cookie = cookie.get_string();
	}


HISTORY
=======

This manual page was released as part of the libvmod-example package,
demonstrating how to create an out-of-tree Varnish vmod.

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-example project. See LICENSE for details.

* Copyright (c) 2011-2013 Varnish Software
