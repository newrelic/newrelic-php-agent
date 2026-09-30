/*
 * Copyright 2026 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Transport contract tests for nr_cmd_{appinfo,txndata,span_batch}_tx.
 *
 * These link the real nr_agent.c (no stubs) and only observe the agent from
 * outside: they hand it a connection (nr_set_daemon_fd) or an address to
 * connect to, and inspect what arrives at the daemon's end. They therefore
 * hold for any connection design (shared fd, per-thread, pooled).
 *
 * Invariants:
 *   I1  A successful call writes exactly one whole frame (8-byte preamble
 *       plus body).
 *   I2  Success keeps the connection; the next call reuses it.
 *   I3  Failure closes the connection that failed; it is not reused.
 *   I4  A truncated frame is the last thing on its connection:
 *       frame* [partial] EOF.
 *   I5  A failure never closes a connection installed or made after the
 *       failing call took hold of its own.
 *   I6  Nothing is written to an fd that was closed and reused.
 *   I7  With no connection and no reachable daemon, a call fails fast and
 *       writes nothing.
 *   I8  An APPINFO reply is always the answer to its own request.
 *   I9  APPINFO replies UNKNOWN/DISCONNECTED/INVALID_LICENSE are not failures.
 *   I10 With no connection but a reachable daemon, the call connects and
 *       sends on the new connection.
 *
 * The stress tests (connection churn, the stress version of "a failure never
 * closes a newer connection", and fd reuse) can be run for longer with the
 * environment variables NR_TEST_TRANSPORT_CHURN_ITERS,
 * NR_TEST_TRANSPORT_FAILURE_STRESS_ITERS and NR_TEST_TRANSPORT_FD_REUSE_ITERS.
 * They rely on real thread interleaving, so under valgrind (which runs one
 * thread at a time) they can pass with the bug present. "A failure never
 * closes a newer connection" therefore also has a deterministic version that
 * holds a failing call in the window where the bug lives, and does not depend
 * on scheduling.
 */

#include "nr_axiom.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "nr_agent.h"
#include "nr_app.h"
#include "nr_app_private.h"
#include "nr_commands.h"
#include "nr_commands_private.h"
#include "nr_span_encoding.h"
#include "nr_txn.h"
#include "util_flatbuffers.h"
#include "util_logging.h"
#include "util_memory.h"
#include "util_network.h"
#include "util_threads.h"

#include "tlib_main.h"
#include "appinfo_reply_builders.h"

#define MS ((nrtime_t)NR_TIME_DIVISOR_MS)
#define A_LOAD(p) __atomic_load_n((p), __ATOMIC_SEQ_CST)
#define A_STORE(p, v) __atomic_store_n((p), (v), __ATOMIC_SEQ_CST)
#define A_ADD(p, v) __atomic_fetch_add((p), (v), __ATOMIC_SEQ_CST)

static char tmpdir[32];
static char live_path[64];
static char none_path[64];

/* ------------------------------------------------------------------ */
/* Address and connection setup                                        */
/* ------------------------------------------------------------------ */

static nrtime_t in_ms(unsigned ms) {
  return nr_get_time() + ms * MS;
}

static void set_address(const char* addr) {
  nr_conn_params_t* p = nr_conn_params_init(addr);

  nr_agent_initialize_daemon_connection_parameters(p);
  nr_conn_params_free(p);
}

/* Drop any connection and point the address at a path nobody listens on. */
static void reset_transport(void) {
  nr_set_daemon_fd(-1);
  set_address(none_path);
}

/* The agent owns socks[0] from now on; the test owns only socks[1]. */
static void install_pair(int socks[2]) {
  nbsockpair(socks);
  nr_set_daemon_fd(socks[0]);
}

static int listen_unix(const char* path) {
  struct sockaddr_un sa;
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);

  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_UNIX;
  strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);
  unlink(path);
  if (0 != bind(fd, (struct sockaddr*)&sa, sizeof(sa)) || 0 != listen(fd, 64)) {
    close(fd);
    return -1;
  }
  nr_network_set_non_blocking(fd);
  return fd;
}

static int listen_live(void) {
  int lfd = listen_unix(live_path);

  set_address(live_path);
  return lfd;
}

static void stop_listening(int lfd) {
  if (lfd >= 0) {
    close(lfd);
  }
  unlink(live_path);
}

/* Returns the accepted (non-blocking) fd, or -1 if none arrived in time. */
static int accept_within(int lfd, nrtime_t deadline) {
  for (;;) {
    nrtime_t now = nr_get_time();
    struct pollfd p = {.fd = lfd, .events = POLLIN};
    int fd;

    if (now < deadline) {
      poll(&p, 1, (int)((deadline - now) / MS) + 1);
    }
    fd = accept(lfd, NULL, NULL);
    if (fd >= 0) {
      nr_network_set_non_blocking(fd);
      return fd;
    }
    if (nr_get_time() >= deadline) {
      return -1;
    }
  }
}

/* ------------------------------------------------------------------ */
/* Reading what arrives at the daemon's end                            */
/* ------------------------------------------------------------------ */

typedef struct {
  const uint8_t* body;
  uint32_t len;
} frame_t;

typedef struct {
  nrbuf_t* buf; /* owns raw when the stream was read from an fd */
  const uint8_t* raw;
  size_t rawlen;
  frame_t* frames;
  size_t nframes;
  size_t partial;       /* bytes of an incomplete trailing frame */
  uint32_t partial_len; /* body length its preamble declares, if present */
  bool eof;
  bool bad_format;
} stream_t;

static bool wait_readable(int fd, nrtime_t deadline) {
  nrtime_t now = nr_get_time();
  struct pollfd p = {.fd = fd, .events = POLLIN};

  if (now >= deadline) {
    return false;
  }
  poll(&p, 1, (int)((deadline - now) / MS) + 1);
  return true;
}

static uint32_t le32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

/*
 * Splits s->raw into whole frames and a trailing partial frame. The framing
 * is the daemon's ReadMessage (daemon/internal/newrelic/listener.go): an
 * 8-byte preamble, uint32 length then uint32 format, both little-endian,
 * followed by length body bytes. It is deliberately independent of the
 * parser in util_network.c, which is part of the code under test.
 */
