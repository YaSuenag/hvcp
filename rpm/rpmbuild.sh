#!/bin/bash

REPO=`dirname $0`/../

# Setup
dnf install -y rpmdevtools gcc-g++ git
rpmdev-setuptree

# Prepare to build
cd $REPO
git archive --format=tgz --prefix=hvcp/ -o ~/rpmbuild/SOURCES/hvcp.tar.gz HEAD
cp rpm/hvcp-server-makefile.patch ~/rpmbuild/SOURCES/
cp rpm/hvcp.spec ~/rpmbuild/SPECS/

# Build
cd ~/rpmbuild/SPECS
rpmbuild -ba hvcp.spec

# Copy RPMs to output dir
cd ~/rpmbuild
cp -fR RPMS/* SRPMS/* /out
