# U-Boot for the Fairphone 2 and Fairphone 3

The `main` branch is upstream U-Boot plus the patches that support the
Fairphone 2 (MSM8974) and Fairphone 3 / 3+ (SDM632). Both phones keep their
stock LK, which loads U-Boot from the `boot` partition in place of a kernel.
Board documentation: `doc/board/qualcomm/fairphone-fp2.rst` and
`doc/board/qualcomm/fairphone-fp3.rst`.

The U-Boot patches carry no Citronics-specific changes, so they can be sent
upstream as they are. This directory and `.github/workflows/citronics.yml`
are the only additions on top of them. `main` follows upstream by rebasing
these patches onto upstream's `main`.

## Building

On Ubuntu 26.04:

    sudo apt-get install build-essential gcc-arm-linux-gnueabihf \
        gcc-aarch64-linux-gnu bison flex bc python3 python3-dev \
        python3-setuptools python3-pyelftools swig libssl-dev \
        libgnutls28-dev uuid-dev xxd
    citronics/build.sh            # both phones, into build-citronics/
    citronics/build.sh fp2 -o /tmp/fp2

Other cross compilers: set `CROSS_COMPILE_ARM` (Fairphone 2) and
`CROSS_COMPILE_ARM64` (Fairphone 3).

## Installing

Boot it from RAM first, then flash it:

    fastboot boot u-boot-fp2.img
    fastboot flash boot u-boot-fp2.img

The Fairphone 3 also needs an empty overlay in its `dtbo` partition; see
`fairphone-fp3.rst`.

## Releases

Every push to `main` builds both images and publishes them, with their
`SHA256SUMS`, as the GitHub release `citronics-<U-Boot version>`; the latest
release is always the current `main`. Pull requests are built only.