static void split_frames(stream_t* s) {
  size_t off = 0;

  while (off < s->rawlen) {
    uint32_t len;

    if (s->rawlen - off < 8) {
      s->partial = s->rawlen - off;
      break;
    }
    len = le32(s->raw + off);
    if (NR_PREAMBLE_FORMAT != le32(s->raw + off + 4)) {
      s->bad_format = true;
    }
    if (s->rawlen - off - 8 < len) {
      s->partial = s->rawlen - off;
      s->partial_len = len;
      break;
    }
    s->frames = nr_realloc(s->frames, (s->nframes + 1) * sizeof(frame_t));
    s->frames[s->nframes].body = s->raw + off + 8;
    s->frames[s->nframes].len = len;
    s->nframes++;
    off += 8 + len;
  }
}

/* Reads until EOF or the deadline, then splits the bytes into frames. */
static void read_stream(int fd, nrtime_t deadline, stream_t* s) {
  uint8_t tmp[65536];

  memset(s, 0, sizeof(*s));
  s->buf = nr_buffer_create(4096, 4096);
  for (;;) {
    ssize_t r = read(fd, tmp, sizeof(tmp));

    if (r > 0) {
      nr_buffer_add(s->buf, tmp, (int)r);
    } else if (0 == r || ECONNRESET == errno) {
      s->eof = true;
      break;
    } else if (EINTR == errno) {
      continue;
    } else if (EAGAIN == errno || EWOULDBLOCK == errno) {
      if (!wait_readable(fd, deadline)) {
        break;
      }
    } else {
      break;
    }
  }

  s->raw = nr_buffer_cptr(s->buf);
  s->rawlen = (size_t)nr_buffer_len(s->buf);
  split_frames(s);
}

static void stream_free(stream_t* s) {
  nr_buffer_destroy(&s->buf);
  nr_free(s->frames);
  memset(s, 0, sizeof(*s));
}

/* Drains what is left on fd and asserts it ends in EOF, not a partial frame. */
static void expect_eof(const char* what, int fd) {
  stream_t s;

  read_stream(fd, in_ms(2000), &s);
  tlib_pass_if_true(what, s.eof, "connection was not closed");
  tlib_pass_if_size_t_equal(what, 0, s.partial);
  stream_free(&s);
}

static void expect_open(const char* what, int fd) {
  char c;
  ssize_t r = read(fd, &c, 1);

  tlib_pass_if_true(what, -1 == r && (EAGAIN == errno || EWOULDBLOCK == errno),
                    "connection is not open and idle: r=%zd errno=%d", r,
                    errno);
}

static int frame_body_type(const frame_t* f) {
  nr_flatbuffers_table_t t;

  nr_flatbuffers_table_init_root(&t, f->body, f->len);
  return nr_flatbuffers_table_read_u8(&t, MESSAGE_FIELD_DATA_TYPE,
                                      MESSAGE_BODY_NONE);
}

/* ------------------------------------------------------------------ */
/* The three functions under test                                      */
/* ------------------------------------------------------------------ */

enum { FN_TXN = 0, FN_SPAN = 1 };
static const char* const fn_name[] = {"txndata", "span_batch"};

static size_t span_len(uint64_t seq) {
  return (seq > 0 && 0 == seq % 16) ? 128 * 1024 : 64 + (seq % 7) * 100;
}

static uint8_t span_fill(uint64_t tid, uint64_t seq) {
  return (uint8_t)(tid * 31 + seq * 17 + 1);
}

/* Payload = (tid, seq) followed by a fill byte derived from them. */
static void payload_make(nr_span_encoding_result_t* r,
                         size_t len,
                         uint64_t tid,
                         uint64_t seq) {
  r->data = nr_malloc(len);
  r->len = len;
  r->span_count = 1;
  memcpy(r->data, &tid, 8);
  memcpy(r->data + 8, &seq, 8);
  memset(r->data + 16, span_fill(tid, seq), len - 16);
}

static bool payload_decode(const frame_t* f,
                           uint64_t* tid,
                           uint64_t* seq,
                           size_t* len) {
  nr_flatbuffers_table_t t, d;
  const uint8_t* data;

  /* A garbled frame must not send the decoder off the end of the buffer. */
  if (f->len < 16 || le32(f->body) >= f->len) {
    return false;
  }
  nr_flatbuffers_table_init_root(&t, f->body, f->len);
  if (MESSAGE_BODY_SPAN_BATCH
          != nr_flatbuffers_table_read_u8(&t, MESSAGE_FIELD_DATA_TYPE, 0)
      || 0 == nr_flatbuffers_table_read_union(&d, &t, MESSAGE_FIELD_DATA)) {
    return false;
  }
  *len = nr_flatbuffers_table_read_vector_len(&d, SPAN_BATCH_FIELD_ENCODED);
  data = nr_flatbuffers_table_read_bytes(&d, SPAN_BATCH_FIELD_ENCODED);
  if (NULL == data || *len < 16 || *len > f->len) {
    return false;
  }
  memcpy(tid, data, 8);
  memcpy(seq, data + 8, 8);
  /* Every byte after the ids equals the fill byte. */
  return data[16] == span_fill(*tid, *seq)
         && (*len < 18 || 0 == memcmp(data + 16, data + 17, *len - 17));
}

static nr_status_t call_span(uint64_t tid, uint64_t seq, size_t len) {
  nr_span_encoding_result_t r;
  nr_status_t st;

  payload_make(&r, len, tid, seq);
  st = nr_cmd_span_batch_tx("run", &r);
  nr_free(r.data);
  return st;
}

static nr_status_t call_fn(int fn, uint64_t seq) {
  nrtxn_t txn;

  if (FN_SPAN == fn) {
    return call_span(0, seq, span_len(seq));
  }
  nr_memset(&txn, 0, sizeof(txn));
  return nr_cmd_txndata_tx(&txn);
}

static int fn_body_type(int fn) {
  return FN_SPAN == fn ? MESSAGE_BODY_SPAN_BATCH : MESSAGE_BODY_TXN;
}

static nrapp_t* app_new(void) {
  nrapp_t* app = nr_zalloc(sizeof(*app));

  nrt_mutex_init(&app->app_lock, 0);
  app->state = NR_APP_UNKNOWN;
  return app;
}

static void app_destroy(nrapp_t** app) {
  nrt_mutex_lock(&(*app)->app_lock); /* nr_app_destroy expects it locked */
  nr_app_destroy(app);
}

/* ------------------------------------------------------------------ */
/* The daemon's side of APPINFO                                        */
/* ------------------------------------------------------------------ */

typedef enum { PEER_REPLY, PEER_WRONG_TYPE, PEER_SILENT } peer_mode_t;

