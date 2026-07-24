#!/bin/bash

ABORT()
{
    echo "-----------------------------------------------"
    echo "Kernel compilation failed! Exiting..."
    echo "-----------------------------------------------"
    exit 1
}

_PRINT_HELP()
{
    echo "Usage: $(basename "$0") <-m, --model model> [options]"
    echo "Options:"
    echo "-m, --model [value]    Specify the model code of the phone"
    echo "-k, --ksu              Include KernelSU Next"
    echo "-r, --recovery         Compile kernel for an Android Recovery"	
    echo "-p, --permissive       Force SELinux status to permissive and add superuser driver, DO NOT USE UNLESS A DEV!"
}

MODEL=""
FRAGMENTS=""
KSU=false
PERMISSIVE=false
RECOVERY=false

while [[ $# -gt 0 ]]; do
    if [[ "$1" == "-m" ]] || [[ "$1" == "--model" ]]; then
        MODEL="$2"
        FRAGMENTS+="$MODEL.config "
        shift
    elif [[ "$1" == "-k" ]] || [[ "$1" == "--ksu" ]]; then
        KSU=true
    elif [[ "$1" == "-p" ]] || [[ "$1" == "--permissive" ]]; then
        PERMISSIVE=true
    elif [[ "$1" == "-r" ]] || [[ "$1" == "--recovery" ]]; then
        RECOVERY=true
    else
        _PRINT_HELP
        exit 1
    fi

    shift
done

echo "Preparing the build environment..."

pushd "$(dirname "$0")" > /dev/null || exit 1

# Define toolchain variables
CLANG_DIR="$(pwd)/toolchain/clang-r596125"
PATH="$CLANG_DIR/bin:$PATH"

# Check if toolchain exists
if [[ ! -f "$CLANG_DIR/bin/clang-22" ]]; then
    echo "-----------------------------------------------"
    echo "Toolchain not found! Downloading..."
    echo "-----------------------------------------------"
    if [[ -d "$CLANG_DIR" ]]; then
        rm -rf "$CLANG_DIR"
    fi
    mkdir -p "$CLANG_DIR"

    curl -L -o "$CLANG_DIR/clang.tar.gz" "https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/mirror-goog-main-llvm-toolchain-source/clang-r596125.tar.gz" || {
        echo "Failed to download clang"
        exit 1
    }
    tar xf "$CLANG_DIR/clang.tar.gz" -C "$CLANG_DIR" || {
        echo "Failed to extract clang"
        exit 1
    }
    rm "$CLANG_DIR/clang.tar.gz"
fi

MAKE_CMD="make "
MAKE_CMD+="LLVM=1 "
MAKE_CMD+="LLVM_IAS=1 "
MAKE_CMD+="ARCH=arm64 "
MAKE_CMD+="O=out "
MAKE_CMD+="-j$(nproc --all)"

if $RECOVERY; then
    FRAGMENTS+="recovery.config "
    if ! $PERMISSIVE; then
        PERMISSIVE=true
    fi
    if $KSU; then
        KSU=false
    fi
fi

if $KSU; then
    FRAGMENTS+="ksu.config "
fi

if $PERMISSIVE; then
    FRAGMENTS+="debug.config "
fi

if [[ -d "$(pwd)/build/out/$MODEL" ]]; then
    rm -rf "build/out/$MODEL"
fi

mkdir -p "build/out/$MODEL/zip/files"
mkdir -p "build/out/$MODEL/zip/META-INF/com/google/android"

BUILD_KERNEL() {
    # Build kernel image
    echo "-----------------------------------------------"

    echo -n "KSU: "
    if $KSU; then
        echo "Yes"
    else
        echo "No"
    fi

    echo -n "Recovery: "
    if $RECOVERY; then
        echo "Yes"
    else
        echo "No"
    fi

    echo -n "Permissive: "
    if $PERMISSIVE; then
        echo "Yes"
    else
        echo "No"
    fi

    echo "-----------------------------------------------"
    echo "Building kernel using \"s5e9925_defconfig\""
    echo "Generating configuration file..."
    echo "-----------------------------------------------"
    $MAKE_CMD s5e9925_defconfig $FRAGMENTS || return 1

    echo "Building kernel..."
    echo "-----------------------------------------------"
    $MAKE_CMD || return 1
}

BUILD_BOOT() {
    KERNEL="build/out/$MODEL/Image"

    cp -a "out/arch/arm64/boot/Image" "$KERNEL" || return 1

    if $RECOVERY; then
        return 0
    fi

    echo "-----------------------------------------------"
    echo "Building boot.img RAMDisk..."
    mkdir -p "build/out/$MODEL/boot_ramdisk00"

    # Copy common files for boot.img's RAMDisk
    cp -a "build/ramdisk/boot/boot_ramdisk00" "build/out/$MODEL" || return 1

    # Copy device build.prop file for boot.img's RAMDisk
    cp -a "build/ramdisk/boot/device/$MODEL/"* "build/out/$MODEL/boot_ramdisk00/system/etc/ramdisk" || return 1

    (
    cd "build/out/$MODEL/boot_ramdisk00" || return 1
    find . ! -name . | LC_ALL=C sort | cpio -o -H newc -R root:root | lz4 -l > "../boot_ramdisk" || return 1
    ) || return 1

    echo "-----------------------------------------------"
    echo "Building boot.img..."

    OUTPUT_FILE="build/out/$MODEL/boot.img"
    RAMDISK_00="build/out/$MODEL/boot_ramdisk"
    OS_VERSION="16.0.0"
    OS_PATCH_LEVEL="2026-05"

	  python3 "toolchain/mkbootimg/mkbootimg.py" \
        --header_version 4 \
        --ramdisk "$RAMDISK_00"	\
        --os_version "$OS_VERSION" \
        --os_patch_level "$OS_PATCH_LEVEL" \
        --kernel "$KERNEL" \
        --output "$OUTPUT_FILE" || return 1
}

BUILD_DTB() {
    echo "-----------------------------------------------"
    echo "Building DTB image..."
    python3 "toolchain/mkdtboimg.py" cfg_create "build/out/$MODEL/dtb.img" "build/dtconfigs/s5e9925.cfg" -d "out/arch/arm64/boot/dts/exynos" || return 1

    echo "-----------------------------------------------"
    echo "Building DTBO image..."
    python3 "toolchain/mkdtboimg.py" cfg_create "build/out/$MODEL/dtbo.img" "build/dtconfigs/$MODEL.cfg" -d "out/arch/arm64/boot/dts/samsung/$MODEL" || return 1
    
}

BUILD_MODULES() {
    MODULES_DIR="modules"
    
    if [[ -d "out/$MODULES_DIR" ]]; then
        rm -rf "out/$MODULES_DIR"
    fi

    if [[ -d "build/out/$MODEL/modules" ]]; then
        rm -rf "build/out/$MODEL/modules"
    fi

    echo "-----------------------------------------------"
    echo "Building modules..."
    # Strip modules and place them in modules folder
    $MAKE_CMD INSTALL_MOD_PATH="$MODULES_DIR" INSTALL_MOD_STRIP=1 modules_install || return 1

    # Now we run depmod to update the dep/softdep files
    # For this we need the kernel version
    KERNEL_DIR_PATH=$(find "out/$MODULES_DIR/lib/modules" -maxdepth 1 -type d -name "5.10*")
    KERNEL_VERSION=$(basename "$KERNEL_DIR_PATH")

    if [[ -z "$KERNEL_DIR_PATH" ]] || [[ -z "$KERNEL_VERSION" ]]; then
        return 1
    fi

    # And finally depmod itself
    depmod -a -b "out/$MODULES_DIR" "$KERNEL_VERSION" || return 1

    # depmod updates modules.alias, modules.dep and modules.softdep
    # But the module order is not updated by depmod
    # We have to remove the filenames ourselves
    # But first, our vendor_boot needs modules.order definitions that go like:
    # fingerprint.ko
    # Clang generates modules.order definitions like this
    # kernel/drivers/fingerprint/fingerprint.ko
    # So we sed the file to adapt
    sed -i 's/.*\///g' "$KERNEL_DIR_PATH/modules.order" || return 1

    # Now we sed the bad filenames out of the file with a loop
    for i in $FILENAMES; do
        sed -i "/$i/d" "$KERNEL_DIR_PATH/modules.order" || return 1
    done

    # Now we have to order the modules
    # Samsung was nice enough to give us the order in vendor_boot_module_order_s5e9925.cfg
    # exynos-chipid_v2.ko exynos-reboot.ko sec_debug_base_early.ko clk_exynos.ko exynos_mct_v2.ko s3c2410_wdt.ko sec_debug_mode.ko
    # These files have to be at the top of modules.order in this order, and then we can keep the default order.
    # Samsung wants the file to be renamed to modules.load anyways, so we will craft our own modules.load file based on modules.order
    touch "$KERNEL_DIR_PATH/modules.load" || return 1

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
    for i in $INITIAL_ORDER; do
        echo "$i" >> "$KERNEL_DIR_PATH/modules.load" || return 1
        sed -i "/$i/d" "$KERNEL_DIR_PATH/modules.order" || return 1
    done

    # Now we add the remaining lines from modules.order into modules.load
    while IFS= read -r i; do
        echo "$i" >> "$KERNEL_DIR_PATH/modules.load" || return 1
    done < "$KERNEL_DIR_PATH/modules.order"

    # Now we have to also modify modules.dep
    # Android generates them like this
    # kernel/drivers/dma/samsung-dma.ko: kernel/drivers/dma/pl330.ko
    # But Samsung wants them like this
    # /lib/modules/samsung-dma.ko: /lib/modules/pl330.ko
    # So we will format it with sed
    sed -i 's/\(kernel\/[^: ]*\/\)\([^: ]*\.ko\)/\/lib\/modules\/\2/g' "$KERNEL_DIR_PATH/modules.dep" || return 1

    # Now the modules and their configuration descriptor files are ready, we move them to a folder and create the new second ramdisk
    # The second ramdisk should contain a /lib/modules where the modules are located
    mkdir -p "build/out/$MODEL/modules/lib/modules"

    find "$KERNEL_DIR_PATH" -name '*.ko' -exec cp '{}' "build/out/$MODEL/modules/lib/modules" ';'

    if [[ "$MODEL" == r0s ]] || [[ "$MODEL" == r11s ]]; then 
        sed -i '/wlan\.ko/d' "$KERNEL_DIR_PATH/modules.load" || return 1
        echo "wlan.ko" >> "$KERNEL_DIR_PATH/modules.load" || return 1
    fi

    # We also copy the module configuration descriptors
    for i in "alias" "dep" "load" "softdep"; do
        cp -a "$KERNEL_DIR_PATH/modules.$i" "build/out/$MODEL/modules/lib/modules" || return 1
    done
}

BUILD_VENDOR_BOOT() {
    echo "-----------------------------------------------"
    echo "Building vendor_boot RAMDisks..."
    # Copy common vendor_ramdisk00 files to build/out
    cp -a "build/ramdisk/vendor_boot/ramdisk00" "build/out/$MODEL/vendor_ramdisk00"
 
    # Copy device firmware files for vendor_ramdisk00
    cp -a "build/ramdisk/vendor_boot/vendor_firmware/$MODEL/"* "build/out/$MODEL/vendor_ramdisk00"

    # Pack RAMDisks
    # vendor_ramdisk == ramdisk00
    # modules_ramdisk == ramdisk01
    (
    cd "build/out/$MODEL/vendor_ramdisk00" || return 1
    find . ! -name . | LC_ALL=C sort | cpio -o -H newc -R root:root | lz4 -l > "../vendor_ramdisk" || return 1
    ) || return 1

    (
    cd "build/out/$MODEL/modules" || return 1
    find . ! -name . | LC_ALL=C sort | cpio -o -H newc -R root:root | lz4 -l > "../modules_ramdisk" || return 1
    )

    echo "-----------------------------------------------"
    echo "Building vendor_boot image..."

    OUTPUT_FILE="build/out/$MODEL/vendor_boot.img"
    DTB_PATH="build/out/$MODEL/dtb.img"
    BOOTCONFIG_PATH="build/ramdisk/bootconfig"
    RAMDISK_00="build/out/$MODEL/vendor_ramdisk"
    RAMDISK_01="build/out/$MODEL/modules_ramdisk"
    CMDLINE="bootconfig loop.max_part=7"

    python3 "toolchain/mkbootimg/mkbootimg.py" \
        --header_version "4" \
        --vendor_cmdline "$CMDLINE" \
        --dtb "$DTB_PATH" \
        --ramdisk_type 1 \
        --ramdisk_name "" \
        --vendor_ramdisk_fragment "$RAMDISK_00" \
        --vendor_bootconfig "$BOOTCONFIG_PATH" \
        --ramdisk_type "3" \
        --ramdisk_name "dlkm" \
        --vendor_ramdisk_fragment "$RAMDISK_01" \
        --vendor_boot "$OUTPUT_FILE" || return 1
}

BUILD_ZIP() {
    echo "-----------------------------------------------"
    echo "Building zip..."
    for i in "boot" "dtbo" "vendor_boot"; do
        cp "build/out/$MODEL/$i.img" "build/out/$MODEL/zip/files/$i.img" || return 1
    done

    cp "build/update-binary" "build/out/$MODEL/zip/META-INF/com/google/android/update-binary" || return 1
    cp "build/updater-script" "build/out/$MODEL/zip/META-INF/com/google/android/updater-script" || return 1

    VERSION=$(grep -o 'CONFIG_LOCALVERSION="[^"]*"' arch/arm64/configs/s5e9925_defconfig | cut -d '"' -f 2)
    VERSION=${VERSION:1}
    DATE="$(date +"%d-%m-%Y_%H-%M-%S")"

    if $KSU; then
        NAME="${VERSION}_${MODEL}_UNOFFICIAL_KSU_$DATE.zip"
    else
        NAME="${VERSION}_${MODEL}_UNOFFICIAL_$DATE.zip"
    fi

    if [[ -f "build/out/$MODEL/$NAME" ]]; then
        rm -f "build/out/$MODEL/$NAME"
    fi

    (
    cd "build/out/$MODEL/zip" || return 1
    zip -r -qq "../$NAME" .
    ) || return 1
}

BUILD_KERNEL || ABORT
BUILD_BOOT || ABORT
BUILD_DTB || ABORT
BUILD_MODULES || ABORT

if ! $RECOVERY; then				   
    BUILD_VENDOR_BOOT || ABORT
    BUILD_ZIP || ABORT
fi 

popd > /dev/null || exit 1
echo "-----------------------------------------------"
echo "Build finished successfully!"
