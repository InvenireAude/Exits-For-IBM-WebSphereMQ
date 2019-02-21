
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
Configuration
In order to actvate the exit, create definiton in the queue manager qm.ini file, e.g::

ApiExitLocal:
  Name=CarbonCopy
  Sequence=3
  Function=EntryPoint
  Module=/var/mqm/exits/iaeapicc
  Data=D=Y,p=TRC.,A=n

The Data string contains coma separated options.
Parameter	Description
D	Specifies the verbosity level.
Y - verbose mode.
y - Basic information.
n/N - Quiet - nothing.
g	Specifies the queue prefix for the MQGET tracing.
p	Specifies the queue prefix for the MQPUT tracing.
E	Set expiration to unlimited.
Y/y - yes.
n/N - no.
P	Change presistence (otherwise the orginal setting is used).
0 - non-presistant trace message.
1 - presistant trace message.
2 - as queue default.
A	Specifies alias base names are to be used.
Y/y - use the base name.
n/N - the application provided name.
If you want to trace only one type of calls, simply do not include the remaining prefix specification.
Remember that the trace queue must be defined and enabled for the put oprations in order to activate tracing.

Restart the queue anager.

Verify the AMQERR01.LOG file:

tail -f /var/mqm/qmgrs/ABC/errors/AMQERR01.LOG
Verify the exit file:
tail -f /var/mqm/iae/errors/iae.PID.txt
The error log is composed of the program PID.
Exit and debuging works with multithreaded programs. However, we do fully not support the IBM WebSphere Message Broker.

Define the trace queue, e.g.:
 DEF QL(TRC.SERVICE.IN) PUT(DISABLED) MAXDEPTH(50000) MAXMSGL(100000)
Activate the trace by altering the queue option:
 ALTER QL(TRC.SERVICE.IN) PUT(ENABLED)
Remember that the queue should be able to store the trace messages (proper MAXDEPTH) or the backup utility should be configured (e.g. the qload program).
Storing large number of messages on the queue (> 10000) may yield in the periodic, time consuming queue indexing process (can be observed as the messages AMQERRxx.LOG files).

Compilation (e.g for Linux-64bit)
=========================================================
Use gmake and pick the Makefile version matching your system, e.g.:

	gmake clean
	gmake -f Makefile.Linux-gcc all

More info:

  http://www.invenireaude.com/article;id=exits.cc