/* Returns true if a whole frame was read before EOF, error or the deadline. */
static bool read_frame(int fd, nrtime_t deadline) {
  nrbuf_t* buf = nr_network_receive(fd, deadline);
  bool ok = (NULL != buf);

  nr_buffer_destroy(&buf);
  return ok;
}

static void send_reply(int fd, peer_mode_t mode, int status, int variant) {
  nr_flatbuffer_t* fb;
  const char* json
      = APP_STATUS_CONNECTED == status ? "{\"agent_run_id\":\"run1\"}" : NULL;

  if (PEER_WRONG_TYPE == mode) {
    fb = nr_flatbuffers_create(0);
    nr_flatbuffers_object_begin(fb, MESSAGE_NUM_FIELDS);
    nr_flatbuffers_object_prepend_u8(fb, MESSAGE_FIELD_DATA_TYPE,
                                     MESSAGE_BODY_TXN, 0);
    nr_flatbuffers_finish(fb, nr_flatbuffers_object_end(fb));
  } else if (2 == variant) {
    fb = create_app_reply_two_fields("run1", (int8_t)status, json);
  } else if (3 == variant) {
    fb = create_app_reply_three_fields("run1", (int8_t)status, json, NULL);
  } else {
    fb = create_app_reply_six_fields("run1", (int8_t)status, json, NULL, 1, 2,
                                     3);
  }
  nr_write_message(fd, nr_flatbuffers_data(fb), nr_flatbuffers_len(fb),
                   in_ms(2000));
  nr_flatbuffers_destroy(&fb);
}

typedef struct {
  int fd;  /* connection to serve, or -1 to accept it from lfd */
  int lfd; /* listener to accept from */
  peer_mode_t mode;
  int status;  /* APP_STATUS_* to reply with */
  int variant; /* 2, 3 or 6: which reply builder */
  int nreq;    /* how many requests to serve */
  int accepted_fd;
  int requests_seen;
} peer_t;

static void* peer_thread(void* arg) {
  peer_t* p = arg;
  int i;

  if (p->lfd >= 0) {
    p->fd = accept_within(p->lfd, in_ms(2000));
    p->accepted_fd = p->fd;
  }
  for (i = 0; i < p->nreq && p->fd >= 0; i++) {
    if (!read_frame(p->fd, in_ms(2000))) {
      break;
    }
    p->requests_seen++;
    if (PEER_SILENT != p->mode) {
      send_reply(p->fd, p->mode, p->status, p->variant);
    }
  }
  return NULL;
}

static void peer_start(peer_t* p, nrthread_t* th) {
  p->accepted_fd = -1;
  if (0 == p->nreq) {
    p->nreq = 1;
  }
  if (0 == p->variant) {
    p->variant = 6;
  }
  nrt_create(th, NULL, peer_thread, p);
}

/* ------------------------------------------------------------------ */
/* Single-threaded cases                                               */
/* ------------------------------------------------------------------ */

static void begin_test(void) {
  reset_transport();
}

static void end_test(void) {
  reset_transport();
}

/* Success writes whole frames and keeps the connection. */
static void test_success_keeps_connection(void) {
  int fn;

  for (fn = FN_TXN; fn <= FN_SPAN; fn++) {
    int socks[2];
    int lfd;
    stream_t s;

    begin_test();
    install_pair(socks);
    lfd = listen_live();

    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 1));
    read_stream(socks[1], in_ms(50), &s);
    tlib_pass_if_size_t_equal("success: one frame per call", 1, s.nframes);
    tlib_pass_if_size_t_equal("success: no trailing bytes", 0, s.partial);
    tlib_pass_if_true("success: preamble format", !s.bad_format, "bad format");
    if (1 == s.nframes) {
      tlib_pass_if_int_equal("success: body type", fn_body_type(fn),
                             frame_body_type(&s.frames[0]));
    }
    stream_free(&s);
    expect_open("success: connection stays open", socks[1]);

    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 2));
    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 3));
    read_stream(socks[1], in_ms(50), &s);
    tlib_pass_if_size_t_equal("success: two calls, two frames", 2, s.nframes);
    tlib_pass_if_size_t_equal("success: two calls, no trailing bytes", 0,
                              s.partial);
    stream_free(&s);
    expect_open("success: two calls, connection stays open", socks[1]);
    tlib_pass_if_int_equal("success: no reconnect", -1,
                           accept_within(lfd, in_ms(30)));

    end_test();
    close(socks[1]);
    stop_listening(lfd);
  }
}

/* No connection and nobody listening: fail fast. */
static void test_unreachable_fails_fast(void) {
  int fn;
  nrapp_t* app;
  nrtime_t start;

  begin_test();
  for (fn = FN_TXN; fn <= FN_SPAN; fn++) {
    start = nr_get_time();
    tlib_pass_if_status_failure(fn_name[fn], call_fn(fn, 1));
    tlib_pass_if_true(fn_name[fn], nr_get_time() - start < 50 * MS,
                      "took %d ms", (int)((nr_get_time() - start) / MS));
  }
  app = app_new();
  start = nr_get_time();
  tlib_pass_if_status_failure("unreachable daemon: appinfo fails fast",
                              nr_cmd_appinfo_tx(app));
  tlib_pass_if_true("unreachable daemon: appinfo fails fast",
                    nr_get_time() - start < 50 * MS, "took %d ms",
                    (int)((nr_get_time() - start) / MS));
  tlib_pass_if_int_equal("unreachable daemon: appinfo state", NR_APP_UNKNOWN,
                         (int)app->state);
  app_destroy(&app);
  end_test();
}

/* A write that times out midway leaves a partial frame, then EOF. */
static void test_partial_write_then_eof(void) {
  int socks[2];
  int snd = 0, rcv = 0;
  int one = 1;
  socklen_t l = sizeof(int);
  size_t len;
  stream_t s;

  begin_test();
  nbsockpair(socks);
  setsockopt(socks[0], SOL_SOCKET, SO_SNDBUF, &one, sizeof(one));
  setsockopt(socks[1], SOL_SOCKET, SO_RCVBUF, &one, sizeof(one));
  getsockopt(socks[0], SOL_SOCKET, SO_SNDBUF, &snd, &l);
  getsockopt(socks[1], SOL_SOCKET, SO_RCVBUF, &rcv, &l);
  nr_set_daemon_fd(socks[0]);

  len = 64 * (size_t)(snd + rcv);
  if (len < 512 * 1024) {
    len = 512 * 1024;
  }
  tlib_pass_if_status_failure("write timeout: call fails",
                              call_span(0, 1, len));

  read_stream(socks[1], in_ms(3000), &s);
  tlib_pass_if_true("write timeout: partial frame left", s.partial > 0,
                    "partial=%zu", s.partial);
  tlib_pass_if_size_t_equal("write timeout: no whole frame", 0, s.nframes);
  tlib_pass_if_true("write timeout: EOF after partial frame", s.eof,
                    "not closed");
  stream_free(&s);

  end_test();
  close(socks[1]);
}

