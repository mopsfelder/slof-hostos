# *****************************************************************************
# * Copyright (c) 2004, 2007 IBM Corporation
# * All rights reserved.
# * This program and the accompanying materials
# * are made available under the terms of the BSD License
# * which accompanies this distribution, and is available at
# * http://www.opensource.org/licenses/bsd-license.php
# *
# * Contributors:
# *     IBM Corporation - initial implementation
# ****************************************************************************/

include make.rules

STD_BOARDS = $(shell targets=""; \
		for a in `echo board-*`; do \
			if [ -e $$a/config ]; then \
				targets="$$targets $$a"; \
			else \
				cd $$a; \
				for b in `echo config* | sed -e s/config.//g`; do \
					if [ "X$$b" != "Xsimics" ]; then \
						if [ "X$$b" != "X`echo $$a|sed -e s/board-//g`" ]; then \
							targets="$$targets $$a-$$b"; \
						else \
							targets="$$targets $$b"; \
					fi fi \
				done; \
				cd ..; \
			fi; \
		done; \
		echo $$targets | sed -e s/board-//g)

all:
	@if [ ! -f .target ]; then \
		echo "Please specify a build target:"; \
		echo "  $(STD_BOARDS)"; \
		exit 1; \
	fi
	@make `cat .target`

rom:
	@echo "******* Build $(BOARD) System ********"
	@echo $(BOARD) > .target
	@make -C board-$(BOARD)
	@$(RM) -f .crc_flash
rw:
	@echo "******* Build $(BOARD) system (RISCWatch boot) ********"
	@echo $(BOARD) > .target
	@make -C board-$(BOARD) l2b
	@$(RM) -f .crc_flash

$(STD_BOARDS):
	@echo "******** Building $@ system ********"
	@if [ -f .target ]; then \
		if [ `cat .target` != $@ ]; then \
			echo "Configuration changed - cleaning up first..."; \
			make distclean; \
			echo $@ > .target; \
		fi; \
	else \
		echo $@ > .target; \
	fi
	@b=`echo $@ | grep "-"`; \
	if [ -n "$$b" ]; then \
		subboard=$${b##*-}; \
		board=$${b%%-*}; \
		make -C board-$$board SUBBOARD=$$subboard; \
	else \
		make -C board-$@; \
	fi
	@$(RM) .crc_flash

test_all:
	@for i in $(STD_BOARDS); do make distclean $$i; done

driver:
		DRIVER=1 make -C board-$(BOARD) driver
		@$(RM) -f .crc_flash .boot_xdr.ffs
cli:
		make -C clients

# Rules for making clean:
clean_here:
		rm -f boot_rom.bin .boot_rom.ffs boot_xdr.bin .boot_xdr.ffs
		rm -f boot_l2-dd2.ad boot_l2b.bin .crc_flash


clean:		clean_here
		@if [ -e .target ]; then \
			tar=`cat .target`; \
			b=`echo $$tar | grep "-"`; \
			if [ -n "$$b" ]; then \
				subboard=$${b##*-}; \
				board=$${b%%-*}; \
				make -C board-$$board SUBBOARD=$$subboard clean; \
			else \
				pwd; \
				make -C board-$$tar clean; \
			fi \
		fi

distclean:	clean_here
		@if [ -e .target ]; then \
			tar=`cat .target`; \
			b=`echo $$tar | grep "-"`; \
			if [ -n "$$b" ]; then \
				subboard=$${b##*-}; \
				board=$${b%%-*}; \
				make -C board-$$board SUBBOARD=$$subboard distclean; \
			else \
				make -C board-$$tar distclean; \
			fi; \
			rm -f .target; \
		fi

distclean_all:	clean_here
		@for dir in board-* ; do \
			$(MAKE) -C $$dir distclean || exit 1; \
		done
		rm -f .target

cli-clean:
		make -C clients clean
