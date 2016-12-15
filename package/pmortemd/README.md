# Purpose #

The pmortem (postmortem) package provides a vehicle for documenting
state of a program by dumping the contents of it's runtime map and a
portion of its stack.  The intent is to provide diagostic data for a
crashing program.

# Motivation #

Designed for Belkin WeMo software, as an aid to identifying program
crashes.  At the time of this development, the WeMo program
registered a SIGSEGV signal handler and simply printed a message
indicating that a segmentation fault had occurred.  The message was
printed by a function with a rather large stack frame using library
functions that are not safe in a signal handler.

While signal saftey is primarily a concern in a signal handler that
indends to resume an executing program, it is still possible to
inadvertently call library functions that will hang the signal handler
or corrupt another thread in the executing program.

This package aims to provide better diagnostics using only
async-signal-safe functions.  For a list of async-signal-safe
functions, see the man page for signal(7).

# Components #

The package consists of three parts: a daemon process (`pmortemd`), a
client library (`libpmortem.a`), and a post-processor script
(`stack-scrape`) to pick out interesting addresses in the stack dump.

## Daemon process ##

The daemon program (`/sbin/pmortemd`) runs in the background.  It opens
a UDS socket at `/tmp/pmortemd.socket` and waits for client
connections.  When a client program connects, it expects to receive a
process id, stack address, and a stream of bytes.  The process id and
stack values are binary data with a length of the native types for
`pid_t` and `void *` in native byte order.  (Both client and server
run on the same processor, so there's no need for abstract
serialization to adjust type sizes or byte ordering.)

The process id is used to print the contents of `/proc/<pid>/maps` to
allow the post processing script to associate addresses in the stack
data with symbols in the executable file and bound libraries.  The
stack address is only used to annotate output of the following byte
stream.

The byte stream that follows the stack address is assumed to be the
contents of the client program's stack at the time of the crash.  It
is printed in hex.

## Client Library ##

The client library (`/usr/lib/libpmortem.a`) provides a single entrypoint:

    void pmortem_connect_and_send(void *stack, size_t size);

The intent is that this will be the first thing a SIGSEGV signal
handler will call.  It connects to the daemon, writes the pid, stack
address and stack contents to the daemon and returns.  It delays
return until the daemon has acknowledged receipt of the data so that
the signal handler can prceed to _exit() or abort().

It is suggested that initially the size should be the system page size
(4 KB on most embedded Linux systems).  The stack size for most
threads on an uClibc based system is usually 8 KB.  The client
truncates the output at a page boundary to try to keep the send()
system call address range checking happy.  So passing 4K initially
will dump the most recent (possibly the only valid) data on the stack.
If in debugging, more stack is needed, development builds might
increase the size of the stack being dumped.

## Post Processing script ##

The post processing script (`stack-scrape`) is a ruby script intended
to run on a development system rather than the target.  It expects to
be passed a pointer to one or more directories, a pointer to the main
program, and the data that was output by the daemon (`/sbin/pmortemd`)
program.  The directories passed should include the toolchain lib
directory as well as a pointer to any shared object libraries used by
the program.  For example:

    ./package/pmortemd/src/stack-scrape \
	      -t /opt/toolchain-mipsel_gcc-4.3.3+cs_uClibc-0.9.33.2/ \
	      -t staging_dir/target-mipsel-openwrt-linux-uclibc/usr/lib \
	      -e build_dir/target-mipsel-openwrt-linux-uclibc_smart/pmortemd/ \
           /tmp/crasher.log 

The directories passed should point to unstripped executable and
libraries.  The script extracts symbols from those executables and
libraries to attempt to find interesting addresses in the dumped stack
data.

# Interpreting Postmortem Output #

This section suggests some pointers for interpreting the output of the
post processing script (`stack-scrape`).  They are useful as a
starting point for failure analysis.

The output of the daemon process consists of the the kernel's address
map for the failing process and a snapshot of the stack of the failing
thread.  The address map is mostly input to the `stack-scrap` script.
The stack dump has a few values worth inspecting before running the
script.

There is a sample log file and scraper output follow this narrative.

## Initial stack inspection ##

The first values to identify are the address where the segment fault
occurred and the last known return address.  On a `MIPS` little endian
system (`mipsel`) running Linux 2.6.21, the fault address is at a
stack offset of 32 bytes.  The RA register at the time of the fault is
at an offset of 288 (0xe0) bytes.

The RA will likely point to one of two locations in the executing
code.  It could be the return address for the function that caused the
crash.  Or it may contain a pointer to the instruction following the
last function call the crashing function issued.  In all cases, the RA
register (on `MIPSEL`) contains the address of a call (`bal`)
instruction plus eight bytes.  (The `bal` instruction is four bytes
long and the instruction following the `bal` is the "branch delay
slot".  The "branch delay slot" is executed while the `bal`
instruction is fetching the first instruction of the function being
called.)