/* Peer gone: the 1st call fails and drops the fd, the 2nd reconnects. */
static void test_peer_gone_then_reconnect(void) {
  int fn;

  for (fn = FN_TXN; fn <= FN_SPAN; fn++) {
    int socks[2];
    int lfd, afd;
    stream_t s;

    begin_test();
    install_pair(socks);
    lfd = listen_live();
    close(socks[1]);

    tlib_pass_if_status_failure(fn_name[fn], call_fn(fn, 1));
    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 2));
    afd = accept_within(lfd, in_ms(500));
    tlib_pass_if_true("peer gone: reconnected", afd >= 0, "no accept");
    if (afd >= 0) {
      read_stream(afd, in_ms(50), &s);
      tlib_pass_if_size_t_equal("peer gone: frame on new connection", 1,
                                s.nframes);
      tlib_pass_if_size_t_equal("peer gone: no trailing bytes", 0, s.partial);
      stream_free(&s);
      close(afd);
    }
    end_test();
    stop_listening(lfd);
  }
}

/* APPINFO replies; not-connected statuses keep the connection. */
static void test_appinfo_replies(void) {
  static const struct {
    int status;
    int variant;
    nrapptype_t state;
  } cases[] = {
      {APP_STATUS_CONNECTED, 6, NR_APP_OK},
      {APP_STATUS_UNKNOWN, 2, NR_APP_UNKNOWN},
      {APP_STATUS_DISCONNECTED, 3, NR_APP_INVALID},
      {APP_STATUS_INVALID_LICENSE, 6, NR_APP_INVALID},
  };
  size_t i;

  for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    int socks[2];
    nrapp_t* app;
    nrthread_t th;
    peer_t peer = {.mode = PEER_REPLY,
                   .lfd = -1,
                   .status = cases[i].status,
                   .variant = cases[i].variant};

    begin_test();
    install_pair(socks);
    peer.fd = socks[1];
    app = app_new();
    peer_start(&peer, &th);

    tlib_pass_if_status_success("appinfo reply: call succeeds",
                                nr_cmd_appinfo_tx(app));
    tlib_pass_if_int_equal("appinfo reply: app state", (int)cases[i].state,
                           (int)app->state);
    nrt_join(th, NULL);
    expect_open("appinfo reply: connection stays open", socks[1]);

    app_destroy(&app);
    end_test();
    close(socks[1]);
  }
}

/* A timed-out APPINFO closes the connection; a late reply is never
 * taken as the answer to a later request. */
static void test_appinfo_timeout_and_late_reply(void) {
  int a[2], b[2];
  nrapp_t* app;
  nrthread_t th;
  peer_t silent = {.mode = PEER_SILENT, .lfd = -1};
  peer_t unknown
      = {.mode = PEER_REPLY, .lfd = -1, .status = APP_STATUS_UNKNOWN};
  uint64_t saved = nr_cmd_appinfo_timeout_us;

  begin_test();
  nr_cmd_appinfo_timeout_us = 20 * MS;
  install_pair(a);
  silent.fd = a[1];
  app = app_new();
  peer_start(&silent, &th);

  tlib_pass_if_status_failure("appinfo timeout: call fails",
                              nr_cmd_appinfo_tx(app));
  tlib_pass_if_int_equal("appinfo timeout: app state", NR_APP_UNKNOWN,
                         (int)app->state);
  nrt_join(th, NULL);
  expect_eof("appinfo timeout: connection closed", a[1]);

  /* The daemon answers late; the old connection is gone. */
  {
    nr_flatbuffer_t* late = create_app_reply_two_fields(
        "run1", APP_STATUS_CONNECTED, "{\"agent_run_id\":\"run1\"}");
    nr_write_message(a[1], nr_flatbuffers_data(late), nr_flatbuffers_len(late),
                     in_ms(100));
    nr_flatbuffers_destroy(&late);
  }
  nr_cmd_appinfo_timeout_us = 1000 * MS;
  install_pair(b);
  unknown.fd = b[1];
  peer_start(&unknown, &th);
  tlib_pass_if_status_success("late reply: next call succeeds",
                              nr_cmd_appinfo_tx(app));
  tlib_pass_if_int_equal(
      "late reply: next call saw UNKNOWN, not the late CONNECTED",
      NR_APP_UNKNOWN, (int)app->state);
  tlib_pass_if_null("late reply: no agent run id", app->agent_run_id);
  nrt_join(th, NULL);

  nr_cmd_appinfo_timeout_us = saved;
  app_destroy(&app);
  end_test();
  close(a[1]);
  close(b[1]);
}

/* A well-framed reply of the wrong message type closes the connection. */
static void test_appinfo_wrong_reply_type(void) {
  int socks[2];
  nrapp_t* app;
  nrthread_t th;
  peer_t peer = {.mode = PEER_WRONG_TYPE, .lfd = -1};

  begin_test();
  install_pair(socks);
  peer.fd = socks[1];
  app = app_new();
  peer_start(&peer, &th);

  tlib_pass_if_status_failure("wrong reply type: call fails",
                              nr_cmd_appinfo_tx(app));
  nrt_join(th, NULL);
  expect_eof("wrong reply type: connection closed", socks[1]);

  app_destroy(&app);
  end_test();
  close(socks[1]);
}

