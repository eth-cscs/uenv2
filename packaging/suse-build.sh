#!/usr/bin/env bash

set -euo pipefail

usage="$(basename "$0") [-h]  [--slurm-version] [--remote] -r,--ref dstdir

Helper script downloads required slurm headers and calls rpmbuild-wrapper.sh.

Options:
-h,--help          show this help text
-r,--ref           git reference (default HEAD implies use the current source)
--remote           uenv github repo (defaults to eth-cscs/uenv2)
--slurm-version    slurm version to use
"

# A temporary variable to hold the output of `getopt`
TEMP=$(getopt -o r:h --long ref:,remote:,help,slurm-version: -- "$@")

# default Slurm version is 0 -> use system slurm
slurm_version="00.00.0"
git_remote=https://github.com/eth-cscs/uenv2

# If getopt has reported an error, exit script with an error
if [ $? != 0 ]; then
    # echo 'Error parsing options' >&2
    echo "${usage}" >&2
    exit 1
fi

eval set -- "$TEMP"

git_ref=HEAD

# Now go through all the options
while true; do
    case "$1" in
    -r | --ref)
        shift
        git_ref="$1"
        shift
        ;;
    --remote)
        shift
        git_remote="$1"
        shift
        ;;
    --slurm-version)
        shift
        slurm_version="$1"
        shift
        ;;
    -h | --help)
        shift
        echo "${usage}"
        exit 1
        ;;
    --)
        shift
        break
        ;;
    *)
        echo "Internal error! $1"
        exit 1
        ;;
    esac
done

scriptdir=$(realpath "$(dirname "${BASH_SOURCE[0]}")")

arch=$(uname -m)

workdir=$(mktemp -d)
builddir=$workdir/rpmbuild

# set up the build directory, where the RPM will be created
rm -rf "${builddir}"
mkdir -p "${builddir}"
(
    cd $builddir
    mkdir -p BUILD BUILDROOT RPMS SOURCES SPECS SRPMS
)

# create a tar ball of the source code and store it in the build path
# if ref is HEAD, use HEAD in this checkout of the repository
# otherwise, clone 
if [[ "$git_ref" == "HEAD" ]]; then
    srcdir=$(realpath ${scriptdir}/../source)
else
    srcdir=$workdir/src
    git clone --quiet --branch $git_ref $git_remote $srcdir
fi

uenv_version=$(sed 's/-.*//' "${srcdir}/VERSION")

echo '======================================================='
echo "uenv_version   $uenv_version"
echo "slurm_version  $slurm_version"
echo "scriptdir      $scriptdir"
echo "branch         $git_ref"
echo "arch           $arch"
echo '======================================================='

tarball=uenv-"${uenv_version}".tar.gz
(
    cd "${srcdir}"
    git archive --format=tar.gz --output="${builddir}/SOURCES/${tarball}" HEAD
)

(
  cd "${builddir}"

  # configure the RPM spec file
  cp "${scriptdir}/uenv.spec" SPECS/
  sed -i "s|UENV_VERSION|${uenv_version}|g" SPECS/uenv.spec
  sed -i "s|SLURM_VERSION|${slurm_version}|g" SPECS/uenv.spec

  flags="-O2 -fmessage-length=0 -D_FORTIFY_SOURCE=2 -fstack-protector"
  export LDFLAGS="-Wl,--disable-new-dtags -Wl,-rpath,/lib64 -Wl,-rpath,/usr/lib64"
  export CXXFLAGS="$flags"
  export CFLAGS="$flags"
  export CC=gcc-12
  export CXX=g++-12

  # create src rpm
  rpmbuild -bs --define "_topdir ." SPECS/uenv.spec

  # create binary rpm
  rpmbuild -vv --nodeps --define "_topdir $(pwd)" --define "_slurm_version ${slurm_version}" \
         --rebuild SRPMS/uenv-*.src.rpm
)

# todo: copy the RPM, whatever it was named by the build too, to a generic name
# that a caller can then copy into a full spec path

rpm_builddir=$builddir/RPMS
rpm_name=uenv-${uenv_version}-${slurm_version}.${arch}.rpm
rpm_fullpath=$rpm_builddir/$arch/$rpm_name

mkdir -p ${scriptdir}/rpms/$arch
set -x
echo "==== copying rpms/$arch/uenv-${uenv_version}-${slurm_version}.rpm"
cp $rpm_fullpath ${scriptdir}/rpms/$arch/uenv-${uenv_version}-${slurm_version}.rpm

