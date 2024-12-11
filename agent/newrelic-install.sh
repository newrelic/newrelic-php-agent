#! /bin/sh

#
# Copyright 2020 New Relic Corporation. All rights reserved.
# SPDX-License-Identifier: Apache-2.0
#


#
# New Relic agent installation script.
#

: ${nrversion:="UNSET"}
#
# This must run as root.
#
myuid=`id -u 2>/dev/null`
if [ "${myuid}" != "0" ]; then
  echo "ERROR: $0 must be run as root" >&2
  exit 1
fi

if [ -n "${NR_INSTALL_SHELL}" -a -z "${NR_KSH_EXECED}" ]; then
  [ -x "${NR_INSTALL_SHELL}" ] && NR_KSH_EXECED=1 exec "${NR_INSTALL_SHELL}" -c $0 "$@"
fi

if [ -n "${BASH_VERSION}" ]; then
  case $BASH_VERSION in
    [3456789].* )  NR_KSH_EXECED=1 ;;
  esac
fi

if [ -z "${NR_INSTALL_NOKSH}" -a -z "${NR_KSH_EXECED}" ]; then
  [ -x /bin/bash ] && NR_KSH_EXECED=1 exec /bin/bash $0 "$@"
  [ -x /usr/bin/bash ] && NR_KSH_EXECED=1 exec /usr/bin/bash $0 "$@"
  [ -x /bin/ksh ] && NR_KSH_EXECED=1 exec /bin/ksh $0 "$@"
  [ -x /usr/bin/ksh ] && NR_KSH_EXECED=1 exec /usr/bin/ksh $0 "$@"
fi

LANG=
nl='
'
#
# Determine how to suppress newlines in echo statements.
#
if echo ' \c' | grep 'c' > /dev/null 2>&1; then
  en='-n'
  ec=
else
  en=
  ec='\c'
fi

#
# Important: if this script is being invoked on a system where a package
# manager such as rpm or dpkg did the installation, the script lives in the
# shared scripts directory, and is exec'ed from that location by a front-end
# script in /usr/bin. If invoked directly out of a tar distribution it will
# not have been execed this way and we need to look in the current location
# for the scripts and agents directories. In the first case (rpm etc) the
# front end script will have set the NR_INSTALL_LOCATION environment variable
# to point to the installed location. If this variable is not set, we need
# to compute it later on. However, we always keep a variable called ispkg
# set to a non-empty string if we were invoked by the front-end script.
# Thus, in the code below, please be aware that this is what ispkg means.
#
ispkg=
[ -n "${NR_INSTALL_LOCATION}" ] && ispkg=1

imode=
if [ -n "$1" ]; then
  [ "$1" = "install" ] && imode=$1
  [ "$1" = "install_daemon" ] && imode=$1
  [ "$1" = "uninstall" ] && imode=$1
  [ "$1" = "purge" ] && imode=$1
fi

if [ -n "${NR_INSTALL_SILENT}" ]; then
  if [ -z "$imode" ]; then
    echo "ERROR: must invoke with either 'install', 'install_daemon' or 'uninstall' in silent mode." >&2
    exit 1
  fi
fi

#
# Set up the logging environment.
#
nritemp=`date +%Y%m%d-%H%M%S 2> /dev/null`
nritemp="${nritemp}-$$"
nrilog=/tmp/nrinstall-${nritemp}.log
nrsysinfo=/tmp/nrinstall-${nritemp}.sysinfo
logtar=/tmp/nrinstall-${nritemp}.tar
now=`date 2>/dev/null`

#
# Clean up any existing files.
#
rm -f $nrilog $nrsysinfo $logtar > /dev/null 2>&1

#
# Create the log file.
#
echo "New Relic Agent Install Log dated: $now" > $nrilog

#
# And the sysinfo file.
#
echo "New Relic Agent Install sysinfo dated: $now" > $nrsysinfo

#
# Set up enough of the error handling environment so we can error out
# in the event we don't support the O/S we're on.
#
error() {
  echo "ERROR: $@" >> $nrilog
  echo "ERROR: $@" >&2
}

#
# Get the OS type.
#
ostype=
if [ -f /etc/redhat-release -o -f /etc/redhat_version ]; then
  ostype=rhel
elif [ -f /etc/os-release ] && grep -q -i 'Debian' /etc/os-release; then
  ostype=debian
elif [ -f /etc/alpine-release ] || [ -d /etc/apk ]; then
  ostype=alpine
elif [ -f /etc/release ]; then
  if grep Solaris /etc/release > /dev/null 2>&1; then
    ostype=solaris
  fi
fi

if [ -z "${ostype}" ]; then
  tus=`uname -s 2> /dev/null`
  case "${tus}" in
    [Dd][Aa][Rr][Ww][Ii][Nn])     ostype=unsupported_os ;;
    [Ff][Rr][Ee][Ee][Bb][Ss][Dd]) ostype=unsupported_os ;;
    [Ss][Uu][Nn][Oo][Ss])         ostype=unsupported_os ;;
    [Ss][Mm][Aa][Rr][Tt][Oo][Ss]) ostype=unsupported_os ;;
  esac
fi
: ${ostype:=generic}

#
# Bail out if we don't support the operating system we are on.
#
# If New Relic doesn't release anything for the O/S we are on,
# the only reason why we would have gotten here is if the user
# unpacked a tarball on an unsupported O/S
# and then attempted to run this shell script.
#
if [ "${ostype}" = "unsupported_os" ] ; then
  msg=$(
  cat << EOF

Your operating system is not supported by the New Relic PHP
agent. If you have any questions or believe you reached this
message in error, please file a ticket with New Relic support.
Please visit:
  https://docs.newrelic.com/docs/apm/agents/php-agent/getting-started/php-agent-compatibility-requirements/
to view compatibilty requirements for the the New Relic PHP agent.
The install will now exit.
EOF
)

  error "${msg}"
  exit 1
fi

#
# We need to determine where we were invoked from. We use this to calculate our
# default paths for where we look for things. This makes the whole New Relic
# package relocatable.
#