/* No connection, reachable daemon: connect once, then reuse. */
static void test_reconnect_on_demand(void) {
  int fn;
  int lfd, afd;
  stream_t s;

  for (fn = FN_TXN; fn <= FN_SPAN; fn++) {
    begin_test();
    lfd = listen_live();

    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 1));
    afd = accept_within(lfd, in_ms(500));
    tlib_pass_if_true("reconnect on demand: accepted", afd >= 0, "no accept");
    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 2));
    tlib_pass_if_int_equal("reconnect on demand: no second connection", -1,
                           accept_within(lfd, in_ms(30)));
    if (afd >= 0) {
      read_stream(afd, in_ms(50), &s);
      tlib_pass_if_size_t_equal("reconnect on demand: two whole frames", 2,
                                s.nframes);
      tlib_pass_if_size_t_equal("reconnect on demand: no trailing bytes", 0,
                                s.partial);
      stream_free(&s);
      close(afd);
    }
    end_test();
    stop_listening(lfd);
  }

  {
    nrapp_t* app;
    nrthread_t th;
    peer_t peer = {.mode = PEER_REPLY, .status = APP_STATUS_CONNECTED};

    begin_test();
    lfd = listen_live();
    peer.lfd = lfd;
    peer.fd = -1;
    peer.nreq = 2;
    app = app_new();
    peer_start(&peer, &th);

    tlib_pass_if_status_success("reconnect on demand: appinfo succeeds",
                                nr_cmd_appinfo_tx(app));
    tlib_pass_if_int_equal("reconnect on demand: appinfo state", NR_APP_OK,
                           (int)app->state);
    tlib_pass_if_status_success("reconnect on demand: second appinfo succeeds",
                                nr_cmd_appinfo_tx(app));
    nrt_join(th, NULL);
    tlib_pass_if_int_equal(
        "reconnect on demand: appinfo requests on one connection", 2,
        peer.requests_seen);
    tlib_pass_if_int_equal("reconnect on demand: appinfo no second connection",
                           -1, accept_within(lfd, in_ms(30)));
    if (peer.accepted_fd >= 0) {
      close(peer.accepted_fd);
    }
    app_destroy(&app);
    end_test();
    stop_listening(lfd);
  }
}

/* The daemon drops us: one call fails, the next reconnects. */
static void test_daemon_drops_connection(void) {
  int fn;

  for (fn = FN_TXN; fn <= FN_SPAN; fn++) {
    int lfd, afd;
    stream_t s;

    begin_test();
    lfd = listen_live();

    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 1));
    afd = accept_within(lfd, in_ms(500));
    tlib_pass_if_true("daemon drops connection: accepted", afd >= 0,
                      "no accept");
    close(afd);

    tlib_pass_if_status_failure(fn_name[fn], call_fn(fn, 2));
    tlib_pass_if_status_success(fn_name[fn], call_fn(fn, 3));
    afd = accept_within(lfd, in_ms(500));
    tlib_pass_if_true("daemon drops connection: reconnected", afd >= 0,
                      "no accept");
    if (afd >= 0) {
      read_stream(afd, in_ms(50), &s);
      tlib_pass_if_size_t_equal(
          "daemon drops connection: frame on new connection", 1, s.nframes);
      tlib_pass_if_size_t_equal("daemon drops connection: no trailing bytes", 0,
                                s.partial);
      stream_free(&s);
      close(afd);
    }
    end_test();
    stop_listening(lfd);
  }
}

/*
 * Linux only: a loopback TCP connect is non-blocking and may still be in
 * progress when the first call arrives. Whether it is depends on timing, so
 * accept both outcomes: calls either fail without writing anything or
 * succeed, and never break a frame.
 */
static void test_connect_in_progress(void) {
#ifdef __linux__
  struct sockaddr_in sa;
  socklen_t sl = sizeof(sa);
  char port[16];
  int lfd, afd;
  int calls = 0, successes = 0;
  nrtime_t deadline;
  stream_t s;

  begin_test();
  lfd = socket(AF_INET, SOCK_STREAM, 0);
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bind(lfd, (struct sockaddr*)&sa, sizeof(sa));
  listen(lfd, 16);
  nr_network_set_non_blocking(lfd);
  getsockname(lfd, (struct sockaddr*)&sa, &sl);
  snprintf(port, sizeof(port), "%d", ntohs(sa.sin_port));
  set_address(port);

  deadline = in_ms(100);
  while (0 == successes && nr_get_time() < deadline) {
    calls++;
    if (NR_SUCCESS == call_fn(FN_TXN, 1)) {
      successes++;
    }
  }
  tlib_pass_if_int_equal("connect in progress: a call succeeded within 100 ms",
                         1, successes);

  afd = accept_within(lfd, in_ms(500));
  tlib_pass_if_true("connect in progress: accepted", afd >= 0, "calls=%d",
                    calls);
  if (afd >= 0) {
    read_stream(afd, in_ms(50), &s);
    tlib_pass_if_size_t_equal("connect in progress: only the successful frame",
                              1, s.nframes);
    tlib_pass_if_size_t_equal("connect in progress: no trailing bytes", 0,
                              s.partial);
    tlib_pass_if_int_equal("connect in progress: single connection", -1,
                           accept_within(lfd, in_ms(30)));
    stream_free(&s);
    close(afd);
  }
  end_test();
  close(lfd);
#endif
}

/* ------------------------------------------------------------------ */
/* Concurrency helpers                                                 */
/* ------------------------------------------------------------------ */

static int iterations(const char* name, int dflt) {
  const char* v = getenv(name);

  return (v && atoi(v) > 0) ? atoi(v) : dflt;
}

static void micro_sleep(unsigned max_us, unsigned* seed) {
  if (max_us) {
    usleep(rand_r(seed) % max_us);
  }
}

typedef struct {
  int fd;
  bool accepted;
  unsigned delay_ms;
  stream_t s;
  nrthread_t th;
} conn_t;

typedef struct {
  nrthread_mutex_t mu;
  conn_t** conns;
  size_t n, cap;
} registry_t;

/* Reads a connection's whole stream (after an optional delay) until EOF. */
static void* reader_thread(void* arg) {
  conn_t* c = arg;

  if (c->delay_ms) {
    usleep(c->delay_ms * 1000);
  }
  read_stream(c->fd, in_ms(60000), &c->s);
  close(c->fd);
  return NULL;
}

static void registry_add(registry_t* r, int fd, bool accepted, unsigned delay) {
  conn_t* c = nr_calloc(1, sizeof(*c));

  c->fd = fd;
  c->accepted = accepted;
  c->delay_ms = delay;
  nrt_mutex_lock(&r->mu);
  if (r->n == r->cap) {
    r->cap = r->cap ? r->cap * 2 : 64;
    r->conns = nr_realloc(r->conns, r->cap * sizeof(*r->conns));
  }
  r->conns[r->n++] = c;
  nrt_mutex_unlock(&r->mu);
  nrt_create(&c->th, NULL, reader_thread, c);
}

/*
 * Connection churn: frames stay whole while connections are replaced.
 *
 * Writers send TXNDATA and SPAN_BATCH frames while a chaos thread keeps
 * swapping the connection (a fresh socketpair, or none, which forces a
 * reconnect to the listener). Every so often the new pair has a tiny send
 * buffer and a reader that starts late, and the writers send only big
 * frames until one is stuck, so a write times out midway through a frame.
 * Every stream must be frame* [partial] EOF; a partial frame followed by
 * another writer's frame would show up as a garbled frame. Every span batch
 * must decode to a (thread, sequence) pair seen exactly once, and every
 * call that returned success must have arrived.
 */