## `stack-scrape` output ##

There are two sections of output from `stack-scrape`.  The first is a
pass through the stack data in the postmortem log.  Each value that
represents a program address is translated to a symbol/offset if the
symbol is available.  Or filename/offset if the symbol is not
available.

The second section is a summary of functions discovered in the first
pass.  The summary identifies the stack frame size (if available) and
the offset where that function's return address is written to the
stack frame (if available).

## Sample postmortem log ##

    root@OpenWrt:/tmp# ./crasher 
    stack: 0x7fc5c530-0x7fc5cd30
    00400000-00402000 r-xp 00000000 00:0a 4435       /tmp/crasher
    00411000-00412000 rw-p 00001000 00:0a 4435       /tmp/crasher
    2aaa8000-2aaaf000 r-xp 00000000 1f:02 314        /rom/lib/ld-uClibc-0.9.33.2.so
    2aaaf000-2aab1000 rw-p 2aaaf000 00:00 0 
    2aabe000-2aabf000 r--p 00006000 1f:02 314        /rom/lib/ld-uClibc-0.9.33.2.so
    2aabf000-2aac0000 rw-p 00007000 1f:02 314        /rom/lib/ld-uClibc-0.9.33.2.so
    2aac0000-2aad0000 r-xp 00000000 1f:02 331        /rom/lib/libgcc_s.so.1
    2aad0000-2aadf000 ---p 2aad0000 00:00 0 
    2aadf000-2aae0000 rw-p 0000f000 1f:02 331        /rom/lib/libgcc_s.so.1
    2aae0000-2ab49000 r-xp 00000000 1f:02 328        /rom/lib/libuClibc-0.9.33.2.so
    2ab49000-2ab59000 ---p 2ab49000 00:00 0 
    2ab59000-2ab5a000 r--p 00069000 1f:02 328        /rom/lib/libuClibc-0.9.33.2.so
    2ab5a000-2ab5b000 rw-p 0006a000 1f:02 328        /rom/lib/libuClibc-0.9.33.2.so
    2ab5b000-2ab60000 rw-p 2ab5b000 00:00 0 
    7fc48000-7fc5d000 rwxp 7fc48000 00:00 0          [stack]
    7fc5c9c8: 0000000b 00000000 00000000 00000000   24021017 0000000c 00000000 00000000
    7fc5c9e8: 00400cb8 00000000 00000000 00000000   2ab5d24d 00000000 00001239 00000000
    7fc5ca08: 2ab5d230 00000000 7fc5cc70 00000000   00000000 00000000 00000000 00000000
    7fc5ca28: 7fc5cccc 00000000 00000000 00000000   00000000 00000000 8021fc20 00000000
    7fc5ca48: 33356335 00000000 00000000 00000000   00000057 00000000 00000807 00000000
    7fc5ca68: 00410000 00000000 00001238 00000000   7fc5cec4 00000000 00400844 00000000
    7fc5ca88: 0047f3bc 00000000 0046bf38 00000000   00000008 00000000 2aab0444 00000000
    7fc5caa8: 0047f008 00000000 004113c8 00000000   2ab19480 00000000 00401255 00000000
    7fc5cac8: 00000000 00000000 2ab62430 00000000   7fc5cc60 00000000 ffffffff 00000000
    7fc5cae8: 00400cb8 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cb08: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cb28: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cb48: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cb68: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cb88: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cba8: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cbc8: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cbe8: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cc08: 00000000 00000000 00000002 00000000   00000000 00000000 00000000 00000000
    7fc5cc28: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cc48: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5cc68: 00000000 00000000 00001239 00000000   7fc5cc94 00400d0c 00000000 00000000
    7fc5cc88: 00000000 00000000 00001238 00000000   00000000 00000000 00000000 00000000
    7fc5cca8: 00000000 00000000 00000000 00000000   00000000 00000000 00000000 00000000
    7fc5ccc8: 00000000 00000000 00000001 00400d3c   00000000 00000000 00000000 00000000
    7fc5cce8: 00001237 00000000 00000000 00400d68   00000000 00000000 00000000 00000000
    7fc5cd08: 00001236 00000000 00000000 00400d94   00000000 00000000 00000000 00000000
    7fc5cd28: 00001235 00000000 00000001 00400eb4   7fc5cf75 7fc5cd98 00400844 0047f3bc
    7fc5cd48: 00000000 00000008 10010001 00400bbc   00000000 00000000 00000000 00000000
    7fc5cd68: 00000000 7fc5cf75 2ab62430 7fc5cf75   7fc5cd98 2ab41694 00000000 00000000
    7fc5cd88: ffffffff 00000000 2ab62430 00000000   00000000 00000000 00000000 00000000
    7fc5cda8: 00000000 00000000 00000003 00400034   00000004 00000020 00000005 00000008
    7fc5cdc8: 00000006 00001000 00000007 2aaa8000   00000008 00000000 00000009 00400a20
    7fc5cde8: 00000000 00000000 0000000b 00000000   0000000c 00000000 0000000d 00000000
    7fc5ce08: 0000000e 00000000 2ab41654 7fc5cd80   7fc5cf75 7fc5cd98 00400844 0047f3bc
    7fc5ce28: 0046bf38 00000008 2aab0444 0047f008   ffffffff 2ab62430 00000000 00000001
    7fc5ce48: 00000000 00000000 2aac7010 00000017   00000010 2aac7010 00400a20 0047f39c
    7fc5ce68: 0047f3bc 0046bf38 ffffffff 2aaae9f0   00000000 00000000 00000000 00000000
    7fc5ce88: 00400da0 00000001 2aac7010 00400a20   0047f39c 00400a68 00400da0 00000001
    7fc5cea8: 7fc5cec4 2aaa8f04 00401170 2aaa974c   7fc5cea0 7fc5cec0 00000001 7fc5cf75
    7fc5cec8: 00000000 7fc5cf7f 7fc5cf89 7fc5cf92   7fc5cf9d 7fc5cfad 7fc5cfb8 7fc5cfdb
    7fc5cee8: 7fc5cfe9 00000000 00000010 00000000   00000006 00001000 00000011 00000064
    7fc5cf08: 00000003 00400034 00000004 00000020   00000005 00000008 00000007 2aaa8000
    7fc5cf28: 00000008 00000000 00000009 00400a20   0000000b 00000000 0000000c 00000000
    7fc5cf48: 0000000d 00000000 0000000e 00000000   00000017 00000000 00000000 00000000
    7fc5cf68: 00000000 00000000 00000000 632f2e00   68736172 55007265 3d524553 746f6f72
    7fc5cf88: 444c4f00 3d445750 4f48002f 2f3d454d   746f6f72 31535000 40755c3d 5c3a685c
    7fc5cfa8: 20245c77 52455400 74763d4d 00323031   48544150 69622f3d 732f3a6e 3a6e6962
    7fc5cfc8: 7273752f 6e69622f 73752f3a 62732f72   53006e69 4c4c4548 69622f3d 68732f6e
    7fc5cfe8: 44575000 6d742f3d 2f2e0070 73617263   00726568 00000000 
    SIGSEGV [11].  Best guess fault address: 00400cb8, ra: 00400cb8, sig return: 0x7fc5c9d8