: ${PATH_SEPARATOR:=':'}
if [ -z "${ispkg}" ]; then
  myloc=

  case "$0" in
    */*) myloc=$0 ;;
    *) oIFS=$IFS
       IFS=$PATH_SEPARATOR
       for d in $PATH; do
         IFS="${oIFS}"
         [ -z "$d" ] && d="."
         [ -r "$d/$0" ] && {
           myloc="$d/$0"
           break
         }
       done
       IFS="${oIFS}"
       ;;
  esac

  [ -z "${myloc}" ] && myloc=$0
  if [ -z "${myloc}" ]; then
    echo "ERROR: existential crisis: could not find myself. Re-run with absolute path." >&2
    exit 1
  fi

#
# Now that we know our invocation location, we can determine the directory it
# is in. This next line is the moral equivalent of `dirname $myloc` except
# that it does not require an extra exec and relies solely on shell variable
# expansion.
#
  mydir="${myloc%/*}"

#
# Not all shells track $PWD accurately so unfortunately, we cannot determine
# the absolute path without doing an exec.
#
  NR_INSTALL_LOCATION=`(cd "${mydir}" 2> /dev/null; pwd 2>/dev/null)`
fi
ilibdir="${NR_INSTALL_LOCATION}"

#
# Determine the architecture we are executing on.
#
arch=`(uname -m) 2> /dev/null` || arch="unknown"
os=`(uname -s) 2> /dev/null` || os="unknown"

case "${arch}" in
  aarch64 | arm64)      arch=aarch64 ;;
  i[3456789]86)         arch=x86 ;;
  *64* | *amd*)         arch=x64 ;;
  i86pc)
    if [ "${ostype}" = "solaris" ]; then
      solarch=`/usr/bin/isainfo -k 2> /dev/null`
      case "$solarch" in
        *64*)           arch=x64 ;;
        *)              arch=x86 ;;
      esac
    else
                        arch=x86
    fi
    ;;
  unknown)              arch=x86 ;;
esac

# allow override of detected arch
[ "${NR_INSTALL_ARCH}" = "x64" -o "${NR_INSTALL_ARCH}" = "x86_64" ] && arch="${NR_INSTALL_ARCH}"

# exit if arch is unsupported
if [ "${arch}" != "x64" ] && [ "${arch}" != "aarch64" ] ; then
    msg=$(
    cat << EOF

An unsupported architecture detected.
Please visit:
  https://docs.newrelic.com/docs/apm/agents/php-agent/getting-started/php-agent-compatibility-requirements/
to view compatibilty requirements for the the New Relic PHP agent.
EOF
)

  error "${msg}"

  echo "The install will now exit."
  exit 1
fi

#
# Do some sanity checking. ilibdir should contain the daemon, the agents and
# the init scripts. Ensure that is the case.
#
dmissing=
fmissing=

check_dir() {
  if [ ! -d "$1" ]; then
    if [ -n "$1" ]; then
      dmissing="${1}${nl}${dmissing}"
    fi
  fi
}

check_file() {
  if [ ! -f "$1" ]; then
    if [ -n "$1" ]; then
      fmissing="${1}${nl}${fmissing}"
    fi
  fi
}

xtradir=
if [ -n "${ispkg}" ]; then
  daemon=/usr/bin/newrelic-daemon
else
  xtradir="${ilibdir}/daemon"
  daemon="${ilibdir}/daemon/newrelic-daemon.${arch}"
fi

for d in "${ilibdir}/scripts" "${ilibdir}/agent" "${xtradir}"; do
  check_dir "${d}"
done

check_dir "${ilibdir}/agent/${arch}"

if [ -n "${dmissing}" ]; then
  echo "ERROR: the following directories could not be found:" >&2
  echo "${dmissing}" | sed -e 's/^/   /' >&2
fi

NRIUTIL="${ilibdir}/scripts/newrelic-iutil.${arch}"
[ -x "${NRIUTIL}" ] || fmissing="${NRIUTIL}${nl}${fmissing}"

if [ -z "${ispkg}" ]; then
  check_file "${daemon}"
  check_file "${ilibdir}/scripts/init.alpine"
  check_file "${ilibdir}/scripts/init.darwin"
  check_file "${ilibdir}/scripts/init.debian"
  check_file "${ilibdir}/scripts/init.freebsd"
  check_file "${ilibdir}/scripts/init.generic"
  check_file "${ilibdir}/scripts/init.rhel"
  check_file "${ilibdir}/scripts/init.solaris"
  check_file "${ilibdir}/scripts/newrelic.sysconfig"
  check_file "${ilibdir}/scripts/newrelic.cfg.template"
  check_file "${ilibdir}/scripts/newrelic.xml"
  check_file "${ilibdir}/scripts/newrelic-php5.logrotate"
  check_file "${ilibdir}/scripts/newrelic-daemon.logrotate"
fi
check_file "${ilibdir}/scripts/newrelic.ini.template"
# Check that exxtension artifacts exist for all supported PHP versions
# MAKE SURE TO UPDATE THIS LIST WHENEVER SUPPORT IS ADDED OR REMOVED
# FOR A PHP VERSION
# Currently supported versions:
#    (7.2, 7.3, 7.4)
# for x64
if [ ${arch} = x64 ]; then
for pmv in "20170718" "20180731" "20190902"; do
  check_file "${ilibdir}/agent/${arch}/newrelic-${pmv}.so"
done
fi
# Currently supported versions:
#    (8.0, 8.1, 8.2, 8.3, 8.4)
# for x64 and aarch64
if [ ${arch} = x64 ] || [ ${arch} = aarch64 ]; then
  for pmv in "20200930" "20210902" "20220829" "20230831" "20240924"; do
    check_file "${ilibdir}/agent/${arch}/newrelic-${pmv}.so"
  done
fi

if [ -n "${fmissing}" ]; then
  echo "ERROR: the following files could not be found:" >&2
  echo "${fmissing}" | sed -e 's/^/   /' >&1
fi

if [ -n "${dmissing}" -o -n "${fmissing}" ]; then
  cat <<EOF

The New Relic installation directory is incomplete. The files or
directories listed above could not be found. This usually means that
you have a corrupt installation archive. Please re-download your New
Relic software and try again. If the problem persists, please contact
${bold}https://support.newrelic.com${rmso} and report the issue. Be sure
to include all the above output in your bug report. We apologize for the
inconvenience.

EOF
  exit 1
fi

#
# Here is the authoritative list of environment variables that affect this
# script. Any time these change you MUST also update the installation FAQ,
# but try to only ever add to this list, don't change the semantic meaning
# of any of these variables, or remove any of them.
#
# NR_INSTALL_NOKSH
#   If set to anything non-empty, will prevent the installation script from
#   attempting to re-exec itself under ksh. It only does so if it finds ksh
#   in /bin or /usr/bin but if you know for sure you have a capable shell and
#   your ksh isn't the original AST ksh or isn't 100% AST ksh93p or later
#   compatible, then set this and hope that /bin/sh is a capable shell, or
#   force re-execution with a shell of your choice by using the
#   NR_INSTALL_SHELL variable described below.
#
# NR_INSTALL_SHELL
#   This script assumes some basic ksh behavior. In particular it relies on
#   The #,%,## and %% variable expansion operators. If your shell does not
#   support these, and you do not have the official ksh93p or later installed
#   as /bin/ksh or /usr/bin/ksh, then you can set this to point to a shell
#   which is suitably compatible. Both bash and zsh are known to work. This
#   option almost never needs to be set but is provided for automated install
#   environments such as Puppet to fine-tune the behavior of this script.
#   Setting this will override any attempts to automatically find ksh and
#   will force the script to re-exec itself with the specified shell, if it
#   exists and is executable.
#
# NR_INSTALL_PHPLIST
#   Colon-separated list of directories that contain PHP installations.
#   This must be the "bin" directory and contain either the CLI PHP or
#   php-config. Setting this will override the computed list of PHP
#   directories. As an alternative to setting this, you can also set
#   the NR_INSTALL_PATH variable. See below.
#
# NR_INSTALL_PATH
#   Colon-separated list of elements to add to the script's PATH. Any
#   duplicate directories in this list will be ignored and will not
#   change the position of any PATH elements. If NR_INSTALL_PHPLIST is not
#   set then the script will look for all PHP installations on the union
#   of the original PATH and this variable.
#
# NR_INSTALL_SILENT
#   If set to any non-empty value, makes the install script totally silent
#   and will not prompt the user for anything. This is most useful for
#   automated installations via tools like Puppet.
#
# NR_INSTALL_ARCH
#   If not empty MUST be one of x64. This overrides the attempt to
#   detect the architecture on which the installation is taking place. This
#   needs to be accurate. If you are installing on a 64-bit system this MUST
#   be set to x64 so that the script can correctly check for whether or not
#   your PHP is 64-bit or 32-bit.
#
# NR_INSTALL_KEY
#   If this is set, it must be set to a valid New Relic license key, and this
#   key will be set in the daemon config file.
#
# NR_INSTALL_INITSCRIPT
#   If set this must be the name under which the init script will be installed.
#   This is usually something like /etc/init.d/newrelic-daemon. If no value is
#   set this will default to /etc/init.d/newrelic-daemon on all systems except
#   OpenSolaris, on which it will be called /etc/init.d/newrelic.
#
# NR_INSTALL_DAEMONPATH
#   If set overrides the default daemon install location. Must be set to the
#   full path (including newrelic-daemon at the end).
#
# NR_INSTALL_USE_CP_NOT_LN
#   If set to any non-empty value, copy the agent and don't create a symlink.
#

#
# This script will put the log file and the sysinfo file into a tar file so
# it is ready to send to New Relic support if anything goes amiss.
#
cleanup() {
  (cd /tmp
   tar cf "${logtar}" nrinstall-${nritemp}.log nrinstall-${nritemp}.sysinfo > /dev/null 2>&1)
  rm -f ${nrilog} ${nrsysinfo} > /dev/null 2>&1
}

trap cleanup EXIT

echo "NRVERSION: ${nrversion}" >> $nrsysinfo
echo "NRVERSION: ${nrversion}" >> $nrilog

#
# Gather system information and store it in the sysinfo file.
#
echo "NR_INSTALL_LOCATION: ${NR_INSTALL_LOCATION}" >> $nrsysinfo
if [ "${arch}" = "x86" ]; then
  msg=$(
  cat << EOF

An unsupported architecture detected.
Please visit:
  https://docs.newrelic.com/docs/apm/agents/php-agent/getting-started/php-agent-compatibility-requirements/
to view compatibilty requirements for the the New Relic PHP agent.
The install will now exit.
EOF
)

  error "${msg}"
  exit 1
fi
echo "ARCH: ${arch}" >> $nrsysinfo
nruname=`uname -a 2>/dev/null`
echo "UNAME: $nruname" >> $nrsysinfo
env | sort | sed -e 's/^/ENV: /' >> $nrsysinfo

if [ -z "${ispkg}" ]; then
  #
  # If yum is installed, list the installed packages.
  #
  if [ -x /usr/bin/yum ]; then
    /usr/bin/yum list -q 2> /dev/null | sed -n -e '/installed$/ {s/^/YUM: /;s/[ 	]*installed$//;p}' >> $nrsysinfo
  fi

  #
  # If dpkg is installed, list the installed packages.
  #
  if [ -x /usr/bin/dpkg-query ]; then
    /usr/bin/dpkg-query -W -f 'DPKG: ${Package} ${Version} ${Architecture}\n' >> $nrsysinfo 2>/dev/null
  fi
fi

fatal() {
  echo "FATAL: $@" >> $nrilog
  echo "FATAL: $@" >&2
  cat >&2 <<EOF
FATAL: New Relic agent installation failed.
       Please contact ${bold}https://support.newrelic.com${rmso}
       and report the above error. We have also created a tar file with
       log files and other system info that can help solve the problem.
       If the file $logtar exists please attach it to your bug report.
       We apologize for the inconvenience.
EOF

  exit 1
}

log() {
  echo "$@" >> $nrilog
}

logcmd() {
  echo "Executing: $@" >> $nrilog
  "$@" >> $nrilog 2>&1
  ret=$?
  echo "Command returned: $ret" >> $nrilog
  return $ret
}

#
# Add the specified element to the PATH. When invoked from an installation
# package the PATH is frequently restricted. We therefore want to add a few
# common elements to the PATH for when we search for PHP installations.
#
add_to_path() {
  case "${PATH}" in
    $1:*) ;;
    *:$1) ;;
    *:$1:*) ;;
    *) [ -d "$1" ] && PATH="${PATH}${PATH_SEPARATOR}$1" ;;
  esac
}

add_to_path /usr/local/bin
add_to_path /usr/local/php
add_to_path /usr/local/php/bin
add_to_path /usr/local/zend/bin

add_to_path /usr/local/php-7.2/bin
add_to_path /usr/local/php-7.3/bin
add_to_path /usr/local/php-7.4/bin
add_to_path /usr/local/php-8.0/bin
add_to_path /usr/local/php-8.1/bin
add_to_path /usr/local/php-8.2/bin
add_to_path /usr/local/php-8.3/bin
add_to_path /usr/local/php-8.4/bin

add_to_path /opt/local/bin
add_to_path /usr/php/bin

add_to_path /usr/php-7.2/bin
add_to_path /usr/php-7.3/bin
add_to_path /usr/php-7.4/bin
add_to_path /usr/php-8.0/bin
add_to_path /usr/php-8.1/bin
add_to_path /usr/php-8.2/bin
add_to_path /usr/php-8.3/bin
add_to_path /usr/php-8.4/bin

add_to_path /usr/php/7.2/bin
add_to_path /usr/php/7.3/bin
add_to_path /usr/php/7.4/bin
add_to_path /usr/php/8.0/bin
add_to_path /usr/php/8.1/bin
add_to_path /usr/php/8.2/bin
add_to_path /usr/php/8.3/bin
add_to_path /usr/php/8.4/bin

add_to_path /opt/php/bin
add_to_path /opt/zend/bin

add_to_path /opt/php-7.2/bin
add_to_path /opt/php-7.3/bin
add_to_path /opt/php-7.4/bin
add_to_path /opt/php-8.0/bin
add_to_path /opt/php-8.1/bin
add_to_path /opt/php-8.2/bin
add_to_path /opt/php-8.3/bin
add_to_path /opt/php-8.4/bin

if [ -n "${NR_INSTALL_PATH}" ]; then
  oIFS="${IFS}"
  IFS=$PATH_SEPARATOR
  set -- $NR_INSTALL_PATH
  while [ -n "$1" ]; do
    add_to_path $1
    shift
  done
  IFS="${oIFS}"
fi
log "Final PATH=$PATH"

#
# Now search along $PATH looking for PHP installations. We look for either
# php or php-config.
#
nrphplist=
oIFS="${IFS}"
IFS=${PATH_SEPARATOR}
set -- $PATH
while [ -n "$1" ]; do
  pe=$1; shift
  if [ -x "${pe}/php-config" -a ! -d "${pe}/php-config" ]; then
    log "Found PHP in ${pe}"
    nrphplist="${nrphplist}${PATH_SEPARATOR}${pe}"
    nrphplist=${nrphplist#${PATH_SEPARATOR}}
  elif [ -x "${pe}/php" -a ! -d "${pe}/php" ]; then
    log "Found PHP in ${pe}"
    nrphplist="${nrphplist}${PATH_SEPARATOR}${pe}"
    nrphplist=${nrphplist#${PATH_SEPARATOR}}
  else
    log "No PHP found in ${pe}"
  fi
done
IFS="${oIFS}"

#
# Allow the user to override the computed list of PHP installations with their
# own list. This may be useful for people deploying the agent via Puppet etc.
#
if [ -n "$NR_INSTALL_PHPLIST" ]; then
  nrphplist=$NR_INSTALL_PHPLIST
fi

if [ -z "${NR_INSTALL_SILENT}" ]; then
  bold=`tput bold 2> /dev/null`
  rmso=`tput sgr0 2> /dev/null`
fi

#
# If we are interactive, display the banner.
#
banner() {
  if [ -z "${NR_INSTALL_SILENT}" ]; then
    clear
    echo "${bold}New Relic PHP Agent Installation (interactive mode)${rmso}"
    echo "==================================================="
    echo ""
  fi
}

getyn() {
  prompt="$1"
  valid=0

  while [ $valid -eq 0 ]; do
    echo ${en} "${prompt} ([y]es, [n]o or x to e[x]it): ${ec}"
    read choice
    case "${choice}" in
      y* | Y* )  return 0 ;;
      n* | N* )  return 1 ;;
      x* | X* | [eE][xX][iI][tT]) exit 0 ;;
      *) ;;
    esac
  done
}

log "Final PHP list is: $nrphplist"

#
# It is useful to know exactly how many elements are in the list so we compute
# that now.
#
if [ -z "${nrphplist}" ]; then
  numphp=0
else
  oIFS="$IFS"
  IFS=${PATH_SEPARATOR}
  set -- $nrphplist
  numphp=$#
  IFS="${oIFS}"
fi

choose_list() {
  prompt="$1"; shift
  upper=$1; shift
  valid=0
  chosen=

  while [ $valid -eq 0 ]; do
    echo ${en} "${prompt} (1-${upper}, 0 to exit): ${ec}"
    read choice
    case "${choice}" in
      0 | [1-9] | [1-9][0-9] | [1-9][0-9][0-9]) ;;
      *) choice= ;;
    esac
    if [ -n "${choice}" ]; then
      if [ $choice -ge 0 -a $choice -le $upper ]; then
       valid=1
      fi
    fi
  done
  chosen="${choice}"
  return $choice
}

#
# If we're interactive, display the main menu.
#
if [ -z "${imode}" ]; then
  banner
  cat <<EOF
    Please select from one of the following options:

    1)  Install ${bold}New Relic${rmso} Agent and Daemon
    2)  Uninstall ${bold}New Relic${rmso} Agent and Daemon

    0)  Exit

EOF

  choose_list "   Enter choice" 2
  case ${chosen} in
    0)  exit 0 ;;
    2)  imode="uninstall" ;;
    *)  imode="install" ;;
  esac
fi

log "Install mode: ${imode}"

#
# If we have no PHP versions in our list, exit now.
#
dodaemon=
if [ "${imode}" = "install_daemon" ] ; then
  dodaemon=yes
  nrphplist=
  numphp=0
elif [ "${imode}" = "install" -a -z "${nrphplist}" ]; then
  log "Empty PHP directory list"
  if [ -z "${NR_INSTALL_SILENT}" ]; then
    cat <<EOF

We searched and searched and couldn't find any complete enough PHP
installs. You may well have PHP installed on your system but are lacking
either the command line version or the php-config program that should
accompany each PHP install. Sometimes, these require that you install
the "dev" package for PHP or the "cli" version. We need one or the
other.

Please visit ${bold}https://newrelic.com/docs/php${rmso} and review the
installation documentation for ways in which you can customize this
script to look for PHP in non-standard locations or to do a manual
install.

EOF
    if [ -z "${ispkg}" ]; then
      if getyn "   Proceed to daemon install"; then
        dodaemon=yes
      fi
    fi
  fi
  if [ -z "${dodaemon}" ]; then
    exit 0
  fi
fi

#
# Given the list of PHP installations we have found, display a list of choices
# and allow the user to select which of the installations to work on. This is
# shared by both the install and uninstall code.
#
get_inst() {
  gin=1

  gioIFS="$IFS"
  IFS=${PATH_SEPARATOR}
  for giod in $nrphplist; do
    if [ $gin -eq $1 ]; then
      echo "${giod}"
      break
    fi
    gin=$(($gin+1))
  done
  IFS="${gioIFS}"
}

disp_get_php_list() {
  phpulist=
  if [ $numphp -eq 0 ]; then
    return 0
  fi
  if [ $numphp -eq 1 ]; then
    phpulist="${nrphplist}"
    return 0
  fi

  #
  # If we're running in silent mode, install everywhere.
  #
  if [ -n "${NR_INSTALL_SILENT}" ]; then
    phpulist="${nrphplist}"
    return 0
  fi

  pln=0
  oIFS="$IFS"
  IFS=${PATH_SEPARATOR}
  for d in $nrphplist; do
    pln=$(($pln+1))
    IFS="$oIFS"
    if [ $pln -lt 10 ]; then
      spaces="   "
    else
      spaces= "  "
    fi
    echo "   ${pln})${spaces}${d}"
  done
  IFS="$oIFS"

  echo ""
  echo "   0)   Exit"
  echo ""

  valid=0
  while [ $valid -eq 0 ]; do
    echo ${en} "   Selection (1-${pln}, 0 to exit or all): ${ec}"
    read sel
    oIFS="$IFS"
    IFS=", "
    if [ -n "$sel" ]; then
      valid=1
      for e in $sel; do
        case "$e" in
          0) exit 0 ;;
          [Aa][Ll][Ll]) phpulist="${nrphplist}" ;;
          [1-9] | [1-9][0-9] | [1-9][0-9][0-9])
            if [ $e -gt $pln ]; then
              valid=0
            else
              td=`get_inst $e`
              phpulist="${phpulist}:$td"
              phpulist=${phpulist#:}
            fi
            ;;
          *) valid=0 ;;
        esac
      done
    else
      valid=0
    fi
    IFS="$oIFS"
  done

  echo ""
  return 0
}

set_osdifile() {
  osdifile=
  if [ -n "${NR_INSTALL_INITSCRIPT}" ]; then
    osdifile="${NR_INSTALL_INITSCRIPT}"
  fi
  if [ "${ostype}" = "darwin" ]; then
    : ${osdifile:=/usr/bin/newrelic-daemon-service}
  elif [ "${ostype}" = "freebsd" -o -f /etc/arch-release ]; then
    # It is possible that this is only for freebsd.
    : ${osdifile:=/etc/rc.d/newrelic-daemon}
  else
    : ${osdifile:=/etc/init.d/newrelic-daemon}
  fi
}

#
# With OpenSolaris on Joyent, many users have a read-only /usr
# file system. On that platform, we create (if required) and always install
# to /opt/newrelic/bin.
#
set_daemon_location() {
  daemonloc=
  if [ "${ostype}" = "solaris" ]; then
    daemonloc=/opt/newrelic/bin/newrelic-daemon
  else
    daemonloc=/usr/bin/newrelic-daemon
  fi

  if [ -n "${NR_INSTALL_DAEMONPATH}" ]; then
    daemonloc="${NR_INSTALL_DAEMONPATH}"
  fi
}

#
# This code gathers into variables certain things about a particular PHP
# installation. We need this for both installing and uninstalling.
# The variables set by this function are:
#
# pi_ver
#   Version number of PHP for this installation.
# pi_bin
#   Path to the php executable for this installation. Can be blank.
# pi_extdir
#   The PHP extension directory.
# pi_extdir2
#   The directory in which the actual loadable module is to be installed,
#   or the directory from which it is to be removed. pi_extdir2 can be blank
#   and will be if it is the same as pi_extdir. However, if the CLI and DSO
#   have different extension directories, pi_extdir2 will contain the second
#   extension path. This is rare.
# pi_inifile_cli
#   The global ini file for this installation. This is for the CLI version
#   of PHP. This can be blank if we couldn't determine it.
# pi_inifile_dso
#   The global ini file for this installation. This is for the DSO version
#   that would get loaded into Apache, for example. Can be blank.
# pi_inidir_cli
#   The name of a directory scanned for extra ini files. Can be empty. This
#   is for the CLI version of PHP.
# pi_inidir_dso
#   The name of a directory scanned for extra ini files. Can be empty. This
#   is for the DSO version of PHP.
# pi_modver
#   The Zend module version for this installation.
# pi_zts
#   Set to "yes" if we can determine (or reasonably assume) that this PHP
#   installation has Zend Thread Safety (ZTS) enabled.
# pi_incdir
#   Set to the top level include directory, if it exists. Can be blank.
# pi_arch
#   An attempt to guess at the architecture of the PHP installation. On MacOS
#   and x86 systems this is always the same as $arch, but on x64 systems we
#   may have the case where they have specifically installed or compiled a
#   32-bit version of PHP/Apache for some reason, and we need to install the
#   32-bit module not the 64-bit one. It is easy to get this wrong but we do
#   the best we can. If we get it wrong, the user will need to set the
#   NR_INSTALL_ARCH and NR_INSTALL_PATH to force the architecture. If
#   NR_INSTALL_ARCH is set, it will always set the value of pi_arch, no matter
#   what we detect.
# pi_php8
#   True if PHP version is 8.0+
#
# For installation, 4 things are important:
# the extension API version, the extension load directory, and whether or not
# this version uses ZTS. These affect which version of the agent we install,
# and where. We prefer to use the CLI php for gathering this information but
# will fall back to using php-config if we must, even though it's a bit more
# complicated. The advantage of using the CLI php is that if the user has
# changed their module path with a setting, it will be reflected correctly
# whereas php-config has a static value that does not change if the user
# changes their ini file.
#
# If you are paying attention you will see the above only mentions 3 things
# we care about. The 4th is where to put the newrelic.ini fragment. This is a
# lot more complicated than we'd like. If the version of PHP being examined
# was compiled to scan an additional directory for ini files, we can safely
# put in a newrelic.ini file into that directory. If it was not compiled that
# way and only uses a single php.ini file, then we need to simply produce the
# sample newrelic.ini file and tell the user where to look for it. If only
# life was that easy.
#
# Things are further complicated by the fact that on
# many distributions, PHP is compiled twice: once for CLI and once as an
# HTTPD module or filter. They frequently have different directories that they
# scan (for example, /etc/php5/apache2 for the Apache module and /etc/php5/cli
# for the CLI version). It is common for those directories to contain a conf.d
# directory that points to a shared configuration directory, most commonly
# ../conf.d. Thus, even though the cli and the Apache modules are compiled to
# scan different directories, they both have a common directory they install
# into, and it is there that we would want to install our newrelic.ini file.
# However, there is no guarantee that there WILL be a common directory, so
# we need to check that and deal with that case, and there is no guarantee
# that there will be a directory scanned at ALL, and we need to check for that
# too.
#
# We can easily retrieve the scan directory from PHP, but the problem is
# this: we do that request using the CLI version of php. The Apache one may
# be (and most likely IS) different. We can't directly query the Apache
# directory, but in most distributions, the Apache one is compiled last, so
# the installed headers reflect the Apache scan directory and the CLI PHP
# will obviously return the CLI location.
#
# By using both methods we can make a very educated guess about these two
# directories, and check to see if they contain a "conf.d" directory that
# is a link to a shared location.
# I warned you this was more complicated than we'd like :)
#
gather_info() {
  pdir="$1"

  havecfg=
  havebin=
  pi_bin=
  pi_php8=

  #
  # Get the path to the binary.
  #
  if [ -x "${pdir}/php-config" ]; then
    havecfg=1
    pi_bin=`${pdir}/php-config --php-binary 2> /dev/null`
    if [ -z "${pi_bin}" -o ! -x "${pi_bin}" ]; then
      if [ -x "${pdir}/php" ]; then
        pi_bin="${pdir}/php"
      fi
    fi
  fi

  if [ -z "${pi_bin}" -a -x "${pdir}/php" ]; then
    pi_bin="${pdir}/php"
  fi
  log "${pdir}: pi_bin=${pi_bin}"

  phpi=
  if [ -n "${pi_bin}" ]; then
    havebin=1
    phpi=/tmp/nrinstall-$$.phpi
    rm -f ${phpi} > /dev/null 2>&1
    "${pi_bin}" -i > ${phpi} 2>&1
    log "${pdir}: php -i output follows {"
    cat ${phpi} >> ${nrilog} 2> /dev/null
    log "${pdir}: php -i output done }"
  fi

  log "${pdir}: havebin=${havebin} havecfg=${havecfg}"

  #
  # Get the version.
  #
  pi_ver=
  if [ -n "${havebin}" ]; then
    pi_ver=`echo '<?php print phpversion(); ?>' | ${pi_bin} -n -d display_errors=Off -d display_startup_errors=Off -d error_reporting=0 -q 2> /dev/null`
  else
    pi_ver=`${pdir}/php-config --version 2> /dev/null`
  fi
  log "${pdir}: pi_ver=${pi_ver}"
  if [ -z "${pi_ver}" ]; then
    log "${pdir}: couldn't determine version"
    error "could not determine the version of PHP located at:
    ${pdir}
Please consult the installation documentation to manually install New Relic
for this copy of PHP. We apologize for the inconvenience.
"
    return 1
  fi

  case "${pi_ver}" in
    7.2.*)
      ;;

    7.3.*)
      ;;

    7.4.*)
      ;;

    8.0.*)
      pi_php8="yes"
      ;;

    8.1.*)
      pi_php8="yes"
      ;;

    8.2.*)
      pi_php8="yes"
      ;;

    8.3.*)
      pi_php8="yes"
      ;;  

    8.4.*)
      pi_php8="yes"
      ;;          

    *)
      error "unsupported version '${pi_ver}' of PHP found at:
    ${pdir}
Ignoring this particular instance of PHP. Please visit:
  https://docs.newrelic.com/docs/apm/agents/php-agent/getting-started/php-agent-compatibility-requirements/
to view compatibilty requirements for the the New Relic PHP agent.
"
      log "${pdir}: unsupported version '${pi_ver}'"
      unsupported_php=1
      return 1
      ;;
  esac

  #
  # Get the extension and ini directories.
  #
  pi_extdir=
  pi_inidir_cli=
  pi_inifile_cli=
  pi_arch="${NR_INSTALL_ARCH}"
  if [ -n "${havebin}" ]; then
    #
    # Prevent daemon spawn either by disabling ini parsing or by
    # instructing the agent. If an install is in progress, we may not have
    # the correct configuration yet. If an uninstall is in progress, we
    # clearly do not want to start the very thing we're trying to remove.
    #
    pi_extdir=`echo '<?php print ini_get("extension_dir"); ?>'     | ${pi_bin} -d display_errors=Off -d display_startup_errors=Off -d error_reporting=0 -d newrelic.daemon.dont_launch=3 -q 2> /dev/null`
    pi_inidir_cli=`echo '<?php print PHP_CONFIG_FILE_SCAN_DIR; ?>' | ${pi_bin} -n -d display_errors=Off -d display_startup_errors=Off -d error_reporting=0 -q 2> /dev/null`
    pi_inifile_cli=`echo '<?php print PHP_CONFIG_FILE_PATH; ?>'    | ${pi_bin} -n -d display_errors=Off -d display_startup_errors=Off -d error_reporting=0 -q 2> /dev/null`
    if [ -z "${pi_arch}" -a "${arch}" = "x64" ]; then
      if file "${pi_bin}" 2> /dev/null | grep 'ELF 32-bit' > /dev/null 2>&1; then
        pi_arch="x86"
      fi
    fi
  else
    pi_extdir=`${pdir}/php-config --extension-dir 2> /dev/null`
  fi

  if [ -n "${pi_inidir_cli}" -a ! -e "${pi_inidir_cli}" ]; then
    logcmd mkdir -p -m 0755 ${pi_inidir_cli}
  fi

  if [ -n "${pi_inidir_cli}" -a ! -d "${pi_inidir_cli}" ]; then
    pi_inidir_cli=
  fi
  if [ -n "${pi_inifile_cli}" -a -d "${pi_inifile_cli}" ]; then
    pi_inifile_cli="${pi_inifile_cli%/}/php.ini"
  fi
  if [ -z "${pi_arch}" ]; then
    pi_arch="${arch}"
  fi

  log "${pdir}: pi_arch=${pi_arch}"

  # Check if this is a supported arch
  # Should be caught on startup but add check here to be sure
  if [ "${pi_arch}" != "x64" ] && [ "${pi_arch}" != "aarch64" ]; then
    msg=$(
    cat << EOF

An unsupported architecture detected.
Please visit:
  https://docs.newrelic.com/docs/apm/agents/php-agent/getting-started/php-agent-compatibility-requirements/
to view compatibilty requirements for the the New Relic PHP agent.
The install will now exit.
EOF
)

    error "${msg}"
    exit 1
  fi

  # This handles both 32-bit on 64-bit systems and 32-bit only systems
  if [ "${pi_arch}" = "x86" ]; then
    error "unsupported 32-bit version '${pi_ver}' of PHP found at:
    ${pdir}
Ignoring this particular instance of PHP.
"
    log "${pdir}: unsupported 32-bit version '${pi_ver}'"
    unsupported_php=1
    return 1
  fi

  if [ "${pi_arch}" = "aarch64" ] && [ "${pi_php8}" != "yes" ]; then
    error "unsupported aarch64 version '${pi_ver}' of PHP found at:
    ${pdir}
Ignoring this particular instance of PHP.
"
    log "${pdir}: unsupported aarch64 version '${pi_ver}'"
    unsupported_php=1
    return 1
  fi

  log "${pdir}: pi_inidir_cli=${pi_inidir_cli}"
  log "${pdir}: pi_inifile_cli=${pi_inifile_cli}"

  #
  # Get and check the include directory.
  #
  pi_incdir=
  if [ -n "${havecfg}" ]; then
    pi_incdir=`${pdir}/php-config --include-dir 2> /dev/null`
    [ -n "${pi_incdir}" -a -d "${pi_incdir}" -a -d "${pi_incdir}/main" -a -d "${pi_incdir}/Zend" ] || pi_incdir=
  fi
  log "${pdir}: pi_incdir=${pi_incdir}"

  #
  # Get the (possibly second) ini scan directory.
  #
  pi_inidir_dso=
  pi_inifile_dso=
  pi_extdir2=
  if [ -n "${pi_incdir}" -a -f "${pi_incdir}/main/build-defs.h" ]; then
    pi_inidir_dso=`sed -n '/^#define[ 	]PHP_CONFIG_FILE_SCAN_DIR / s,.*#define.*[ 	]"\(.*\)",\1,p' "${pi_incdir}/main/build-defs.h" 2> /dev/null`
    pi_inifile_dso=`sed -n '/^#define[ 	]PHP_CONFIG_FILE_PATH / s,.*#define.*[ 	]"\(.*\)",\1,p' "${pi_incdir}/main/build-defs.h" 2> /dev/null`
    pi_extdir2=`sed -n '/^#define[ 	]PHP_EXTENSION_DIR / s,.*#define.*[ 	]"\(.*\)",\1,p' "${pi_incdir}/main/build-defs.h" 2> /dev/null`
  fi
  if [ -n "${pi_inidir_dso}" -a ! -d "${pi_inidir_dso}" ]; then
    pi_inidir_dso=
  fi
  if [ -n "${pi_inifile_dso}" -a -d "${pi_inifile_dso}" ]; then
    pi_inifile_dso="${pi_inifile_dso%/}/php.ini"
  else
    pi_inifile_dso=
  fi
  log "${pdir}: pi_inidir_dso=${pi_inidir_dso}"
  log "${pdir}: pi_inifile_dso=${pi_inifile_dso}"

  #
  # Trim trailing /
  #
  pi_extdir="${pi_extdir%/}"
  pi_extdir2="${pi_extdir2%/}"
  if [ -z "${pi_extdir}" -a -n "${pi_extdir2}" ]; then
    pi_extdir="${pi_extdir2}"
    pi_extdir2=
  fi

  if [ -z "${pi_extdir}" ]; then
    error "could not determine extension directory for the PHP located at:
    ${pdir}
Please consult the installation documentation to manually install New Relic
for this copy of PHP. We apologize for the inconvenience.
"
    return 1
  fi

  if [ ! -d "${pi_extdir}" ]; then
    error "computed PHP extension directory:
    ${pi_extdir}
which is for the PHP installation located at:
    ${pdir}
does not exist. This particular instance of PHP will be skipped.
"
    return 1
  fi
  if [ -n "${pi_extdir2}" -a "${pi_extdir}" = "${pi_extdir2}" ]; then
    pi_extdir2=
  fi
  if [ -n "${pi_extdir2}" -a ! -d "${pi_extdir2}" ]; then
    pi_extdir2=
  fi
  log "${pdir}: pi_extdir=${pi_extdir}"
  log "${pdir}: pi_extdir2=${pi_extdir2}"

#
# Get the module API version
#
  pi_modver=
  case "${pi_ver}" in
    7.2.*)  pi_modver="20170718" ;;
    7.3.*)  pi_modver="20180731" ;;
    7.4.*)  pi_modver="20190902" ;;
    8.0.*)  pi_modver="20200930" ;;
    8.1.*)  pi_modver="20210902" ;;
    8.2.*)  pi_modver="20220829" ;;
    8.3.*)  pi_modver="20230831" ;;
    8.4.*)  pi_modver="20240924" ;;
  esac
  log "${pdir}: pi_modver=${pi_modver}"

#
# Get whether or not ZTS (Zend Thread Safety) is enabled
#
  pi_zts=
  if [ -n "${havebin}" ]; then
    if grep 'Thread Safety' ${phpi} | grep 'enabled' > /dev/null 2>&1; then
      pi_zts=yes
    elif grep 'Thread Safety' ${phpi} | grep 'disabled' > /dev/null 2>&1; then
      pi_zts=no
    fi
  fi

  if [ -z "${pi_zts}" ]; then
    if [ -n "${pincdir}" -a -f "${pincdir}/main/php_config.h" ]; then
      if grep '^#define[ 	]*ZTS[ 	]*1' "${pincdir}/main/php_config.h" > /dev/null 2>&1; then
        pi_zts="yes"
      else
        pi_zts="no"
      fi
    else
#
# If we can't find the header to check for ZTS, see if we can use the extension
# directory name to check. It will frequently have something like +zts in its
# name if ZTS is enabled.
#
      pi_zts="no"
      case "${pi_extdir}" in
      *non-zts* | *no-zts*) ;;
        *zts*)  pi_zts="yes" ;;
        *) if [ -n "${pi_extdir2}" ]; then
            case "${pi_extdir2}" in
              *non-zts* | *no-zts*) ;;
              *zts*) pi_zts="yes" ;;
            esac
          fi
          ;;
      esac
    fi
  fi
  log "${pdir}: pi_zts=${pi_zts}"

# zts installs are no longer supported
  if [ "${pi_zts}" = "yes" ]; then
    msg=$(
    cat << EOF

An unsupported PHP ZTS build has been detected. Please refer to this link:
  https://docs.newrelic.com/docs/apm/agents/php-agent/getting-started/php-agent-compatibility-requirements/
to view compatibilty requirements for the the New Relic PHP agent.
The install will now exit.
EOF
)
    error "${msg}"
    exit 1
  fi

#
# This is where we figure out where to put the ini file, if at all. We only do
# this if there is a scan directory defined. If the particular PHP installation
# does not use a scan directory and instead only uses a single ini file, we add
# that ini file to a list, and display instructions at the end of the install
# on what and how to modify that file.
#
  if [ "${pi_inifile_cli}" = "${pi_inifile_dso}" ]; then
    pi_inifile_dso=
  fi
  if [ "${pi_inidir_cli}" = "${pi_inidir_dso}" ]; then
    pi_inidir_dso=
  fi

  if [ -n "${pi_inidir_cli}" -o -n "${pi_inidir_dso}" ]; then
#
# We have either a CLI scan directory, or a DSO scan directory, or both. If we
# have both, we check to see if they are the same, and if not, we check to see
# if both of them have a "conf.d" symbolic link that points to the same place.
# If they do, we set both directories to point to that same directory, and the
# install and uninstall both know to check that the directories are not equal
# so they won't do the work twice. If we only have one or the other, or we have
# both but they both point to different places, we have nothing to do.
#
    if [ -n "${pi_inidir_cli}" -a -n "${pi_inidir_dso}" ]; then
      sdirset=
      aconfcli=`${NRIUTIL} realpath "${pi_inidir_cli%/}/conf.d"`
      aconfdso=`${NRIUTIL} realpath "${pi_inidir_dso%/}/conf.d"`
      if [ -n "${aconfcli}" -a -n "${aconfdso}" -a "${aconfcli}" -ef "${aconfdso}" ]; then
        sdirset="${aconfcli}"
      else
        aconfcli=`${NRIUTIL} realpath "${pi_inidir_cli%/}"`
        aconfdso=`${NRIUTIL} realpath "${pi_inidir_dso%/}"`
        if [ -n "${aconfcli}" -a -n "${aconfdso}" -a "${aconfcli}" -ef "${aconfdso}" ]; then
          sdirset="${aconfcli}"
        fi
      fi

      if [ -n "${sdirset}" ]; then
#
# Unset all ini files and the DSO ini directory, leaving just the CLI
# directory. This will result in this being the only place that the ini file
# gets installed.
#
        pi_inifile_cli=
        pi_inifile_dso=
        pi_inidir_dso=
        pi_inidir_cli="${sdirset}"
      fi
    fi
  fi

  # Bias directories over files.
  if [ -n "${pi_inifile_cli}" -a -n "${pi_inidir_cli}" ]; then
    pi_inifile_cli=
  fi
  if [ -n "${pi_inifile_dso}" -a -n "${pi_inidir_dso}" ]; then
    pi_inifile_dso=
  fi

  #
  # Ubuntu 13.10 has separate Apache, CLI and FPM configuration directories,
  # but our detection algorithms currently only detect the CLI directory. As a
  # stopgap, let's detect that situation and set ${pi_inidir_dso} so that the
  # INI file gets installed to the right place.
  #
  for cfg_pfx in /etc/php5 /etc/php7 /etc/php8 /etc/php/[578].*; do
    if [ "${pi_inidir_cli}" = "${cfg_pfx}/cli/conf.d" -a -z "${pi_inidir_dso}" ]; then
      #
      # Check Apache first, then FPM. If both are installed, we want FPM to win:
      # while there are many ways to end up with libapache2-mod-php5 installed,
      # it's unlikely php5-fpm will be installed unless FPM is actually in use.
      #
      if [ -d "${cfg_pfx}/apache2/conf.d" ]; then
        pi_inidir_dso="${cfg_pfx}/apache2/conf.d"
      fi

      if [ -d "${cfg_pfx}/fpm/conf.d" ]; then
        pi_inidir_dso="${cfg_pfx}/fpm/conf.d"
      fi

      #
      # Debian can use a mods-available directory to store the ini files.
      # It creates a symlink from the ini file in the conf.d directory that
      # our installer can fail to find (because the symlink is prefixed with
      # "20-" (notably the number can change based on configurations).
      # While this install script will not install into the mods-available
      # directory, our .deb installer can. Therefore, we want to detect if
      # newrelic has previously been installed in the mods-available directory
      # so that we do not create an additional ini file -- which would result in
      # the conf.d directory having both newrelic.ini and 20-newrelic.ini.
      #

      if [ -d "${cfg_pfx}/mods-available" -a -f "${cfg_pfx}/mods-available/newrelic.ini" ]; then
        pi_inidir_cli="${cfg_pfx}/mods-available"
        if [ -n "${pi_inidir_dso}" ]; then
          pi_inidir_dso="${cfg_pfx}/mods-available"
        fi
      fi
    fi
  done

  #
  # Set the list of ini files to display to the user
  #
  if [ -n "${pi_inifile_cli}" ]; then
    pi_inilist="${pi_inilist}${PATH_SEPARATOR}${pi_inifile_cli}"
    pi_inilist=${pi_inilist#${PATH_SEPARATOR}}
  fi
  if [ -n "${pi_inifile_dso}" ]; then
    pi_inilist="${pi_inilist}${PATH_SEPARATOR}${pi_inifile_dso}"
    pi_inilist=${pi_inilist#${PATH_SEPARATOR}}
  fi

  log "${pdir}: final pi_inifile_cli=${pi_inifile_cli}"
  log "${pdir}: final pi_inifile_dso=${pi_inifile_dso}"
  log "${pdir}: final pi_inidir_cli=${pi_inidir_cli}"
  log "${pdir}: final pi_inidir_dso=${pi_inidir_dso}"

  if [ -n "${phpi}" ]; then
    rm -f $phpi > /dev/null 2>&1
  fi
  return 0
}

#
# Low-level work-horse of doing the actual installation. For new installations
# we copy in the template if we are using config directories. For existing
# installations we do any upgrade work we need to on existing files (but NOT
# on php.ini files, just on newrelic.ini files in config directories.
#
install_agent_here() {
#
# First things first--we ensure that a link to the correct module exists.
#
  zts=
  istat=
  if [ "${pi_zts}" = "yes" ]; then
    zts="-zts"

    # Force copy of zts files as it is EOL so this will
    # prevent future eraseure of linked to file from
    # leading to a dangling symlink
    NR_INSTALL_USE_CP_NOT_LN=1
  fi
  srcf="${ilibdir}/agent/${pi_arch}/newrelic-${pi_modver}${zts}.so"
  destf="${pi_extdir}/newrelic.so"
  logcmd rm -f "${destf}"
  if [ -z "${NR_INSTALL_USE_CP_NOT_LN}" ]; then
    logcmd ln -sf "${srcf}" "${destf}" || {
      istat="failed"
      log "${pdir}: link '${srcf}' -> '${destf}' FAILED!"
    }
  else
    logcmd cp -f "${srcf}" "${destf}" || {
      istat="failed"
      log "${pdir}: copy '${srcf}' -> '${destf}' FAILED!"
    }
  fi

  if [ -z "${istat}" -a -n "${pi_extdir2}" ]; then
    destf="${pi_extdir2}/newrelic.so"
    logcmd rm -f "${destf}"
    if [ -z "${NR_INSTALL_USE_CP_NOT_LN}" ]; then
      logcmd ln -sf "${srcf}" "${destf}" || {
        istat="failed"
        log "${pdir}: link '${srcf}' -> '${destf}' FAILED!"
      }
    else
      logcmd cp -f "${srcf}" "${destf}" || {
        istat="failed"
        log "${pdir}: copy '${srcf}' -> '${destf}' FAILED!"
      }
    fi
  fi

  if [ -z "${istat}" ]; then
#
# Now check to see if we should copy in a sample newrelic.ini file. This will
# only ever be done if we have INI directories, and only if no newrelic.ini
# file exists in the target directory.
#
    if [ -n "${pi_inidir_cli}" -a ! -f "${pi_inidir_cli}/newrelic.ini" ]; then
      if sed -e "s/REPLACE_WITH_REAL_KEY/${nrkey}/" "${ilibdir}/scripts/newrelic.ini.template" > "${pi_inidir_cli}/newrelic.ini"; then
        logcmd chmod 644 "${pi_inidir_cli}/newrelic.ini"
        if [ -z "${NR_INSTALL_SILENT}" ]; then
          echo "      Install Status : ${pi_inidir_cli}/newrelic.ini created"
        fi
      else
        istat="failed"
        log "${pdir}: copy ini template to ${pi_inidir_cli}/newrelic.ini failed"
      fi
    fi

    if [ -z "${istat}" -a -n "${pi_inidir_dso}" -a ! -f "${pi_inidir_dso}/newrelic.ini" ]; then
      if sed -e "s/REPLACE_WITH_REAL_KEY/${nrkey}/" "${ilibdir}/scripts/newrelic.ini.template" > "${pi_inidir_dso}/newrelic.ini"; then
        logcmd chmod 644 "${pi_inidir_dso}/newrelic.ini"
        if [ -z "${NR_INSTALL_SILENT}" ]; then
          echo "      Install Status : ${pi_inidir_dso}/newrelic.ini created"
        fi
      else
        istat="failed"
        log "${pdir}: copy ini template to ${pi_inidir_dso}/newrelic.ini failed"
      fi
    fi
  fi

  if [ -n "${pi_inidir_cli}" -a -f "${pi_inidir_cli}/newrelic.ini" ]; then
    pi_nrinilist="${pi_nrinilist}${PATH_SEPARATOR}${pi_inidir_cli%/}/newrelic.ini"
    pi_nrinilist=${pi_nrinilist#${PATH_SEPARATOR}}
  fi

  if [ -n "${pi_inidir_dso}" -a -f "${pi_inidir_dso}/newrelic.ini" ]; then
    pi_nrinilist="${pi_nrinilist}${PATH_SEPARATOR}${pi_inidir_dso%/}/newrelic.ini"
    pi_nrinilist=${pi_nrinilist#${PATH_SEPARATOR}}
  fi

  if [ -z "${istat}" ]; then
    istat="OK"
  fi
  if [ -z "${NR_INSTALL_SILENT}" ]; then
    echo "      Install Status : ${istat}"
  fi
  log "${pdir}: install status: ${istat}"
}

#
# Install the agent. This is a front-end to the real work which is in
# install_agent_here - it installs the agent into a specific PHP. This front
# end function presents the user with a list of PHP locations and asks which
# of the various installations the user wants to install to. If there is only
# 1 version of PHP detected, we simply call install_agent_here with that
# version.
#
do_install() {
  if [ -n "${NR_INSTALL_KEY}" ]; then
    case "${NR_INSTALL_KEY}" in
      ????????????????????????????????????????) ;;
      *) error "NR_INSTALL_KEY environment variable invalid"
         exit 1
         ;;
    esac
  fi

  banner

  if [ ! -d /var/log/newrelic ]; then
    if [ -d /var/log ]; then
      logcmd mkdir /var/log/newrelic
      logcmd chmod 0755 /var/log/newrelic
      log "created /var/log/newrelic"
    fi
  fi

  if [ ! -d /etc/newrelic ]; then
    if ! logcmd mkdir /etc/newrelic; then
      fatal "failed to create directory /etc/newrelic"
    fi
  else
    if [ -z "${ispkg}" -a -f /etc/newrelic/newrelic.cfg ]; then
      if grep '^[ 	]*license' /etc/newrelic/newrelic.cfg > /dev/null 2>&1; then
        if ! grep 'OBSOLESCENCE NOTICE' /etc/newrelic/newrelic.cfg > /dev/null 2>&1; then
          sed -e '/^[ 	]*license/ a\
# OBSOLESCENCE NOTICE\
# The license keyword is now ignored in this file. Instead it is now\
# set by the agent (for example in your INI file for the PHP agent). As a\
# temporary measure to ensure the agent functions correctly the license above\
# has been saved in the file /etc/newrelic/upgrade_please.key. If no license\
# is specified in the agent it will use the license from that file. This is a\
# TEMPORARY measure and you are strongly encouraged to upgrade your agent\
# configuration file and remove /etc/newrelic/upgrade_please.key in order to\
# eliminate any confusion about where the license used comes from. Please also\
# remove the license keyword above and this notice.\
\
' /etc/newrelic/newrelic.cfg > /etc/newrelic/newrelic.cfg.tmp 2>/dev/null
          cp -f /etc/newrelic/newrelic.cfg.tmp /etc/newrelic/newrelic.cfg > /dev/null 2>&1
          rm -f /etc/newrelic/newrelic.cfg.tmp > /dev/null 2>&1
          nrkey=`sed -n -e 's/^[ 	]*license_key[ 	]*=[ 	]*//p' -e 's/[ 	]*$//' /etc/newrelic/newrelic.cfg 2> /dev/null`
          if [ -n "${nrkey}" ]; then
            echo "${nrkey}" > /etc/newrelic/upgrade_please.key 2> /dev/null
            chmod 644 /etc/newrelic/upgrade_please.key 2> /dev/null
          fi
        fi
      fi
    fi
  fi

  logcmd chown root /etc/newrelic
  logcmd chmod 755 /etc/newrelic

  if [ -f /etc/newrelic/upgrade_please.key ]; then
    nrkey=`head -1 /etc/newrelic/upgrade_please.key 2>/dev/null`
  fi

  if [ -n "${NR_INSTALL_KEY}" ]; then
    nrkey="${NR_INSTALL_KEY}"
  fi

  if [ -z "${NR_INSTALL_SILENT}" -a -z "${nrkey}" ]; then
    badlicense=1
    while [ "${badlicense}" = "1" ]; do
      echo ${en} "   Enter New Relic license key (or leave blank): ${ec}"
      read nrkey
      if [ -z "${nrkey}" ]; then
        badlicense=0
      else
        case "${nrkey}" in
          ????????????????????????????????????????) badlicense=0 ;;
          *) echo "Invalid license - must be exactly 40 characters" ;;
        esac
      fi
    done
  fi

  if [ -z "${nrkey}" ]; then
    nrkey="REPLACE_WITH_REAL_KEY"
  fi

  if [ $numphp -gt 1 ]; then
    if [ -z "${NR_INSTALL_SILENT}" ]; then
      cat <<EOF

