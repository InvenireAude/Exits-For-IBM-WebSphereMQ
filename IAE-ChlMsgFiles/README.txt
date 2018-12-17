
=====================================================================
Message Channel Exit : Sniffer (File Logger) for IBM WebSphere MQ
=====================================================================

 Prerequisites

===============
Before you compile the software you will need at least the following software:

    IBM WebSphere MQ V7.0 or higher (Linux, AIX or other *nix, if self complied)


 Overview
 =========

The message exit installs as the MCA extension and is called when a message is transmitted. The user data along with appropriate is stored in the active data log file. When appending a new message would exceed the specified threshold, the active file is renamed and the message is save into a fresh file.
Message Channel Exit - IBM WebSphere MQ
The message can operate on both receiving and sending end of the channel.
Installation
Create the errors and data directories:

mkdir -p /var/mqm/iae/data
mkdir -p /var/mqm/iae/errors
chmod 770 /var/mqm/iae/data
chmod 770 /var/mqm/iae/errors

The above directory names are fixed as there are no means to pass so much configuration to the exit. If you would like to have the data saved in other location create the symbolic links, accordingly.

Remember to setup the permissions according to your security policy, the directory must be accessible for MCA users.

This exit does not write any user data (message content) into the error log files.
Configuration
In order to actvate the exit, change the channel definition, e.g::

alter chl(MQA.TO.MQB) chltype(sdr) msgexit('iaechfl(iaechfl)')
alter chl(MQA.TO.MQB) chltype(sdr) msgdata('DN010')

The msgdata string follows this pattern DCXXX.
--------------------------------------------------------------------
 Parameter 	Description
--------------------------------------------------------------------
	D 		Specifies the verbosity level.

    		D - verbose mode.
    		d - Basic information.
    		n/N - Quite - nothing.

	C 	Specifies the error handling behaviour.

    		C/c - close the channel.
    		n/N - suppress the function.

	XXX 	Specifies the data log file maximum size (MB).

------------------------------------------------------------------

Start the channel.

Verify the AMQERR01.LOG file:

	tail -f /var/mqm/qmgrs/ABC/errors/AMQERR01.LOG

Verify the exit file:

	tail -f /var/mqm/iae/errors/ABC_ABC.TO.XYZ.123412.txt

The error log is composed of the queue manager name, the channel name, the MCA PID.


 Message Extraction
=====================

In order to extract the saved messages, use the iae_chfl_getdata program. The command line options allow you to select messages according to the following criteria.

  Usage: iae_cfe_getdata [options] <input_file>

	Options:
		 -h            This help.
		 -o <file>     Output file name.
		 -O <file>     Output file name (overwrite).
		 -s <string>   Search text.
		 -S <pattern>  Search text as regex pattern.
		 -m <string>   Destination queue manager name.
		 -M <string>   Destination queue manager name as regex pattern.
		 -q <string>   Destination queue name.
		 -Q <string>   Destination queue name as regex pattern.
		 -n <num>      Number of messages to extract.
		 -N <num>      Skip first <num> messages.
		 -D <data_format>   Data format, one of: .
  		                   1 - qload compatible (default) .
		                     2 - pure data only.



Compilation (e.g for Linux-64bit)
=========================================================
Use gmake !!!

	gmake clean
	gmake -f Makefile.Linux-64bit-gcc all

More info:

http://www.invenireaude.com/article;id=exits.cc