## Sample `stack-scrap` output

    [hugh@1b221f2f812a:smart]$ ./package/pmortemd/src/stack-scrape -t /opt/toolchain-mipsel_gcc-4.3.3+cs_uClibc-0.9.33.2/ -e build_dir/target-mipsel-openwrt-linux-uclibc_smart/pmortemd/ /tmp/crasher.log 
    00400cb8: level3 + 0x20 (/tmp/crasher)
    00400844: /tmp/crasher + 0x400844
    2ab19480: memset + 0x0 (/lib/libuClibc-0.9.33.2.so)
    00401255: /tmp/crasher + 0x401255
    00400cb8: level3 + 0x20 (/tmp/crasher)
    00400d0c: level2 + 0x40 (/tmp/crasher)
    00400d3c: level1 + 0x20 (/tmp/crasher)
    00400d68: level0 + 0x20 (/tmp/crasher)
    00400d94: start_crash + 0x20 (/tmp/crasher)
    00400eb4: main + 0x114 (/tmp/crasher)
    00400844: /tmp/crasher + 0x400844
    00400bbc: sig_segv + 0x0 (/tmp/crasher)
    2ab41694: __uClibc_main + 0x2cc (/lib/libuClibc-0.9.33.2.so)
    00400034: /tmp/crasher + 0x400034
    2aaa8000: /lib/ld-uClibc-0.9.33.2.so + 0x0
    00400a20: __start + 0x0 (/tmp/crasher)
    2ab41654: __uClibc_main + 0x28c (/lib/libuClibc-0.9.33.2.so)
    00400844: /tmp/crasher + 0x400844
    2aac7010: __mulsf3 + 0x100 (/lib/libgcc_s.so.1)
    2aac7010: __mulsf3 + 0x100 (/lib/libgcc_s.so.1)
    00400a20: __start + 0x0 (/tmp/crasher)
    2aaae9f0: /lib/ld-uClibc-0.9.33.2.so + 0x69f0
    00400da0: main + 0x0 (/tmp/crasher)
    2aac7010: __mulsf3 + 0x100 (/lib/libgcc_s.so.1)
    00400a20: __start + 0x0 (/tmp/crasher)
    00400a68: hlt + 0x0 (/tmp/crasher)
    00400da0: main + 0x0 (/tmp/crasher)
    2aaa8f04: _start + 0x54 (/lib/ld-uClibc-0.9.33.2.so)
    00401170: _fini + 0x0 (/tmp/crasher)
    2aaa974c: _dl_deallocate_tls + 0x118 (/lib/ld-uClibc-0.9.33.2.so)
    00400034: /tmp/crasher + 0x400034
    2aaa8000: /lib/ld-uClibc-0.9.33.2.so + 0x0
    00400a20: __start + 0x0 (/tmp/crasher)
    function                   frame size,  offset to RA
    _dl_deallocate_tls                  8,   44
    _start                             16,    0
    __mulsf3                           96,   92
    __uClibc_main                     288,  284
    memset                              0,    0
    __start                            32,    0
    _fini                              32,   28
    hlt                                 0,    0
    level0                             32,   28
    level1                             32,   28
    level2                             88,   84
    level3                             32,   28
    main                               72,   68
    sig_segv                           32,   28
    start_crash                        32,   28