#define CHURN_WRITERS 8
#define CHURN_MAXSEQ (1u << 20)

typedef struct {
  int id;
  int* stop;
  int* big_only; /* while set, send only big span batches */
  int* big_acks; /* writers that have switched to big-only mode */
  uint8_t* ok;   /* ok[seq]: call returned success */
} churn_writer_t;

typedef struct {
  int lfd;
  int* stop;
  registry_t* reg;
} churn_acceptor_t;

static void wait_until(int* counter, int want) {
  nrtime_t deadline = in_ms(1000);

  while (A_LOAD(counter) < want && nr_get_time() < deadline) {
    usleep(200);
  }
}

/* Peeks at the daemon's end until a frame too big for the pair has started. */
static void wait_for_big_frame(int fd) {
  nrtime_t deadline = in_ms(1000);
  uint8_t buf[65536];

  while (nr_get_time() < deadline) {
    stream_t s = {.raw = buf};
    ssize_t n = recv(fd, buf, sizeof(buf), MSG_PEEK);
    bool big;

    if (n > 0) {
      s.rawlen = (size_t)n;
      split_frames(&s);
      big = s.partial_len >= span_len(16);
      for (size_t k = 0; k < s.nframes; k++) {
        big = big || s.frames[k].len >= span_len(16);
      }
      nr_free(s.frames);
      if (big) {
        return;
      }
    }
    usleep(1000);
  }
}

static void* churn_writer(void* arg) {
  churn_writer_t* w = arg;
  uint32_t seq;
  bool acked = false;

  for (seq = 1; seq < CHURN_MAXSEQ && !A_LOAD(w->stop); seq++) {
    nrtxn_t txn;

    if (A_LOAD(w->big_only)) {
      if (!acked) {
        A_ADD(w->big_acks, 1);
        acked = true;
      }
      seq = (seq + 15) & ~15u; /* span_len() is big for multiples of 16 */
    } else {
      acked = false;
      nr_memset(&txn, 0, sizeof(txn));
      nr_cmd_txndata_tx(&txn);
    }
    if (NR_SUCCESS == call_span((uint64_t)w->id, seq, span_len(seq))) {
      w->ok[seq] = 1;
    }
    usleep(20); /* don't starve the chaos thread of the daemon lock */
  }
  return NULL;
}

static void* churn_acceptor(void* arg) {
  churn_acceptor_t* a = arg;
  int fd;

  while (!A_LOAD(a->stop)) {
    fd = accept_within(a->lfd, in_ms(20));
    if (fd >= 0) {
      registry_add(a->reg, fd, true, 0);
    }
  }
  while ((fd = accept_within(a->lfd, in_ms(50))) >= 0) {
    registry_add(a->reg, fd, true, 0);
  }
  return NULL;
}

static void test_frames_stay_whole_under_churn(void) {
  int iters = iterations("NR_TEST_TRANSPORT_CHURN_ITERS", 200);
  int stall_every = iters / 2 > 0 ? iters / 2 : 1;
  registry_t reg = {0};
  int stop_writers = 0, stop_acceptor = 0, big_only = 0, big_acks = 0;
  churn_writer_t writers[CHURN_WRITERS];
  nrthread_t wth[CHURN_WRITERS], ath;
  churn_acceptor_t acc;
  unsigned seed = 1;
  int lfd, i;
  size_t k;
  size_t frames = 0, accepted_frames = 0, partials = 0, span_frames = 0;
  size_t violations = 0;
  uint8_t* seen[CHURN_WRITERS];

  begin_test();
  nrt_mutex_init(&reg.mu, 0);
  lfd = listen_live();
  acc = (churn_acceptor_t){.lfd = lfd, .stop = &stop_acceptor, .reg = &reg};
  nrt_create(&ath, NULL, churn_acceptor, &acc);

  for (i = 0; i < CHURN_WRITERS; i++) {
    seen[i] = nr_calloc(CHURN_MAXSEQ, 1);
    writers[i] = (churn_writer_t){.id = i,
                                  .stop = &stop_writers,
                                  .big_only = &big_only,
                                  .big_acks = &big_acks,
                                  .ok = nr_calloc(CHURN_MAXSEQ, 1)};
    nrt_create(&wth[i], NULL, churn_writer, &writers[i]);
  }

  for (i = 0; i < iters; i++) {
    micro_sleep(1000, &seed);
    if (i % stall_every == stall_every - 1 || rand_r(&seed) % 2) {
      int socks[2];
      unsigned delay = 0;

      nbsockpair(socks);
      if (i % stall_every == stall_every - 1) {
        int one = 1;

        setsockopt(socks[0], SOL_SOCKET, SO_SNDBUF, &one, sizeof(one));
        setsockopt(socks[1], SOL_SOCKET, SO_RCVBUF, &one, sizeof(one));
        delay = 700;
      }
      registry_add(&reg, socks[1], false, delay);
      if (delay) {
        /* Once every writer is past its last small frame, install. */
        A_STORE(&big_acks, 0);
        A_STORE(&big_only, 1);
        wait_until(&big_acks, CHURN_WRITERS);
      }
      nr_set_daemon_fd(socks[0]);
      if (delay) {
        wait_for_big_frame(socks[1]);
        A_STORE(&big_only, 0);
      }
    } else {
      nr_set_daemon_fd(-1);
    }
  }

  A_STORE(&stop_writers, 1);
  for (i = 0; i < CHURN_WRITERS; i++) {
    nrt_join(wth[i], NULL);
  }
  nr_set_daemon_fd(-1);
  A_STORE(&stop_acceptor, 1);
  nrt_join(ath, NULL);

  for (k = 0; k < reg.n; k++) {
    conn_t* c = reg.conns[k];
    size_t f;

    nrt_join(c->th, NULL);
    if (c->s.bad_format || !c->s.eof) {
      violations++;
    }
    partials += c->s.partial > 0;
    for (f = 0; f < c->s.nframes; f++) {
      const frame_t* fr = &c->s.frames[f];
      uint64_t tid, seq;
      size_t len;

      frames++;
      accepted_frames += c->accepted;
      if (fr->len < 16 || le32(fr->body) >= fr->len) {
        violations++;
      } else if (MESSAGE_BODY_TXN == frame_body_type(fr)) {
        continue;
      } else if (!payload_decode(fr, &tid, &seq, &len) || tid >= CHURN_WRITERS
                 || seq >= CHURN_MAXSEQ || len != span_len(seq)
                 || seen[tid][seq]++) {
        violations++;
      } else {
        span_frames++;
      }
    }
    stream_free(&c->s);
    nr_free(c);
  }
  nr_free(reg.conns);
  nrt_mutex_destroy(&reg.mu);

  for (i = 0; i < CHURN_WRITERS; i++) {
    uint32_t seq;

    for (seq = 1; seq < CHURN_MAXSEQ; seq++) {
      if (writers[i].ok[seq] && !seen[i][seq]) {
        violations++;
      }
    }
    nr_free(writers[i].ok);
    nr_free(seen[i]);
  }

  printf(
      "churn: %zu frames (%zu span batches, %zu on accepted connections), "
      "%zu partial\n",
      frames, span_frames, accepted_frames, partials);
  tlib_pass_if_size_t_equal("churn: stream violations", 0, violations);
  tlib_pass_if_true("churn: reconnect path ran", accepted_frames > 0,
                    "no frame arrived on an accepted connection");
  tlib_pass_if_true("churn: a frame was cut short", partials > 0,
                    "no partial frame: the timeout path was not exercised");

  end_test();
  stop_listening(lfd);
}

