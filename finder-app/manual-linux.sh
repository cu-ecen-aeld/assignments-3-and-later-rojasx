#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"

cd "$OUTDIR"
############################################
# Get linux

if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    # make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

############################################
echo "Adding the Image in outdir"
    
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

############################################
echo "Creating the staging directory for the root filesystem"

cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

############################################
echo "Creating rootfs!"

cd "$OUTDIR"
mkdir -p "rootfs"
cd "rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

############################################
# Busybox
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    echo "Getting busybox!"
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
else
    cd busybox
fi

############################################
echo "Building busybox!"

make distclean
make defconfig
echo "Defconfig completed!"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
echo "Build completed!"
make CONFIG_PREFIX=/tmp/aeld/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install
echo "Install completed!"

############################################
echo "Library dependencies"

cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

echo "Copying libraries from arm-cross-compiler"
cp /home/xavier/arm-cross-compiler/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib/ld-linux-aarch64.so.1 lib64/
cp /home/xavier/arm-cross-compiler/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libm.so.6 lib64/
cp /home/xavier/arm-cross-compiler/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libresolv.so.2 lib64/
cp /home/xavier/arm-cross-compiler/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/lib64/libc.so.6 lib64/

############################################
echo "Making device nodes!"

sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

############################################
echo "Ownership of rootfs to root"

cd "$OUTDIR"
sudo chown -R root:root rootfs 

############################################
echo "Making and zipping writer utility!"

cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > /tmp/aeld/initramfs.cpio
cd "$OUTDIR"
gzip -f initramfs.cpio
