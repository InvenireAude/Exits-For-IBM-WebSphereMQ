
notarget:
	@echo Specify one of the targets: all, clean.

#########################################################################

EXE=out/bin/iae_chfl_getdata
EXIT=out/exit/iaechfl

#########################################################################
all: makedirs $(EXE) $(EXIT)
	@echo All done, here are your artifacts:
	@ls -l out/*/*
#########################################################################

clean:
	@echo Cleaning ...
	@rm -rf out
	
#########################################################################
makedirs:
	@echo Ensuring output directories.
	@mkdir -p out/bin
	@mkdir -p out/exit