## Sample analysis ##

The above two sections are derived from the sample/test program
`pmtest.c`.  That program passes control through a few functions
before intentionally crashing with a segmentation fault.

### Initial fault isolation ###

First of all, the crashing instruction address in 00400cb8 located at
address 7fc5c9e8.  The `stack-scraper` script translates 00400cb8 to
"level3 + 0x20".  And the RA register at 7fc5cae8 is also 00400cb8.
Looking at the output of `objdump`:

    /opt/toolchain-mipsel_gcc-4.3.3+cs_uClibc-0.9.33.2/usr/bin/mipsel-openwrt-linux-objdump -Sdr /home/hugh/belkin/git/smart/build_dir/target-mipsel-openwrt-linux-uclibc_smart/pmortemd/crasher
    
    00400c98 <level3>:
      400c98:       27bdffe0        addiu   sp,sp,-32
      400c9c:       afb00018        sw      s0,24(sp)
      400ca0:       24820001        addiu   v0,a0,1
      400ca4:       00808021        move    s0,a0
      400ca8:       27a40010        addiu   a0,sp,16
      400cac:       afbf001c        sw      ra,28(sp)
      400cb0:       0c1003b4        jal     400ed0 <nop>
      400cb4:       afa20010        sw      v0,16(sp)
      400cb8:       ae020000        sw      v0,0(s0)
      400cbc:       8fbf001c        lw      ra,28(sp)
      400cc0:       8fb00018        lw      s0,24(sp)
      400cc4:       03e00008        jr      ra
      400cc8:       27bd0020        addiu   sp,sp,32

we see that 400cb8 is a store word instruction following the branch
delay slot from the call to nop.  This corresponds to the source code
in pmtest.c:

    void level3(uint32_t arg)
    {
    	uint32_t value = arg + 1;
    
    	*(uint32_t *) arg = nop(&value);
    }

The unsigned integer `arg` is `0x1234`, as passed to `start_crash`
from `main`.

So in this case, the faulting address is the same as the RA address
because the fault occurs in a `bal` branch delay slot.

### Stack frame decomposition ###

Knowing that the crash occurred in function `level3`, and knowing that
`level3` was called from `level2` we can locate the stack frame for
`level3` by finding the return address on the stack.  The return
address (as printed by `stack-scrape` is `00400d0c` (`level2 + 0x40`).
That return address is at stack location 7fc5cc7c.  From the frame
size/offset to RA data printed at the end of `stack-scrape` we see
that `level3`'s stack frame is 32 bytes long with the RA (address in
`level2`) 28 bytes into the stack frame.  So `level3`'s stack frame is
the data dumped from 7fc5cc60 - 7fc55c80.

Similarly we decompose the stack frames for:

    level2       7fc5cc80 - 7fc5ccd8
	level1       7fc5ccd8 - 7fc5ccf8
	start-crash  7fc5ccf8 - 7fc5cd18
	main         7cc5cd18 - 7fc5cd60

### Caution ###

Not every stack trace is as easy to decompose.  Not every address on
the stack that translates to a program symbol is part of the current
backtrace.  For example, some addresses will point to functions that
have since returned upt the call stack and simply haven't been
overwritten by other data.

So it is often better to start with the bottom of the stack (`main +
0x114`) in this case and work toward the top of the stack.  In any
event, the interpreting contents of the stack requires some
concentration, experimentation, and analysis of both the stack data
and the output of objdump.
