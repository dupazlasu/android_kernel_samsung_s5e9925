#!/bin/bash

abort()
{
    cd -
    echo "-----------------------------------------------"
    echo "Kernel compilation failed! Exiting..."
    echo "-----------------------------------------------"
    exit -1
}

unset_flags()
{
    cat << EOF
Usage: $(basename "$0") [options]
Options:
    -m, --model [value]    Specify the model code of the phone
    -k, --ksu [y/N]        Include KernelSU
    -r, --recovery [y/N]   Compile kernel for an Android Recovery	
    -p, --permissive [y/N]      Force SELinux status to permissive and add superuser driver, DO NOT USE UNLESS A DEV!															 
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --model|-m)
            MODEL="$2"
            shift 2
            ;;
        --ksu|-k)
            KSU_OPTION="$2"
            shift 2
            ;;
        --permissive|-p)
            PERMISSIVE_OPTION="$2"
            shift 2
            ;;
        --recovery|-r)
            RECOVERY_OPTION="$2"
            shift 2
            ;;
        *)\
            unset_flags
            exit 1
            ;;
    esac
done

echo "Preparing the build environment..."

pushd $(dirname "$0") > /dev/null
CORES=`cat /proc/cpuinfo | grep -c processor`

# Define toolchain variables
CLANG_DIR=$PWD/toolchain/clang-r596125
PATH=$CLANG_DIR/bin:$PATH

# Check if toolchain exists
if [ ! -f "$CLANG_DIR/bin/clang-22" ]; then
    echo "-----------------------------------------------"
    echo "Toolchain not found! Downloading..."
    echo "-----------------------------------------------"
    rm -rf $CLANG_DIR
    mkdir -p $CLANG_DIR
    pushd $CLANG_DIR > /dev/null
    curl -LJOk https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/mirror-goog-main-llvm-toolchain-source/clang-r596125.tar.gz
    tar xf linux-x86-mirror-goog-main-llvm-toolchain-source-clang-r596125.tar.gz
    rm linux-x86-mirror-goog-main-llvm-toolchain-source-clang-r596125.tar.gz
    echo "Cleaning up..."
    popd > /dev/null
fi

MAKE_ARGS="
LLVM=1 \
LLVM_IAS=1 \
ARCH=arm64 \
O=out
"

# Define specific variables
case $MODEL in
r0s)
    BOARD=SRPUH13A011
;;
g0s)
    BOARD=SRPUG08A011
;;
b0s)
    BOARD=SRPUH13B009
;;
*)
    unset_flags
    exit
esac

if [[ "$RECOVERY_OPTION" == "y" ]]; then
    RECOVERY=recovery.config
    KSU_OPTION=n
fi

if [[ "$KSU_OPTION" == "y" ]]; then
    KSU=ksu.config
fi

if [[ "$PERMISSIVE_OPTION" == "y" ]]; then
    PERMISSIVE=debug.config
fi

rm -rf build/out/$MODEL
mkdir -p build/out/$MODEL/zip/files
mkdir -p build/out/$MODEL/zip/META-INF/com/google/android

build_kernel() {
    # Build kernel image
    echo "-----------------------------------------------"
    echo "Defconfig: "s5e9925_defconfig""

    if [ -z "$KSU" ]; then
        echo "KSU: N"
    else
        echo "KSU: $KSU"
    fi

    if [ -z "$RECOVERY" ]; then
    echo "Recovery: N"
    else
        echo "Recovery: Y"
    fi

    if [ -z "$PERMISSIVE" ]; then
        echo "PERMISSIVE: No"
    else
        echo "PERMISSIVE: Yes"
    fi

    echo "-----------------------------------------------"
    echo "Building kernel using "s5e9925_defconfig""
    echo "Generating configuration file..."
    echo "-----------------------------------------------"
    make ${MAKE_ARGS} -j$CORES s5e9925_defconfig $MODEL.config $RECOVERY $KSU $PERMISSIVE || abort

    echo "Building kernel..."
    echo "-----------------------------------------------"
    make ${MAKE_ARGS} -j$CORES || abort
}

