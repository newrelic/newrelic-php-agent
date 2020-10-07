/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nr_axiom.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include "util_logging.h"
#include "util_memory.h"
#include "util_signals.h"
#include "util_syscalls.h"

#include "tlib_main.h"

typedef void (*nr_signaller_t)(int signaller_arg);

static void nr_test_signals_do_kill(nr_signaller_t signaller,
                                    int signaller_arg) {
  if (0 != signaller) {
    signaller(signaller_arg);
  }
}

static const char* signal_name(int sig);

/*
 * Test the handlers for segmentation violation.
 *
 * Since we can't recover from a segmentation violation in a portable
 * manner, we use a SIGUSR1 instead, and assume that sending and receipt
 * of SIGUSR1 from our process back to ourselves is synchronous.
 */

/*
 * WATCH OUT! It is tempting to call nrl_error or other logging
 * functions from these signal handler functions, but those functions
 * may call malloc, which is not allowed in signal handlers. The thread
 * sanitizer discovers this, and will complain about it.
 */

static void sigsegv_signal_tracer(int sig) {
  nr_signal_tracer_common(sig);
}

#if defined(TEST_DODGY_SIGNALS) /* { */
static void sigfpe_signal_tracer(int sig) {
  nr_signal_tracer_common(sig);
}

static jmp_buf sigfpe_real_signal_tracer_jmp_buf;
static void sigfpe_real_signal_tracer(int sig) {
  nr_signal_tracer_common(sig);
  longjmp(sigfpe_real_signal_tracer_jmp_buf, 1);
}
#endif /* } */

static void send_signal(int signal_to_send) {
  kill(nr_getpid(),
       signal_to_send); /* assumes synchronous send and receive to self */
}

#if defined(TEST_DODGY_SIGNALS) /* { */
static int nr_test_signals_zero(void) {
  return 0;
}

static void do_integer_zero_divide(int arg NRUNUSED) {
  int x = 0;

  nrl_send_log_message(NRL_ALWAYS, "before integer zero divide x=%d", x);
  (void)x;
  x = 1 / nr_test_signals_zero();
  nrl_send_log_message(NRL_ALWAYS, "after  integer zero divide x=%d", x);
}

static void do_integer_corner_divide(int arg NRUNUSED) {
  int x = 0;

  (void)x;
  nrl_send_log_message(NRL_ALWAYS, "before integer INT_MIN / -1 divide x=%d",
                       x);
  x = INT_MIN
      / (nr_test_signals_zero()
         - 1); /* eg, INT_MIN / -1 , which isn't representable */
  nrl_send_log_message(NRL_ALWAYS, "after  integer INT_MIN / -1 divide x=%d",
                       x);
}
#endif /* } */

/*
 * Execute backtrace and related just for the side effect
 * of loading the DSO that does that, so that we don't call
 * malloc from a signal handler.
 */
static void test_signals_prime_backtrace(void) {
#ifdef HAVE_BACKTRACE
  void* array[100];
  size_t size;
  int fd;

  size = backtrace(array, sizeof(array) / sizeof(array[0]));
  fd = nr_open("/dev/null", 0, 0666);
  backtrace_symbols_fd(array, size, fd);
  nr_close(fd);
  fd = -1;
#endif
}

