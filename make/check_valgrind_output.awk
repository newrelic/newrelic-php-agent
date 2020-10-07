#!/usr/bin/awk -f

#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#

# A script to process a Valgrind log file and specifically extract whether
# there were unsuppressed errors. If so, the script will exit with a non-zero
# status code. This script will always output the error count on a single line,
# which can then be displayed in a build script.
#
# It is possible to run this script in verbose mode by setting the
# VALGRIND_VERBOSE environment variable to a non-zero, non-blank value. In this
# mode, if there are errors, the full Valgrind log file will be output after
# the status line.
#
# This script should work on any POSIX-compliant awk implementation, although
# in practice, the only platforms we can run Valgrind on are likely to have GNU
# awk available regardless.

BEGIN {
  # As the exit statement will always fire the END block, we have to track
  # whether we actually saw a summary or not.
  found_summary = 0

  # See if we're in verbose mode: if we are, then we need to track all of the
  # output so that we can output it if errors occurred.
  #
  # This is controlled by an environment variable because POSIX awk doesn't
  # allow us to pass generic switches in on the command line: they will be
  # treated as either awk options or input files, and removing them from ARGV
  # in the BEGIN block breaks processing on at least the OS X awk
  # implementation.
  found_errors = 0
  verbose = 0
  if (ENVIRON["VALGRIND_VERBOSE"]) {
    verbose = 1
    output = ""
  }
}

{
  # Append Valgrind output to the output variable if we're planning to output
  # it on error.
  if (verbose) {
    output = output $0 RS
  }
}

/ERROR SUMMARY: / {
  found_summary = 1

  # GNU awk supports capture groups, but POSIX awk doesn't. For the time being
  # we'll do this the old fashioned way below by splitting the string if the
  # regex matches.
  if (match($0, "[0-9]+ error(s?) from [0-9]+ context(s?)")) {
    errors = substr($0, RSTART, RLENGTH)

    split(errors, words, " ")
    if (words[1] > 0) {
      print errors, "in", FILENAME
      found_errors = 1
      exit 1
    }

    print errors
    exit 0
  } else {
    print "Malformed error summary:", $0
    exit 1
  }
}

END {
  if (0 == found_summary) {
    print "No error summary found"
    exit 1
  }

  if (found_errors && verbose) {
    print output
  }
}
