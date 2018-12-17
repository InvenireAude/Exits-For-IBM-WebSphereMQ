
notarget:
	@echo Specify one of the targets: all, clean.

#########################################################################

EXIT32=out/exits/iaeapicc
EXIT32R=out/exits/iaeapicc_r
EXIT64=out/exits64/iaeapicc
EXIT64R=out/exits64/iaeapicc_r

#########################################################################
all: makedirs $(EXIT32) $(EXIT32R) $(EXIT64) $(EXIT64R)
	@echo All done, here are your artifacts:
	@ls -l out/*/*
#########################################################################

clean:
	@echo Cleaning ...
	@rm -rf out

#########################################################################
checkmake:

#########################################################################
makedirs:
	@echo Ensuring output directories.
	@mkdir -p out/exits
	@mkdir -p out/exits64

install: all
	@rm -f /var/mqm/exits/iaeapicc
	@rm -f /var/mqm/exits64/iaeapicc_r
	@rm -f /var/mqm/exits/iaeapicc
	@rm -f /var/mqm/exits64/iaeapicc_r
	@cp $(EXIT32)  /var/mqm/exits/iaeapicc
	@cp $(EXIT32R) /var/mqm/exits/iaeapicc_r
	@cp $(EXIT64)  /var/mqm/exits64/iaeapicc
	@cp $(EXIT64R) /var/mqm/exits64/iaeapicc_r
	@echo Installed:
	@ls -l /var/mqm/exits*/iaeapicc*
