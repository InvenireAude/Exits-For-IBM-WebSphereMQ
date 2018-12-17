
=====================================================================
Message Channel Exit : Sniffer (File Logger) for IBM WebSphere MQ
=====================================================================

 Prerequisites

===============
Before you compile the software you will need at least the following software:

    IBM WebSphere MQ V7.0 or higher (Linux, AIX (vac) or other *nix based system)


 Overview
 =========

The message exit overrides the MQGET and MQGET callback calls and duplicates messages
to the trace queues. If the queue is not accessible the copy is not made and MQGET
proceeds as usual.

You can define the trace queues for all object names that you plan to trace. Setup them
with the PUT(DISABLE) option by default and enable this setting  when the tracing is needed.

Installation

Create the errors and data directories:

mkdir -p  /var/mqm/iae/errors
chmod 770 /var/mqm/iae/errors
chmod +t  /var/mqm/iae/errors

The above directory names are fixed as there are no means to pass so much configuration to the exit. If you would like to have the data saved in other location create the symbolic links, accordingly.

Remember to setup the permissions according to your security policy, the directory must be accessible for MCA users.

This exit does not write any user data (message content) into the error log files.
Configuration
In order to actvate the exit, change the channel definition, e.g::


--------------------------------------------------------------------
 Parameter 	Description
--------------------------------------------------------------------
	D 		Specifies the verbosity level.

    		D - verbose mode.
    		d - Basic information.
    		n/N - Quite - nothing.

------------------------------------------------------------------



Compilation (e.g for Linux-64bit)
=========================================================
Use gmake and pick the Makefile version matching your system, e.g.:

	gmake clean
	gmake -f Makefile.Linux-gcc all

More info:

  http://www.invenireaude.com/article;id=exits.cc