Below is a list of the directories in which we found a copy of PHP.
Please select the directory or directories for which you wish to install
${bold}New Relic${rmso}. You can select either a single directory or
multiple directories by separating each choice with either a space or a
comma. To select all of the directories shown, please enter the special
keyword 'all' (without the quotes).

EOF
    fi
  fi
  disp_get_php_list

#
# Now that we have the list of PHP paths we know the user wants to install to,
# we go through each one, determine the Extension API version and module path,
# and ask the user if they want to install to that location.
#
  pi_inilist=
  pi_nrinilist=

  if [ -n "${phpulist}" ]; then
    ioIFS="$IFS"
    IFS="$PATH_SEPARATOR"
    for pdir in $phpulist; do
      IFS="${ioIFS}"
      gather_info "${pdir}" || continue

#
# Tell the user what we've found.
#
      if [ -z "${NR_INSTALL_SILENT}" ]; then
        echo "Found a valid PHP in : ${pdir}"
        echo "         PHP Version : ${pi_ver}"
        echo "  Module API version : ${pi_modver}"
        echo "    Module directory : ${pi_extdir}"
        if [ -n "${pi_extdir2}" ]; then
        echo "    Module directory : ${pi_extdir2}"
        fi
        echo "  Zend Thread Safety : ${pi_zts}"
        if [ -n "${pi_inidir_cli}" ]; then
        echo "   CLI ini directory : ${pi_inidir_cli}"
        fi
        if [ -n "${pi_inidir_dso}" ]; then
        echo "   DSO ini directory : ${pi_inidir_dso}"
        fi
        if [ -n "${pi_inifile_cli}" ]; then
        echo "        CLI ini file : ${pi_inifile_cli}"
        fi
        if [ -n "${pi_inifile_dso}" ]; then
        echo "        DSO ini file : ${pi_inifile_dso}"
        fi
      fi

      install_agent_here

      if [ -z "${NR_INSTALL_SILENT}" ]; then
        echo ""
      fi
    done
    IFS="${ioIFS}"
  fi

  pi_upglist="${pi_inilist}${PATH_SEPARATOR}${pi_nrinilist}"
  pi_upglist=${pi_upglist#${PATH_SEPARATOR}}
  pi_upglist=${pi_upglist%${PATH_SEPARATOR}}

  if [ -n "${pi_upglist}" ]; then
    ioIFS="${IFS}"
    IFS=${PATH_SEPARATOR}
    for ifile in ${pi_upglist}; do
      if [ -f "${ifile}" ]; then

        #
        # If the file already exists, and the user had previously configured
        # the agent but it does not contain a license key, and we have a
        # known license key, try to be helpful and insert the license key in
        # the file for them.
        #
        if ! grep -q '^[ 	]*newrelic.license' "${ifile}" > /dev/null 2>&1; then
          if grep -q '^[ 	]*newrelic.logfile' "${ifile}" > /dev/null 2>&1; then
            tmpifile="${ifile}.nr$$"
            sed -e "/^[ 	]*newrelic.logfile/ a\\
newrelic.license = \"${nrkey}\"
" "${ifile}" > "${tmpifile}" 2> /dev/null && cp -f "${tmpifile}" "${ifile}" > /dev/null 2>&1 && rm -f "${tmpifile}" > /dev/null 2>&1
          elif grep -q '^[ 	]\[newrelic\]' "${ifile}" > /dev/null 2>&1; then
            tmpifile="${ifile}.nr$$"
            sed -e "/^[ 	]\[newrelic\]/ a\\
newrelic.license = \"${nrkey}\"
" "${ifile}" > "${tmpifile}" 2> /dev/null && cp -f "${tmpifile}" "${ifile}" > /dev/null 2>&1 && rm -f "${tmpifile}" > /dev/null 2>&1
          else
            tffn="${ifile##*/}"
            if [ "${tffn}" = "newrelic.ini" ]; then
              echo "newrelic.license = \"${nrkey}\"" >> "${ifile}"
            else
              sed -e "s/REPLACE_WITH_REAL_KEY/${nrkey}/" "${ilibdir}/scripts/newrelic.ini.template" >> "${ifile}"
            fi
          fi
         else
          tmpifile="${ifile}.nr$$"
          sed -e "s/REPLACE_WITH_REAL_KEY/${nrkey}/" "${ifile}" > "${tmpifile}" 2> /dev/null && cp -f "${tmpifile}" "${ifile}" > /dev/null 2>&1 && rm -f "${tmpifile}" > /dev/null 2>&1
        fi
      fi
    done
    IFS="${ioIFS}"
  fi

  if [ -n "${pi_inilist}" ]; then
    echo "INI files that require manual editing (template located at ${ilibdir}/scripts/newrelic.ini.template):" >> $nrilog
    if [ -z "${NR_INSTALL_SILENT}" ]; then
      cat <<EOF

Agent installation complete. The following php.ini files need to be
modified by hand to load the ${bold}New Relic${rmso} extension, enable
it, and configure it. A sample ini file fragment can be found at:

    ${ilibdir}/scripts/newrelic.ini.template

Without performing this step the agent will not be loaded correctly. The
ini file(s) you need to modify are:

EOF
    fi
    ioIFS="${IFS}"
    IFS=${PATH_SEPARATOR}
    for ifile in ${pi_inilist}; do
      echo "    ${ifile}" >> $nrilog
      if [ -z "${NR_INSTALL_SILENT}" ]; then
        echo "    ${ifile}"
      fi
    done
    IFS="${ioIFS}"
    if [ -z "${NR_INSTALL_SILENT}" ]; then
      echo ""
    fi
  fi

#
# Now deal with the daemon.
#
  set_daemon_location

  idaemon="${dodaemon}"
  if [ -z "${ispkg}" -a -z "${idaemon}" ]; then
      idaemon=yes
  fi

  if [ "x${idaemon}" = "xyes" ]; then
    if [ "${ostype}" = "solaris" ]; then
      logcmd mkdir -p -m 0755 /opt/newrelic/bin
    fi
    if logcmd rm -f "${daemonloc}"; then
      if logcmd cp -f "${daemon}" "${daemonloc}"; then
        logcmd chmod 755 "${daemonloc}"
        log "daemon installed"
      else
        fatal "failed to copy new daemon into place"
      fi
    else
      fatal "could not remove existing daemon"
    fi
  fi

#
# Last, but not least, we need to install the init script if this was not a
# package installation (which will already have installed the correct
# script).
#
  set_osdifile

  if [ -z "${ispkg}" ]; then
    # ensure target directory exists
    if [ ! -d "$(dirname ${osdifile})" ]; then
      logcmd mkdir -p -m 0755 "$(dirname ${osdifile})"
    fi
    if logcmd cp -f "${ilibdir}/scripts/init.${ostype}" "${osdifile}"; then
      logcmd chmod 755 "${osdifile}" || {
        fatal "failed to set permissions on ${osdifile}"
      }
    else
      fatal "failed to copy daemon init script to ${osdifile}"
    fi

    sysconf=
    if [ "${ostype}" = "rhel" ]; then
      sysconf=/etc/sysconfig/newrelic-daemon
    elif [ "${ostype}" = "debian" ]; then
      sysconf=/etc/default/newrelic-daemon
    elif [ "${ostype}" = "alpine" ]; then
      sysconf=/etc/conf.d/newrelic-daemon
    fi

    if [ -n "${sysconf}" -a ! -f "${sysconf}" ]; then
      # ensure target directory exists
      if [ ! -d "$(dirname ${sysconf})" ]; then
        logcmd mkdir -p -m 0755 "$(dirname ${sysconf})"
      fi
      if logcmd cp -f "${ilibdir}/scripts/newrelic.sysconfig" "${sysconf}"; then
        logcmd chmod 755 "${sysconf}" || {
          fatal "failed to set permissions on ${sysconf}"
        }
      else
        fatal "failed to copy script to ${sysconf}"
      fi
    fi

    if [ "${ostype}" = "solaris" ]; then
      logcmd /usr/sbin/svccfg import "${ilibdir}/scripts/newrelic.xml" || {
        error "failed to install the New Relic daemon service"
      }
    fi
    logcmd cp -f "${ilibdir}/scripts/newrelic.cfg.template" /etc/newrelic/newrelic.cfg.template
  fi

  if [ -x "${osdifile}" -a -f /etc/newrelic/newrelic.cfg ]; then
    NR_SILENT=yes SILENT=yes "${osdifile}" restart > /dev/null 2>&1
  fi

  if [ -z "${NR_INSTALL_SILENT}" ]; then
    if [ -n "${dodaemon}" ]; then
      if [ "${imode}" = "install_daemon" ] ; then
        cat <<EOF

The New Relic Proxy Daemon is now installed on your system. Congratulations!

EOF
      else
        cat <<EOF

The New Relic Proxy Daemon is installed, but the agent
is not. Please point your favorite web browser at
${bold}https://docs.newrelic.com/docs/apm/agents/php-agent/installation/php-agent-installation-overview/${rmso} 
for how to install the agent by hand.

EOF
      fi
    else
      if [ -z "${unsupported_php}" ]; then
        cat <<EOF

${bold}New Relic${rmso} is now installed on your system. Congratulations!

EOF
      else
        cat <<EOF

${bold}New Relic${rmso} installation encountered at least one unsupported
version of PHP. If the errors above are acceptable, please continue your
installation:

EOF
      fi
      cat <<EOF

1. Set newrelic.appname in your newrelic.ini file.

2. Restart your web server. This will fix most reporting issues and
   load the agent's new features and bug fixes.

If you have questions or comments, go to https://support.newrelic.com.

EOF
    fi
  fi
  return 0
}

