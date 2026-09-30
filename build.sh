#!/data/data/com.termux/files/usr/bin/bash

TERMUX_PKG_HOMEPAGE=https://github.com/fahdomer370-byte/isa
TERMUX_PKG_DESCRIPTION="ISA programming language runtime and compiler"
TERMUX_PKG_LICENSE="MIT"
TERMUX_PKG_LICENSE_FILE="LICENSE"
TERMUX_PKG_MAINTAINER="fahdomer370-byte"
TERMUX_PKG_VERSION=0.1.0
TERMUX_PKG_SRCURL="git+https://github.com/fahdomer370-byte/isa.git"
TERMUX_PKG_GIT_BRANCH="v${TERMUX_PKG_VERSION}"
TERMUX_PKG_BUILD_IN_SRC=true

termux_step_make() {
    make \
        CC="$CC" \
        CFLAGS="$CFLAGS -Iinclude" \
        LDFLAGS="$LDFLAGS"
}

termux_step_make_install() {
    make PREFIX="$TERMUX_PREFIX" install
}