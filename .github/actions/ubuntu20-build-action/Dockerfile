FROM ubuntu:20.04

# Uncomment deb-src lines for all enabled repos. First part of single-quoted
# string (up the the !) is the pattern of the lines that will be ignored.
# Needed for apt-get build-dep call later in script
RUN sed -Ei '/.*partner/! s/^# (deb-src .*)/\1/g' /etc/apt/sources.list

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y build-essential

# PHP dependencies
RUN apt-get build-dep -y php7.4
RUN apt-get install -y libmysqlclient-dev php-dev libmcrypt-dev libphp7.4-embed

# Other tools
RUN apt-get install -y curl gdb valgrind libcurl4-openssl-dev pkg-config postgresql libpq-dev libedit-dev libreadline-dev git

COPY build.sh /build.sh

ENTRYPOINT ["/build.sh"]