static const char* signal_name(int sig) {
  switch (sig) {
#ifdef SIGHUP
    case SIGHUP:
      return "SIGHUP";
#endif
#ifdef SIGINT
    case SIGINT:
      return "SIGINT";
#endif
#ifdef SIGQUIT
    case SIGQUIT:
      return "SIGQUIT";
#endif
#ifdef SIGILL
    case SIGILL:
      return "SIGILL";
#endif
#ifdef SIGTRAP
    case SIGTRAP:
      return "SIGTRAP";
#endif
#ifdef SIGABRT
    case SIGABRT:
      return "SIGABRT";
#endif
#ifdef SIGEMT
    case SIGEMT:
      return "SIGEMT";
#endif
#ifdef SIGFPE
    case SIGFPE:
      return "SIGFPE";
#endif
#ifdef SIGKILL
    case SIGKILL:
      return "SIGKILL";
#endif
#ifdef SIGBUS
    case SIGBUS:
      return "SIGBUS";
#endif
#ifdef SIGSEGV
    case SIGSEGV:
      return "SIGSEGV";
#endif
#ifdef SIGSYS
    case SIGSYS:
      return "SIGSYS";
#endif
#ifdef SIGPIPE
    case SIGPIPE:
      return "SIGPIPE";
#endif
#ifdef SIGALRM
    case SIGALRM:
      return "SIGALRM";
#endif
#ifdef SIGTERM
    case SIGTERM:
      return "SIGTERM";
#endif
#ifdef SIGURG
    case SIGURG:
      return "SIGURG";
#endif
#ifdef SIGSTOP
    case SIGSTOP:
      return "SIGSTOP";
#endif
#ifdef SIGTSTP
    case SIGTSTP:
      return "SIGTSTP";
#endif
#ifdef SIGCONT
    case SIGCONT:
      return "SIGCONT";
#endif
#ifdef SIGCHLD
    case SIGCHLD:
      return "SIGCHLD";
#endif
#ifdef SIGTTIN
    case SIGTTIN:
      return "SIGTTIN";
#endif
#ifdef SIGTTOU
    case SIGTTOU:
      return "SIGTTOU";
#endif
#ifdef SIGIO
    case SIGIO:
      return "SIGIO";
#endif
#ifdef SIGXCPU
    case SIGXCPU:
      return "SIGXCPU";
#endif
#ifdef SIGXFSZ
    case SIGXFSZ:
      return "SIGXFSZ";
#endif
#ifdef SIGVTALRM
    case SIGVTALRM:
      return "SIGVTALRM";
#endif
#ifdef SIGPROF
    case SIGPROF:
      return "SIGPROF";
#endif
#ifdef SIGWINCH
    case SIGWINCH:
      return "SIGWINCH";
#endif
#ifdef SIGLOST
    case SIGLOST:
      return "SIGLOST";
#endif
#ifdef SIGUSR1
    case SIGUSR1:
      return "SIGUSR1";
#endif
#ifdef SIGUSR2
    case SIGUSR2:
      return "SIGUSR2";
#endif
#ifdef SIGPWR
    case SIGPWR:
      return "SIGPWR";
#endif
#if defined(__APPLE__)
#ifdef SIGPOLL
    case SIGPOLL:
      return "SIGPOLL";
#endif
#endif
#ifdef SIGWIND
    case SIGWIND:
      return "SIGWIND";
#endif
#ifdef SIGPHONE
    case SIGPHONE:
      return "SIGPHONE";
#endif
#ifdef SIGWAITING
    case SIGWAITING:
      return "SIGWAITING";
#endif
#ifdef SIGLWP
    case SIGLWP:
      return "SIGLWP";
#endif
#ifdef SIGDANGER
    case SIGDANGER:
      return "SIGDANGER";
#endif
#ifdef SIGGRANT
    case SIGGRANT:
      return "SIGGRANT";
#endif
#ifdef SIGRETRACT
    case SIGRETRACT:
      return "SIGRETRACT";
#endif
#ifdef SIGMSG
    case SIGMSG:
      return "SIGMSG";
#endif
#ifdef SIGSOUND
    case SIGSOUND:
      return "SIGSOUND";
#endif
#ifdef SIGSAK
    case SIGSAK:
      return "SIGSAK";
#endif
#ifdef SIGPRIO
    case SIGPRIO:
      return "SIGPRIO";
#endif
#ifdef SIG33
    case SIG33:
      return "SIG33";
#endif
#ifdef SIG34
    case SIG34:
      return "SIG34";
#endif
#ifdef SIG35
    case SIG35:
      return "SIG35";
#endif
#ifdef SIG36
    case SIG36:
      return "SIG36";
#endif
#ifdef SIG37
    case SIG37:
      return "SIG37";
#endif
#ifdef SIG38
    case SIG38:
      return "SIG38";
#endif
#ifdef SIG39
    case SIG39:
      return "SIG39";
#endif
#ifdef SIG40
    case SIG40:
      return "SIG40";
#endif
#ifdef SIG41
    case SIG41:
      return "SIG41";
#endif
#ifdef SIG42
    case SIG42:
      return "SIG42";
#endif
#ifdef SIG43
    case SIG43:
      return "SIG43";
#endif
#ifdef SIG44
    case SIG44:
      return "SIG44";
#endif
#ifdef SIG45
    case SIG45:
      return "SIG45";
#endif
#ifdef SIG46
    case SIG46:
      return "SIG46";
#endif
#ifdef SIG47
    case SIG47:
      return "SIG47";
#endif
#ifdef SIG48
    case SIG48:
      return "SIG48";
#endif
#ifdef SIG49
    case SIG49:
      return "SIG49";
#endif
#ifdef SIG50
    case SIG50:
      return "SIG50";
#endif
#ifdef SIG51
    case SIG51:
      return "SIG51";
#endif
#ifdef SIG52
    case SIG52:
      return "SIG52";
#endif
#ifdef SIG53
    case SIG53:
      return "SIG53";
#endif
#ifdef SIG54
    case SIG54:
      return "SIG54";
#endif
#ifdef SIG55
    case SIG55:
      return "SIG55";
#endif
#ifdef SIG56
    case SIG56:
      return "SIG56";
#endif
#ifdef SIG57
    case SIG57:
      return "SIG57";
#endif
#ifdef SIG58
    case SIG58:
      return "SIG58";
#endif
#ifdef SIG59
    case SIG59:
      return "SIG59";
#endif
#ifdef SIG60
    case SIG60:
      return "SIG60";
#endif
#ifdef SIG61
    case SIG61:
      return "SIG61";
#endif
#ifdef SIG62
    case SIG62:
      return "SIG62";
#endif
#ifdef SIG63
    case SIG63:
      return "SIG63";
#endif
#ifdef SIGCANCEL
    case SIGCANCEL:
      return "SIGCANCEL";
#endif
#ifdef SIG32
    case SIG32:
      return "SIG32";
#endif
#ifdef SIG64
    case SIG64:
      return "SIG64";
#endif
#ifdef SIG65
    case SIG65:
      return "SIG65";
#endif
#ifdef SIG66
    case SIG66:
      return "SIG66";
#endif
#ifdef SIG67
    case SIG67:
      return "SIG67";
#endif
#ifdef SIG68
    case SIG68:
      return "SIG68";
#endif
#ifdef SIG69
    case SIG69:
      return "SIG69";
#endif
#ifdef SIG70
    case SIG70:
      return "SIG70";
#endif
#ifdef SIG71
    case SIG71:
      return "SIG71";
#endif
#ifdef SIG72
    case SIG72:
      return "SIG72";
#endif
#ifdef SIG73
    case SIG73:
      return "SIG73";
#endif
#ifdef SIG74
    case SIG74:
      return "SIG74";
#endif
#ifdef SIG75
    case SIG75:
      return "SIG75";
#endif
#ifdef SIG76
    case SIG76:
      return "SIG76";
#endif
#ifdef SIG77
    case SIG77:
      return "SIG77";
#endif
#ifdef SIG78
    case SIG78:
      return "SIG78";
#endif
#ifdef SIG79
    case SIG79:
      return "SIG79";
#endif
#ifdef SIG80
    case SIG80:
      return "SIG80";
#endif
#ifdef SIG81
    case SIG81:
      return "SIG81";
#endif
#ifdef SIG82
    case SIG82:
      return "SIG82";
#endif
#ifdef SIG83
    case SIG83:
      return "SIG83";
#endif
#ifdef SIG84
    case SIG84:
      return "SIG84";
#endif
#ifdef SIG85
    case SIG85:
      return "SIG85";
#endif
#ifdef SIG86
    case SIG86:
      return "SIG86";
#endif
#ifdef SIG87
    case SIG87:
      return "SIG87";
#endif
#ifdef SIG88
    case SIG88:
      return "SIG88";
#endif
#ifdef SIG89
    case SIG89:
      return "SIG89";
#endif
#ifdef SIG90
    case SIG90:
      return "SIG90";
#endif
#ifdef SIG91
    case SIG91:
      return "SIG91";
#endif
#ifdef SIG92
    case SIG92:
      return "SIG92";
#endif
#ifdef SIG93
    case SIG93:
      return "SIG93";
#endif
#ifdef SIG94
    case SIG94:
      return "SIG94";
#endif
#ifdef SIG95
    case SIG95:
      return "SIG95";
#endif
#ifdef SIG96
    case SIG96:
      return "SIG96";
#endif
#ifdef SIG97
    case SIG97:
      return "SIG97";
#endif
#ifdef SIG98
    case SIG98:
      return "SIG98";
#endif
#ifdef SIG99
    case SIG99:
      return "SIG99";
#endif
#ifdef SIG100
    case SIG100:
      return "SIG100";
#endif
#ifdef SIG101
    case SIG101:
      return "SIG101";
#endif
#ifdef SIG102
    case SIG102:
      return "SIG102";
#endif
#ifdef SIG103
    case SIG103:
      return "SIG103";
#endif
#ifdef SIG104
    case SIG104:
      return "SIG104";
#endif
#ifdef SIG105
    case SIG105:
      return "SIG105";
#endif
#ifdef SIG106
    case SIG106:
      return "SIG106";
#endif
#ifdef SIG107
    case SIG107:
      return "SIG107";
#endif
#ifdef SIG108
    case SIG108:
      return "SIG108";
#endif
#ifdef SIG109
    case SIG109:
      return "SIG109";
#endif
#ifdef SIG110
    case SIG110:
      return "SIG110";
#endif
#ifdef SIG111
    case SIG111:
      return "SIG111";
#endif
#ifdef SIG112
    case SIG112:
      return "SIG112";
#endif
#ifdef SIG113
    case SIG113:
      return "SIG113";
#endif
#ifdef SIG114
    case SIG114:
      return "SIG114";
#endif
#ifdef SIG115
    case SIG115:
      return "SIG115";
#endif
#ifdef SIG116
    case SIG116:
      return "SIG116";
#endif
#ifdef SIG117
    case SIG117:
      return "SIG117";
#endif
#ifdef SIG118
    case SIG118:
      return "SIG118";
#endif
#ifdef SIG119
    case SIG119:
      return "SIG119";
#endif
#ifdef SIG120
    case SIG120:
      return "SIG120";
#endif
#ifdef SIG121
    case SIG121:
      return "SIG121";
#endif
#ifdef SIG122
    case SIG122:
      return "SIG122";
#endif
#ifdef SIG123
    case SIG123:
      return "SIG123";
#endif
#ifdef SIG124
    case SIG124:
      return "SIG124";
#endif
#ifdef SIG125
    case SIG125:
      return "SIG125";
#endif
#ifdef SIG126
    case SIG126:
      return "SIG126";
#endif
#ifdef SIG127
    case SIG127:
      return "SIG127";
#endif
#ifdef SIGINFO
    case SIGINFO:
      return "SIGINFO";
#endif
#ifdef EXC_BAD_ACCESS
    case EXC_BAD_ACCESS:
      return "EXC_BAD_ACCESS";
#endif
#ifdef EXC_BAD_INSTRUCTION
    case EXC_BAD_INSTRUCTION:
      return "EXC_BAD_INSTRUCTION";
#endif
#ifdef EXC_ARITHMETIC
    case EXC_ARITHMETIC:
      return "EXC_ARITHMETIC";
#endif
#ifdef EXC_EMULATION
    case EXC_EMULATION:
      return "EXC_EMULATION";
#endif
#ifdef EXC_SOFTWARE
    case EXC_SOFTWARE:
      return "EXC_SOFTWARE";
#endif
#ifdef EXC_BREAKPOINT
    case EXC_BREAKPOINT:
      return "EXC_BREAKPOINT";
#endif
    default:
      return "?:";
  }
}

