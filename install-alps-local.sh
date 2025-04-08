#!/bin/bash

set -e

arch=$(uname -m)
echo "== architecture: $arch"

root=$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)
build=$root/build-alps-$arch
pyenv=$root/pyenv-alps-$arch
install=$HOME/.local/$arch
uenv_version=$(cat VERSION)

rm -rf $build
rm -rf $pyenv
echo "== configure in $build"

export CC=gcc-12
export CXX=g++-12
uvx --with meson,ninja meson setup --prefix=$install $build $root
uvx --with meson,ninja meson compile -C$build
uvx --with meson,ninja meson install -C$build --skip-subprojects

echo ""
echo "== succesfully installed"

echo "======================================================================================="
echo "add the following to your ~/.bashrc (or equivalent for .zshrc):"
echo ""
echo "Note:"
echo "- log out and in again for the changes to take effect"
echo "- if successful, 'uenv --version' will report version ${uenv_version}"
echo ""
echo "export PATH=$install/bin:\$PATH"
echo "unset -f uenv"
