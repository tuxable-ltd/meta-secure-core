LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/COPYING.MIT;md5=3da9cfbcb788c80a0384361b4de20420"
SUMMARY = "SecureBoot grub.cfg for use in EFI systems"
HOMEPAGE = "https://www.gnu.org/software/grub/manual/grub/grub.html#Configuration"

DEPENDS = "grub-efi-native"
RPROVIDES:${PN} += "virtual-grub-bootconf"

require conf/image-uefi.conf
inherit user-key-store

FILESEXTRAPATHS:prepend := "${THISDIR}/grub-efi:"

GRUB_SIGN_VERIFY_STRICT ?= "1"

SRC_URI = " \
    file://grub-efi.cfg \
    file://boot-menu.inc \
    ${@'file://efi-secure-boot.inc file://password.inc' if d.getVar('UEFI_SB', True) == '1' else ''} \
"

S = "${WORKDIR}"

do_install:append() {
    local menu="${WORKDIR}/boot-menu.inc"

    # Enable the default IMA rules if IMA is enabled and luks is disabled.
    # This is because unseal operation will fail when any PCR is extended
    # due to updating the aggregate integrity value by the default IMA rules.
    [ x"${IMA}" = x"1" -a x"${@bb.utils.contains('DISTRO_FEATURES', 'luks', '1', '0', d)}" != x"1" ] && {
        ! grep -q "ima_policy=tcb" "$menu" &&
            sed -i 's/^\s*linux\s\+.*bzImage.*/& ima_policy=tcb/g' "$menu"
    }

    # Replace the root parameter in boot command line with BOOT_CMD_ROOT,
    # which can be configured. It is helpful when secure boot is enabled.
    [ -n "${BOOT_CMD_ROOT}" ] && {
        sed -i "s,root=/dev/hda2,root=${BOOT_CMD_ROOT},g" "$menu"
    }

    # Install the stacked grub configs.
    install -d "${D}${EFI_FILES_PATH}"
    install -m 0600 "${WORKDIR}/grub-efi.cfg" "${D}${EFI_FILES_PATH}/grub.cfg"
    install -m 0600 "$menu" "${D}${EFI_FILES_PATH}"
    [ x"${UEFI_SB}" = x"1" ] && {
        install -m 0600 "${WORKDIR}/efi-secure-boot.inc" "${D}${EFI_FILES_PATH}"
        install -m 0600 "${WORKDIR}/password.inc" "${D}${EFI_FILES_PATH}"
    }

    # Create the initial environment block with empty item.
    grub-editenv "${D}${EFI_FILES_PATH}/grubenv" create
}

python do_sign:prepend() {
    bb.build.exec_func("check_deploy_keys", d)
    if d.getVar('GRUB_SIGN_VERIFY') == '1':
        bb.build.exec_func("check_boot_public_key", d)
}   

fakeroot python do_sign() {
    image_dir = d.getVar('D', True)
    efi_boot_path = d.getVar('EFI_FILES_PATH', True)
    dir = image_dir + efi_boot_path + '/'

    uks_bl_sign(dir + 'grub.cfg', d)
    uks_bl_sign(dir + 'boot-menu.inc', d)

    if d.getVar('UEFI_SB', True) == "1":
        uks_bl_sign(dir + 'efi-secure-boot.inc', d)
        uks_bl_sign(dir + 'password.inc', d)
}
addtask sign after do_install before do_deploy

fakeroot do_chownboot() {
    chown root:root -R "${D}${EFI_FILES_PATH}/grub.cfg${SB_FILE_EXT}"
    chown root:root -R "${D}${EFI_FILES_PATH}/boot-menu.inc${SB_FILE_EXT}"
    [ x"${UEFI_SB}" = x"1" ] && {
        chown root:root -R "${D}${EFI_FILES_PATH}/efi-secure-boot.inc${SB_FILE_EXT}"
        chown root:root -R "${D}${EFI_FILES_PATH}/password.inc${SB_FILE_EXT}"
    }
}
addtask chownboot after do_deploy before do_package

do_deploy:append() {
    install -m 0644 "${D}${EFI_FILES_PATH}/${GRUB_IMAGE}" "${DEPLOYDIR}"

    # Deploy the stacked grub configs.
    install -m 0600 "${D}${EFI_FILES_PATH}/grubenv" "${DEPLOYDIR}"
    install -m 0600 "${D}${EFI_FILES_PATH}/grub.cfg" "${DEPLOYDIR}"
    install -m 0600 "${D}${EFI_FILES_PATH}/boot-menu.inc" "${DEPLOYDIR}"
    install -m 0600 "${D}${EFI_FILES_PATH}/grub.cfg${SB_FILE_EXT}" "${DEPLOYDIR}"
    install -m 0600 "${D}${EFI_FILES_PATH}/boot-menu.inc${SB_FILE_EXT}" "${DEPLOYDIR}"
    [ x"${UEFI_SB}" = x"1" ] && {
        install -m 0600 "${D}${EFI_FILES_PATH}/efi-secure-boot.inc" "${DEPLOYDIR}"
        install -m 0600 "${D}${EFI_FILES_PATH}/password.inc" "${DEPLOYDIR}"
        install -m 0600 "${D}${EFI_FILES_PATH}/efi-secure-boot.inc${SB_FILE_EXT}" "${DEPLOYDIR}"
        install -m 0600 "${D}${EFI_FILES_PATH}/password.inc${SB_FILE_EXT}" "${DEPLOYDIR}"
    }

    install -d "${DEPLOYDIR}/efi-unsigned"
    install -m 0644 "${B}/${GRUB_IMAGE}" "${DEPLOYDIR}/efi-unsigned"
    PSEUDO_DISABLED=1 cp -af "${D}${EFI_FILES_PATH}/${GRUB_TARGET}-efi" "${DEPLOYDIR}/efi-unsigned"
}

FILES:${PN} += "${EFI_FILES_PATH}"

CONFFILES:${PN} += "\
    ${EFI_FILES_PATH}/grub.cfg \
    ${EFI_FILES_PATH}/grubenv \
    ${EFI_FILES_PATH}/boot-menu.inc \
    ${EFI_FILES_PATH}/efi-secure-boot.inc \
"
