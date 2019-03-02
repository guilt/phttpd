Pussy HTTPD
-----------

Pussy HTTPD is a lightweight, classic web server which can be used
to serve static web sites. It supports a minimum subset of the 
HTTP 1.0 and 1.1 protocols. It features keep-alives, IPv6 and
daemonization support. It is reasonably easy to use.

It was written in a few hours over a weekend, when the author
was betting that he could write one in a small amount of time.
It was meant to be a tiny version of an elaborate web server
like Tomcat. Hence the name. ;)

How to Build
------------

The distribution is very compact, and building the distribution is
very easy.

```
$ make -C netlibmini
$ make -C phttpd
```

That's it. Now to launch the server, execute phttpd_s. If you are
running on a privileged port, you may need to run as root.

`# ./phttpd_s`

A sample rc.phttpd is provided for those who wish to run the server
at init / startup.

PHTTPD - Simple
---------------

The phttpd_s is the simplest server to use. When run from a 
particular directory, it serves files from the htdocs/ folder. 
If a file is not specified in the URI, it uses index.html. It
does not do directory browsing, which is a feature, preventing
people from trespassing into sensitive content.

For example, if you are running a site called spam.com, a directory
structure would be like this:

```
htdocs/
       index.html                   Homepage
       images/                      Directory, not browsable
               logo.gif             Site logo
       users/                       Directory, not browsable
               index.html           Information about people
               bob/                 Bob's directory
                      index.html    Homepage
               alice/               Alice's directory
                      index.html    Homepage
                      favicon.ico   Alice's site icon
```

PHTTPD - Vhost
--------------

The phttpd_v server is a little more complex. It supports virtual
hosting, a way by which multiple web sites can run from the same
computer. When run from a particular directory, it serves files
from the vhosts/<hostname.com> folder. The rules for directory
browsing and default URI apply, as well. 

For all the IP Addresses/Hostnames that you want your server
accessed as, You are required to create directories/symbolic links
as well as maintain their content as well. 

For example, when hosting two sites called pam.com and sam.com, 
you will probably be using a structure like this:

```
vhosts/
        pam.com/                     
                index.html          Homepage of pam.com 
        sam.com/                     
                index.html          Homepage of sam.com 
                favicon.ico         sam.com's site icon
```

Limitations
-----------

The web servers are given away free, but with an accompanying
DISCLAIMER. That is to merely protect the author from being sued
(or exploited) but not an excuse to write lousy code. Any 
suggestions/bug reports/fixes would be most welcome.

Feedback
--------

* Author: Karthik Kumar Viswanathan
* Web   : http://karthikkumar.org
* Email : karthikkumar@gmail.com
