# Makefile.amiga for Amiga Bourne Compatible Shell.
#

srcdir = .

CC = gcc
CPP = $(CC) -E

DEFS = -DAMIGA -DPOSIXLY_CORRECT -DAUTOINIT -D__sys_stdtypes_h
LIBS = -lunix -lauto -lnet -lstack

CPPFLAGS = 
CFLAGS = -g -O
LDSTATIC = 
LDFLAGS = 

SHELL_PROG = sh

prefix = /gcc/bin

# Suffix for executables: nothing for AmigaOS.
exe_suffix=

SHELL = /bin/sh

SRCS = alloc.c amigaos.c c_ksh.c c_sh.c c_test.c c_ulimit.c \
	environ.c eval.c exec.c expr.c history.c io.c jobs.c lex.c \
	main.c misc.c missing.c path.c shf.c sigact.c syn.c table.c trap.c \
	tree.c tty.c var.c version.c
OBJS = alloc.o amigaos.o c_ksh.o c_sh.o c_test.o c_ulimit.o \
	environ.o eval.o exec.o expr.o history.o io.o jobs.o lex.o \
	main.o misc.o missing.o path.o shf.o sigact.o syn.o table.o trap.o \
	tree.o tty.o var.o version.o
HDRS = c_test.h expand.h ksh_dir.h ksh_limval.h ksh_stat.h ksh_time.h \
	ksh_times.h ksh_wait.h lex.h proto.h sh.h shf.h sigact.h \
	table.h tree.h tty.h
DISTFILES = $(SRCS) $(HDRS) ksh.Man Makefile \
	mkinstalldirs install-sh new-version.sh siglist.h  siglist.in siglist.sh \
	mkman check-fd.c check-pgrp.c check-sigs.c \
	README NEWS CONTRIBUTORS LEGAL PROJECTS INSTALL NOTES BUG-REPORTS \
	IAFA-PACKAGE ChangeLog ChangeLog.0
# ETCFILES also disted, but handled differently
ETCFILES = etc/ksh.kshrc etc/profile etc/sys_config.sh
# TESTFILES also disted, but handled differently
TESTFILES = tests/README tests/th tests/th-sh tests/alias.t tests/arith.t \
	tests/bksl-nl.t tests/brkcont.t tests/cdhist.t tests/eglob.t \
	tests/glob.t tests/heredoc.t tests/history.t tests/ifs.t \
	tests/integer.t tests/lineno.t tests/read.t tests/regress.t \
	tests/syntax.t tests/unclass1.t tests/unclass2.t \
	tests/version.t 

all: $(SHELL_PROG)$(exe_suffix) $(SHELL_PROG).1

$(SHELL_PROG)$(exe_suffix): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

.c.o:
	$(CC) -c $(CPPFLAGS) $(DEFS) -I. -I$(srcdir) $(CFLAGS) $<

clean:
	rm -f ksh$(exe_suffix) sh$(exe_suffix) ksh.1 sh.1 $(OBJS) siglist.out \
	      emacs.out core a.out mon.out gmon.out \
	      version.c.bak Makefile.bak Makefile.tmp check-fd$(exe_suffix) \
	      check-pgrp$(exe_suffix) check-sigs$(exe_suffix)

mostlyclean: clean

distclean: clean
	rm -f tags TAGS *~

depend: $(SRCS)
	sed -n '1,/[ ]PUT ANYTHING BELOW THIS LINE/p' < Makefile > Makefile.tmp
	srcs=; for i in $(SRCS) ; do srcs="$$srcs $(srcdir)/$$i"; done; \
	  $(CC) -M $(DEFS) -I. -I$(srcdir) $(CFLAGS) $$srcs | \
	    sed -e 's?[ 	]/[^ 	]*??g' -e 's?[ 	]./? ?g' \
		-e 's?[ 	]$(srcdir)//*? ?g' -e 's?^$(srcdir)//*??' \
		-e '/^[ 	]*\\[	 ]*$$/d' -e '/^[^:]*:[	 ]*$$/d' \
		-e 's/^\([	 ]*\)$$/ sh.h/' \
	    >> Makefile.tmp
	mv Makefile.tmp Makefile
	@echo 'Make depend done (stopping make)'; false

# DON'T PUT ANYTHING BELOW THIS LINE (and don't delete it - its for make depend)
alloc.o: alloc.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h
amigaos.o: amigaos.c sh.h
c_ksh.o: c_ksh.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h sh.h
c_sh.o: c_sh.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h ksh_time.h \
 ksh_times.h sh.h
c_test.o: c_test.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h c_test.h
c_ulimit.o: c_ulimit.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_time.h sh.h
environ.o: environ.c
eval.o: eval.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_dir.h ksh_stat.h
exec.o: exec.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h c_test.h ksh_stat.h
expr.o: expr.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h sh.h
history.o: history.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h
io.o: io.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h
jobs.o: jobs.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h ksh_wait.h \
 ksh_times.h ksh_time.h \
 tty.h sh.h
lex.o: lex.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h sh.h
main.o: main.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h ksh_time.h \
 sh.h
misc.o: misc.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h sh.h
missing.o: missing.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h ksh_dir.h \
 ksh_time.h ksh_times.h sh.h
path.o: path.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h
shf.o: shf.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h ksh_limval.h \
 sh.h
sigact.o: sigact.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h
syn.o: syn.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h c_test.h
table.o: table.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h
trap.o: trap.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h siglist.h
tree.o: tree.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h
tty.o: tty.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_stat.h tty.h sh.h
var.o: var.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h ksh_time.h ksh_limval.h \
 ksh_stat.h sh.h
version.o: version.c sh.h \
 shf.h table.h tree.h expand.h lex.h \
 proto.h
