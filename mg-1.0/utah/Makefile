#=======================================================================
# Update the mg fullword index files (usually in a nightly cron job).
# [18-May-1994]
#=======================================================================

EGREP		= /bin/egrep

FIND		= /bin/find

MGDATA		= $(HOME)/mgdata

MGBIN		= nice time /usr/local/bin

RM		= /bin/rm -f

SED		= /bin/sed

SHELL		= /bin/sh

SORT		= /bin/sort

#=======================================================================

all:	bibfiles texfiles

bibfiles:	bibfiles.lst
	MGDATA=$(MGDATA) $(MGBIN)/mgbuild -g ./mg_get bibfiles
	MGDATA=$(MGDATA) $(MGBIN)/mg_fast_comp_dict -f bibfiles/bibfiles
	-$(RM) $(MGDATA)/bibfiles/bibfiles.invf.dict \
		  $(MGDATA)/bibfiles/bibfiles.invf.idx \
		  $(MGDATA)/bibfiles/bibfiles.text.stats \
		  $(MGDATA)/bibfiles/bibfiles.text.idx \
		  $(MGDATA)/bibfiles/bibfiles.weight \
		  $(MGDATA)/bibfiles/bibfiles.invf.chunk \
		  $(MGDATA)/bibfiles/bibfiles.invf.chunk.trans \
		  $(MGDATA)/bibfiles/bibfiles.invf.dict.hash

bibfiles.lst:	FRC
	$(FIND) $(HOME) -follow -name '*.bib' -print | $(SORT) | \
		$(SED) -e 's_^[.]/_$(HOME)/_' > bibfiles.lst

clean mostlyclean:
	-$(RM) *.i
	-$(RM) *.o
	-$(RM) *~
	-$(RM) \#*
	-$(RM) a.out
	-$(RM) core

clobber distclean realclean:	clean
	-$(RM) $(MGDATA)/bibfiles $(MGDATA)/texfiles

FRC:

texfiles:	texfiles.lst
	MGDATA=$(MGDATA) $(MGBIN)/mgbuild -g ./mg_get texfiles
	MGDATA=$(MGDATA) $(MGBIN)/mg_fast_comp_dict -f texfiles/texfiles
	-$(RM) $(MGDATA)/texfiles/texfiles.invf.dict \
		  $(MGDATA)/texfiles/texfiles.invf.idx \
		  $(MGDATA)/texfiles/texfiles.text.stats \
		  $(MGDATA)/texfiles/texfiles.text.idx \
		  $(MGDATA)/texfiles/texfiles.weight \
		  $(MGDATA)/texfiles/texfiles.invf.chunk \
		  $(MGDATA)/texfiles/texfiles.invf.chunk.trans \
		  $(MGDATA)/texfiles/texfiles.invf.dict.hash

texfiles.lst:	FRC
	$(FIND) $(HOME) -follow -name '*.??x' -print | \
		$(EGREP) 'tex$$|ltx$$|stx$$|atx$$' | \
		$(SORT) | \
		$(SED) -e 's_^[.]/_$(HOME)/_' > texfiles.lst