build_boot() {

    cp -a out/arch/arm64/boot/Image build/out/$MODEL

    if [ -z "$RECOVERY" ]; then			   
    echo "-----------------------------------------------"
    echo "Building boot.img RAMDisk..."
    mkdir -p build/out/$MODEL/boot_ramdisk00

    # Copy common files for boot.img's RAMDisk
    cp -a build/ramdisk/boot/boot_ramdisk00 build/out/$MODEL
    
    # Copy device build.prop file for boot.img's RAMDisk
    cp -a build/ramdisk/boot/device/$MODEL/* build/out/$MODEL/boot_ramdisk00/system/etc/ramdisk

    pushd build/out/$MODEL/boot_ramdisk00 > /dev/null
    find . ! -name . | LC_ALL=C sort | cpio -o -H newc -R root:root | lz4 -l > ../boot_ramdisk || abort
    popd > /dev/null

    echo "-----------------------------------------------"
    echo "Building boot.img..."

    OUTPUT_FILE=build/out/$MODEL/boot.img
    RAMDISK_00=build/out/$MODEL/boot_ramdisk
    KERNEL=build/out/$MODEL/Image
    HEADER_VERSION=4
    OS_VERSION=16.0.0
    OS_PATCH_LEVEL=2026-03
    CMDLINE=""

	python3 toolchain/mkbootimg/mkbootimg.py --header_version $HEADER_VERSION --cmdline "$CMDLINE" --ramdisk $RAMDISK_00 \
	--os_version $OS_VERSION --os_patch_level $OS_PATCH_LEVEL --kernel $KERNEL --output $OUTPUT_FILE || abort
	fi  
}

build_dtb() {
    echo "-----------------------------------------------"
    echo "Building DTB image..."
    ./toolchain/mkdtimg cfg_create build/out/$MODEL/dtb.img build/dtconfigs/s5e9925.cfg -d out/arch/arm64/boot/dts/exynos || abort 

    echo "-----------------------------------------------"
    echo "Building DTBO image..."
    ./toolchain/mkdtimg cfg_create build/out/$MODEL/dtbo.img build/dtconfigs/$MODEL.cfg -d out/arch/arm64/boot/dts/samsung/$MODEL || abort
    
}

build_modules() {
    MODULES_FOLDER=modules
    rm -rf out/$MODULES_FOLDER

    echo "-----------------------------------------------"
    echo "Building modules..."
    # Strip modules and place them in modules folder
    make ${MAKE_ARGS} INSTALL_MOD_PATH=$MODULES_FOLDER INSTALL_MOD_STRIP=1 modules_install || abort
    
    # Now we run depmod to update the dep/softdep files
    # For this we need the kernel version
    KERNEL_DIR_PATH=$(find "out/$MODULES_FOLDER/lib/modules" -maxdepth 1 -type d -name "5.10*") || abort
    KERNEL_VERSION=$(basename $KERNEL_DIR_PATH) || abort

    # And finally depmod itself
    depmod -a -b out/$MODULES_FOLDER $KERNEL_VERSION || abort

    # depmod updates modules.alias, modules.dep and modules.softdep
    # But the module order is not updated by depmod
    # We have to remove the filenames ourselves
    # But first, our vendor_boot needs modules.order definitions that go like:
    # fingerprint.ko
    # Clang generates modules.order definitions like this
    # kernel/drivers/fingerprint/fingerprint.ko
    # So we sed the file to adapt
    sed -i 's/.*\///g' $KERNEL_DIR_PATH/modules.order 

    # Now we sed the bad filenames out of the file with a loop
    for FILENAME in $FILENAMES; do
        sed -i "/$FILENAME/d" "$KERNEL_DIR_PATH/modules.order"
    done

    # Now we have to order the modules
    # Samsung was nice enough to give us the order in vendor_boot_module_order_s5e9925.cfg
    # exynos-chipid_v2.ko exynos-reboot.ko sec_debug_base_early.ko clk_exynos.ko exynos_mct_v2.ko s3c2410_wdt.ko sec_debug_mode.ko
    # These files have to be at the top of modules.order in this order, and then we can keep the default order.
    # Samsung wants the file to be renamed to modules.load anyways, so we will craft our own modules.load file based on modules.order
    touch $KERNEL_DIR_PATH/modules.load

    INITIAL_ORDER="
    exynos-chipid_v2.ko
    exynos-reboot.ko
    sec_debug_base_early.ko
    clk_exynos.ko
    exynos_mct_v2.ko
    s3c2410_wdt.ko
    sec_debug_mode.ko
    "

    # First we add the order from Samsung into our new modules.load
    # And we sed it out of modules.order
    for LINE in $INITIAL_ORDER; do
        echo $LINE >> $KERNEL_DIR_PATH/modules.load
        sed -i "/$LINE/d" "$KERNEL_DIR_PATH/modules.order"
    done

    # Now we add the remaining lines from modules.order into modules.load
    while IFS= read -r line; do
        echo "$line" >> "$KERNEL_DIR_PATH/modules.load"
    done < "$KERNEL_DIR_PATH/modules.order"

    # Now we have to also modify modules.dep
    # Android generates them like this
    # kernel/drivers/dma/samsung-dma.ko: kernel/drivers/dma/pl330.ko
    # But Samsung wants them like this
    # /lib/modules/samsung-dma.ko: /lib/modules/pl330.ko
    # So we will format it with sed
    sed -i 's/\(kernel\/[^: ]*\/\)\([^: ]*\.ko\)/\/lib\/modules\/\2/g' "$KERNEL_DIR_PATH/modules.dep"

    # Now the modules and their configuration descriptor files are ready, we move them to a folder and create the new second ramdisk
    # The second ramdisk should contain a /lib/modules where the modules are located
    mkdir -p build/out/$MODEL/modules/lib/modules

    find $KERNEL_DIR_PATH -name '*.ko' -exec cp '{}' build/out/$MODEL/modules/lib/modules ';'

    if [ "$MODEL" == r0s ]; then 
        sed -i /wlan\.ko/d "$KERNEL_DIR_PATH/modules.load"
        echo "wlan.ko" >> "$KERNEL_DIR_PATH/modules.load"
    fi

    # We also copy the module configuration descriptors
    cp $KERNEL_DIR_PATH/modules.{alias,dep,softdep,load} build/out/$MODEL/modules/lib/modules

}

build_vendor_boot() {
    echo "-----------------------------------------------"
    echo "Building vendor_boot RAMDisks..."
    # Copy common vendor_ramdisk00 files to build/out
    cp -a build/ramdisk/vendor_boot/ramdisk00 build/out/$MODEL/vendor_ramdisk00
    
    # Copy device firmware files for vendor_ramdisk00
    cp -a build/ramdisk/vendor_boot/vendor_firmware/$MODEL/* build/out/$MODEL/vendor_ramdisk00

    # Pack RAMDisks
    # vendor_ramdisk == ramdisk00
    # modules_ramdisk == ramdisk01
    pushd build/out/$MODEL/vendor_ramdisk00 > /dev/null
    find . ! -name . | LC_ALL=C sort | cpio -o -H newc -R root:root | lz4 -l > ../$MODEl/vendor_ramdisk || abort
    popd > /dev/null

    pushd build/out/$MODEL/modules > /dev/null
    find . ! -name . | LC_ALL=C sort | cpio -o -H newc -R root:root | lz4 -l > ../modules_ramdisk || abort
    popd > /dev/null

    echo "-----------------------------------------------"
    echo "Building vendor_boot image..."

    OUTPUT_FILE=build/out/$MODEL/vendor_boot.img
    DTB_PATH=build/out/$MODEL/dtb.img
    BOOTCONFIG_PATH=build/ramdisk/bootconfig
    RAMDISK_00=build/out/$MODEL/vendor_ramdisk
    RAMDISK_01=build/out/$MODEL/modules_ramdisk
    HEADER_VERSION=4
    BASE=0x00000000
    PAGESIZE=0x00001000
    KERNEL_OFFSET=0x10008000
    RAMDISK_OFFSET=0x14000000
    TAGS_OFFSET=0x10000000
    DTB_OFFSET=0x0000000011F00000
    CMDLINE="bootconfig loop.max_part=7"

    python3 toolchain/mkbootimg/mkbootimg.py --header_version $HEADER_VERSION --pagesize $PAGESIZE --base $BASE --kernel_offset $KERNEL_OFFSET \
	--ramdisk_offset $RAMDISK_OFFSET --tags_offset $TAGS_OFFSET --dtb_offset $DTB_OFFSET --vendor_cmdline "$CMDLINE" --board $BOARD --dtb $DTB_PATH  \
	--ramdisk_type 1 --ramdisk_name "" --vendor_ramdisk_fragment $RAMDISK_00 --vendor_bootconfig $BOOTCONFIG_PATH --ramdisk_type 3 \
	--ramdisk_name dlkm --vendor_ramdisk_fragment $RAMDISK_01 --vendor_boot $OUTPUT_FILE || abort
}

build_zip() {
    echo "-----------------------------------------------"
    echo "Building zip..."
    cp build/out/$MODEL/boot.img build/out/$MODEL/zip/files/boot.img
    cp build/out/$MODEL/vendor_boot.img build/out/$MODEL/zip/files/vendor_boot.img
    cp build/out/$MODEL/dtbo.img build/out/$MODEL/zip/files/dtbo.img
    cp build/update-binary build/out/$MODEL/zip/META-INF/com/google/android/update-binary
    cp build/updater-script build/out/$MODEL/zip/META-INF/com/google/android/updater-script

    version=$(grep -o 'CONFIG_LOCALVERSION="[^"]*"' arch/arm64/configs/s5e9925_defconfig | cut -d '"' -f 2)
    version=${version:1}
    pushd build/out/$MODEL/zip > /dev/null
    DATE=`date +"%d-%m-%Y_%H-%M-%S"`

    if [[ "$KSU_OPTION" == "y" ]]; then
        NAME="$version"_"$MODEL"_UNOFFICIAL_KSU_"$DATE".zip
    else
        NAME="$version"_"$MODEL"_UNOFFICIAL_"$DATE".zip
    fi
    zip -r -qq ../"$NAME" .
    popd > /dev/null
}

build_kernel
build_boot
build_dtb
build_modules

if [ -z "$RECOVERY" ]; then				   
build_vendor_boot
build_zip
fi 

popd > /dev/null
echo "-----------------------------------------------"
echo "Build finished successfully!"