tlib_parallel_info_t parallel_info
    = {.suggested_nthreads = -1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  struct sigaction sa;
  nr_status_t rv;
  const char* actual_output_file_name = "logsignals.tmp";

  /*
   * Need to ensure we don't start out with existing log files from multiple
   * test runs.
   */
  nr_unlink(actual_output_file_name);

  (void)signal_name(SIGSEGV);
  test_signals_prime_backtrace();

  rv = nrl_set_log_file(actual_output_file_name);
  tlib_pass_if_true("log initialization succeeds", NR_SUCCESS == rv, "rv=%d",
                    (int)rv);
  tlib_pass_if_exists(actual_output_file_name);
  rv = nrl_send_log_message(NRL_ALWAYS, "expect PASS 1");
  tlib_pass_if_true("NRL_ALWAYS succeeds", NR_SUCCESS == rv, "rv=%d", (int)rv);

  nr_memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigsegv_signal_tracer;
  sigdelset(&sa.sa_mask, SIGUSR1);
  nr_signal_tracer_prep();

  sigaction(SIGUSR1, &sa, 0);
  nr_test_signals_do_kill(send_signal, SIGUSR1);
  tlib_pass_if_true("recovers from receipt of SIGUSR1 (recur 0)", 1, "deref");

  sigaction(SIGUSR1, &sa, 0);
  nr_test_signals_do_kill(send_signal, SIGUSR1);
  tlib_pass_if_true("recovers from receipt of SIGUSR1 (recur 10)", 1, "deref");

  sigaction(SIGUSR1, &sa, 0);
  nr_test_signals_do_kill(send_signal, SIGUSR1);
  tlib_pass_if_true("recovers from receipt of SIGUSR1 (recur 200)", 1, "deref");

  nrl_close_log_file();

  /*
   * Nothing is done with the log file.
   * It is too complex, and varies too much
   * between runs and across platforms.
   */
}
