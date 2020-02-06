#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
test -e sbin/kresc
sbin/kresc  # command will fail because of invalid parameters
test "$?" -eq 1  # linker error would have different exit code