/*
 * A failure never closes a newer connection.
 *
 * Deterministic version. A call whose reply fails to parse logs errors and
 * closes the connection. If it closes after releasing the lock, a newer
 * connection installed in between is closed instead. The agent's log goes to
 * a full FIFO, so the failing call blocks on its first log line. The test
 * installs a new connection while it is blocked, then lets it go. The new
 * connection must still be open. If the close happens under the lock (the
 * fix), the installer just waits its turn and the result is the same.
 */
typedef struct {
  int fd;
  int installed;
} spare_installer_t;

static void* spare_appinfo_once(void* arg) {
  nr_cmd_appinfo_tx(arg);
  return NULL;
}

static void* spare_installer(void* arg) {
  spare_installer_t* i = arg;

  nr_set_daemon_fd(i->fd);
  A_STORE(&i->installed, 1);
  return NULL;
}

static void test_failure_spares_newer_connection(void) {
  char fifo[96];
  char junk[1024];
  int rfd, wfd, a[2], b[2];
  nrapp_t* app;
  nrthread_t worker, installer, peer_th;
  peer_t bad = {.mode = PEER_WRONG_TYPE, .lfd = -1};
  spare_installer_t ins = {0};
  nrtime_t give_up;

  begin_test();
  memset(junk, 'x', sizeof(junk));
  snprintf(fifo, sizeof(fifo), "%s/log.fifo", tmpdir);
  mkfifo(fifo, 0600);
  rfd = open(fifo, O_RDONLY | O_NONBLOCK);
  wfd = open(fifo, O_WRONLY | O_NONBLOCK);
  while (write(wfd, junk, sizeof(junk)) > 0) {
  }
  nrl_set_log_level("error");
  nrl_set_log_file(fifo); /* blocking write end: a full FIFO stalls the log */

  install_pair(a);
  bad.fd = a[1];
  app = app_new();
  peer_start(&bad, &peer_th);
  nrt_create(&worker, NULL, spare_appinfo_once, app);
  nrt_join(peer_th, NULL); /* the wrong-type reply has been sent */

  /* Install a newer connection while the failing call is stuck logging. */
  nbsockpair(b);
  ins.fd = b[0];
  nrt_create(&installer, NULL, spare_installer, &ins);
  give_up = in_ms(500);
  while (!A_LOAD(&ins.installed) && nr_get_time() < give_up) {
    usleep(1000);
  }

  while (read(rfd, junk, sizeof(junk)) > 0) { /* let the failing call go */
  }
  nrt_join(worker, NULL);
  nrt_join(installer, NULL);

  expect_open("newer connection was closed by an older call's failure", b[1]);

  nrl_close_log_file();
  close(rfd);
  close(wfd);
  unlink(fifo);
  app_destroy(&app);
  end_test();
  close(a[1]);
  close(b[1]);
}

/*
 * A failure never closes a newer connection (stress): the same property
 * under real interleaving, native runs only.
 *
 * Workers call APPINFO in a loop. The chaos thread alternates a "bad" pair
 * (its peer answers with the wrong message type, so the call fails to parse
 * the reply) with a "good" pair, installing the good one as soon as the bad
 * peer has answered. A good peer that sees EOF before its pair was retired by
 * the chaos thread was closed by somebody else. Also requires that bad peers
 * answered, so a green run means the failure path ran.
 */
#define SPARE_WORKERS 4

typedef struct {
  int fd;
  bool good;
  int retiring;
  int served;
  int killed; /* good peer saw EOF it was not owed */
  nrthread_t th;
} spare_peer_t;

typedef struct {
  int* stop;
  unsigned calls;
} spare_worker_t;

static void* spare_peer_thread(void* arg) {
  spare_peer_t* p = arg;

  for (;;) {
    /* The deadline never expires in practice, so a failure here is a close. */
    if (!read_frame(p->fd, in_ms(60000))) {
      if (p->good && !A_LOAD(&p->retiring)) {
        A_STORE(&p->killed, 1);
      }
      break;
    }
    send_reply(p->fd, p->good ? PEER_REPLY : PEER_WRONG_TYPE,
               APP_STATUS_CONNECTED, 2);
    A_ADD(&p->served, 1);
  }
  close(p->fd);
  return NULL;
}

static void* spare_worker(void* arg) {
  spare_worker_t* w = arg;
  nrapp_t* app;

  app = app_new();
  while (!A_LOAD(w->stop)) {
    nr_cmd_appinfo_tx(app);
    w->calls++;
    usleep(20); /* don't starve the chaos thread of the daemon lock */
  }
  app_destroy(&app);
  return NULL;
}

static spare_peer_t* spare_install(bool good) {
  spare_peer_t* p = nr_calloc(1, sizeof(*p));
  int socks[2];

  nbsockpair(socks);
  p->fd = socks[1];
  p->good = good;
  nrt_create(&p->th, NULL, spare_peer_thread, p);
  nr_set_daemon_fd(socks[0]);
  return p;
}