#
# Uninstallation is a great deal simpler, as we leave all config files in place
# in case this is an upgrade. In cases where a user really wants to remove all New Relic
# artifacts from their system, they can run this script with the "purge" command and
# it will do both a normal uninstall AND remove any config files it created.
#

purge=0
remove_if_unchanged() {
  target="$1"
  source="$2"
  dorm=1

  if [ -f "${target}" -a -f "${source}" ]; then
    cmp "${source}" "${target}" > /dev/null 2>&1 || dorm=0
  else
    dorm=0
  fi

  if [ "x${dorm}" = "x1" -o "x${purge}" = "x1" ]; then
    logcmd rm -f "${target}"
  fi
}

remove_from_here() {
  destf="${pi_extdir}/newrelic.so"
  if [ -f "${destf}" ]; then
    logcmd rm -f "${destf}"
  fi

  if [ -n "${pi_extdir2}" ]; then
    destf="${pi_extdir2}/newrelic.so"
    if [ -f "${destf}" ]; then
      logcmd rm -f "${destf}"
    fi
  fi

  if [ -n "${pi_inidir_cli}" -a -f "${pi_inidir_cli}/newrelic.ini" ]; then
    remove_if_unchanged "${pi_inidir_cli}/newrelic.ini" "${ilibdir}/scripts/newrelic.ini.template"
  fi

  if [ -n "${pi_inidir_dso}" -a -f "${pi_inidir_dso}/newrelic.ini" ]; then
    remove_if_unchanged "${pi_inidir_dso}/newrelic.ini" "${ilibdir}/scripts/newrelic.ini.template"
  fi
}

do_uninstall() {
  banner

  phpulist="${nrphplist}"

#
# Remove new .ini and .so links for the versions we were told to.
#
  pi_inilist=

  if [ -n "${phpulist}" ]; then
    ioIFS="$IFS"
    IFS=${PATH_SEPARATOR}
    for pdir in $phpulist; do
      IFS="${ioIFS}"
      gather_info "${pdir}" || continue

      # keep deprecated/eol module if requested
      if [ -n "${NR_UNINSTALL_KEEP_ZTS}" ]; then
        if [ -z "${NR_INSTALL_SILENT}" ]; then
          echo "Keeping module in ${pdir} because NR_UNINSTALL_KEEP_ZTS set."
        fi
        continue
      fi

      if [ -z "${NR_INSTALL_SILENT}" ]; then
        echo "Removing from this PHP : ${pdir}"
        echo "      Module directory : ${pi_extdir}"
        if [ -n "${pi_extdir2}" ]; then
        echo "      Module directory : ${pi_extdir2}"
        fi
      fi

      remove_from_here

      if [ -z "${NR_INSTALL_SILENT}" ]; then
        echo ""
      fi
    done
    IFS="${ioIFS}"
  fi

#
# Remove init script.
#
  set_osdifile

  if [ -x "${osdifile}" ]; then
    NR_SILENT=yes SILENT=yes "${osdifile}" stop > /dev/null 2>&1
  fi

  if [ -z "${ispkg}" ]; then
    remove_if_unchanged "${osdifile}" "${ilibdir}/scripts/init.${ostype}"
  fi

#
# Remove daemon config file.
#
  remove_if_unchanged /etc/newrelic/newrelic.cfg "${ilibdir}/scripts/newrelic.cfg.template"

#
# Remove the daemon.
#
  if [ -z "${ispkg}" ]; then
    set_daemon_location
    logcmd rm -f "${daemonloc}"
  fi

  if [ -z "${NR_INSTALL_SILENT}" -a "x${purge}" = "x0" ]; then
    cat <<EOF

Removal of ${bold}New Relic${rmso} complete. If you are removing in
order to perform an upgrade, run the installation script from the new
distribution. All existing configuration files that were modified
have been left in place. If you wish to permanently remove ${bold}New
Relic${rmso}, you may wish to invoke this script with the 'purge'
option to completely remove ${bold}New Relic${rmso}. If so, please take
a moment to mail ${bold}https://support.newrelic.com${rmso} telling us
why you are leaving us and what we can do to help you or improve your
experience. Thank you in advance.

EOF
  fi
}

do_purge() {
  purge=1
  do_uninstall

#
# Remove /etc/newrelic if it's empty.
#
  if [ -d /etc/newrelic ]; then
    logcmd rmdir /etc/newrelic
  fi
#
# Remove the default log directory.
#
  if [ -d /var/log/newrelic ]; then
    logcmd rm -fr /var/log/newrelic
  fi

  if [ -z "${NR_INSTALL_SILENT}" ]; then
    cat <<EOF

We are sorry to see you go! If there is anything we can do
to improve your experience with the product, please mail
${bold}https://support.newrelic.com${rmso} and we will do our very best
to help you or to improve the product to meet your expectations. Thank
you in advance. -- The ${bold}New Relic${rmso} ${agent} Team

EOF
  fi
  return 0
}

[ "${imode}" = "install" ] && do_install
[ "${imode}" = "install_daemon" ] && do_install
[ "${imode}" = "uninstall" ] && do_uninstall
[ "${imode}" = "purge" ] && do_purge

exit 0