static void test_failure_spares_newer_connection_stress(void) {
  int iters = iterations("NR_TEST_TRANSPORT_FAILURE_STRESS_ITERS", 40);
  uint64_t saved = nr_cmd_appinfo_timeout_us;
  int stop = 0, i, killed = 0, bad_served = 0;
  spare_worker_t workers[SPARE_WORKERS];
  nrthread_t wth[SPARE_WORKERS];
  spare_peer_t** peers = nr_calloc((size_t)iters * 2, sizeof(*peers));
  size_t npeers = 0, k;
  unsigned long calls = 0;

  begin_test();
  nr_cmd_appinfo_timeout_us = 1000 * MS;
  for (i = 0; i < SPARE_WORKERS; i++) {
    workers[i] = (spare_worker_t){.stop = &stop};
    nrt_create(&wth[i], NULL, spare_worker, &workers[i]);
  }

  for (i = 0; i < iters; i++) {
    spare_peer_t* bad;
    spare_peer_t* good;
    nrtime_t give_up = in_ms(5);

    if (npeers && peers[npeers - 1]->good) {
      A_STORE(&peers[npeers - 1]->retiring, 1);
    }
    bad = spare_install(false);
    peers[npeers++] = bad;
    while (0 == A_LOAD(&bad->served) && nr_get_time() < give_up) {
      sched_yield();
    }
    good = spare_install(true);
    peers[npeers++] = good;
    usleep(200);
  }

  A_STORE(&peers[npeers - 1]->retiring, 1);
  A_STORE(&stop, 1);
  for (i = 0; i < SPARE_WORKERS; i++) {
    nrt_join(wth[i], NULL);
    calls += workers[i].calls;
  }
  nr_set_daemon_fd(-1);
  for (k = 0; k < npeers; k++) {
    nrt_join(peers[k]->th, NULL);
    killed += A_LOAD(&peers[k]->killed);
    bad_served += peers[k]->good ? 0 : A_LOAD(&peers[k]->served);
    nr_free(peers[k]);
  }
  nr_free(peers);

  printf(
      "stress: %lu calls, %d failing replies, %d good connections "
      "closed by another call's failure\n",
      calls, bad_served, killed);
  tlib_pass_if_int_equal(
      "stress: good connections closed by a failure "
      "elsewhere",
      0, killed);
  tlib_pass_if_true("stress: failure path ran", bad_served > 0,
                    "no bad peer ever answered");

  nr_cmd_appinfo_timeout_us = saved;
  end_test();
}

/*
 * fd reuse: no writes to a reused fd number.
 *
 * Writers send TXNDATA while the chaos thread closes the agent's connection
 * (fd N) and right away makes a decoy pipe whose write end is fd N. The
 * decoy must never receive a byte. The decoy is placed with F_DUPFD, which
 * takes N only if it is free, so it never clobbers an fd a writer's failed
 * reconnect attempt is holding. The test counts how often the decoy really
 * got N, and fails if that never happened.
 */
#define REUSE_WRITERS 4

static void* reuse_writer(void* arg) {
  int* stop = arg;
  nrtxn_t txn;

  nr_memset(&txn, 0, sizeof(txn));
  while (!A_LOAD(stop)) {
    nr_cmd_txndata_tx(&txn);
    usleep(20); /* don't starve the chaos thread of the daemon lock */
  }
  return NULL;
}

static void drain(int fd) {
  char tmp[4096];

  while (read(fd, tmp, sizeof(tmp)) > 0) {
  }
}

static void test_no_write_to_reused_fd(void) {
  int iters = iterations("NR_TEST_TRANSPORT_FD_REUSE_ITERS", 2000);
  int stop = 0, i;
  nrthread_t wth[REUSE_WRITERS];
  int socks[2];
  int hits = 0, bytes_seen = 0;

  begin_test();
  install_pair(socks);
  for (i = 0; i < REUSE_WRITERS; i++) {
    nrt_create(&wth[i], NULL, reuse_writer, &stop);
  }

  for (i = 0; i < iters; i++) {
    int n = socks[0];
    int p[2], dw;
    char c;

    nr_set_daemon_fd(-1); /* closes fd n */
    if (0 != pipe(p)) {
      break;
    }
    nr_network_set_non_blocking(p[0]);
    if (p[1] == n) {
      dw = p[1];
    } else {
      dw = fcntl(p[1], F_DUPFD, n);
      close(p[1]);
    }
    hits += dw == n;
    usleep(100);
    if (dw >= 0) {
      close(dw);
    }
    if (read(p[0], &c, 1) > 0) {
      bytes_seen++;
    }
    close(p[0]);

    drain(socks[1]);
    close(socks[1]);
    install_pair(socks);
  }

  A_STORE(&stop, 1);
  for (i = 0; i < REUSE_WRITERS; i++) {
    nrt_join(wth[i], NULL);
  }
  end_test();
  close(socks[1]);

  printf("fd reuse: decoy got the closed fd's number in %d of %d iterations\n",
         hits, iters);
  tlib_pass_if_int_equal("fd reuse: decoy pipes that received data", 0,
                         bytes_seen);
  tlib_pass_if_true("fd reuse: scenario was exercised", hits > 0,
                    "the decoy never got the closed fd's number");
}

tlib_parallel_info_t parallel_info = {.suggested_nthreads = 1, .state_size = 0};

void test_main(void* p NRUNUSED) {
  char dir[] = "/tmp/nrXXXXXX";

  /* axiom writes without MSG_NOSIGNAL; a closed peer must not kill us. */
  signal(SIGPIPE, SIG_IGN);
  alarm(600);

  if (NULL == mkdtemp(dir)) {
    tlib_pass_if_true("mkdtemp", 0, "mkdtemp failed: %s", strerror(errno));
    return;
  }
  snprintf(tmpdir, sizeof(tmpdir), "%s", dir);
  snprintf(live_path, sizeof(live_path), "%s/live.sock", tmpdir);
  snprintf(none_path, sizeof(none_path), "%s/none.sock", tmpdir);
  reset_transport();

  test_success_keeps_connection();
  test_unreachable_fails_fast();
  test_partial_write_then_eof();
  test_peer_gone_then_reconnect();
  test_appinfo_replies();
  test_appinfo_timeout_and_late_reply();
  test_appinfo_wrong_reply_type();
  test_reconnect_on_demand();
  test_daemon_drops_connection();
  test_connect_in_progress();
  test_no_write_to_reused_fd();
  test_frames_stay_whole_under_churn();
  test_failure_spares_newer_connection_stress();
  /* Last: it changes the log level and file. */
  test_failure_spares_newer_connection();

  reset_transport();
  rmdir(tmpdir);
  alarm(0);
}
