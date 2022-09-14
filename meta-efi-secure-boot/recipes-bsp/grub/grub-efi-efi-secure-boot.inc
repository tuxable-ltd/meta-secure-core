DEPENDS += "openssl-native"

FILESEXTRAPATHS:prepend := "${THISDIR}/grub-efi:"

GRUB_SIGN_VERIFY_STRICT ?= "1"

GRUB_MOKVERIFY_PATCH = " \
   file://mok2verify-support-to-verify-non-PE-file-with-PKCS-7.patch \
   file://mok2verify-multiboot.patch \
   file://verify-all-buffiles.patch \
"

SRC_URI:append:class-target = " \
    file://0001-pe32.h-add-header-structures-for-TE-and-DOS-executab.patch \
    file://0002-shim-add-needed-data-structures.patch \
    file://0003-efi-chainloader-implement-an-UEFI-Exit-service-for-s.patch \
    file://0004-efi-chainloader-port-shim-to-grub.patch \
    file://0005-efi-chainloader-use-shim-to-load-and-verify-an-image.patch \
    file://0006-efi-chainloader-boot-the-image-using-shim.patch \
    file://0007-efi-chainloader-take-care-of-unload-undershim.patch \
    file://chainloader-handle-the-unauthenticated-image-by-shim.patch \
    file://chainloader-Don-t-check-empty-section-in-file-like-..patch \
    file://chainloader-Actually-find-the-relocations-correctly-.patch \
    file://efi-chainloader-implemented-for-32-bit.patch \
    file://Grub-get-and-set-efi-variables.patch \
    file://uefi_verify.patch \
    file://0001-grub-verify-Add-strict_security-variable.patch \
    file://0001-kern-efi-init.c-disable-inside-lockdown-and-shim_loc.patch \
    ${@d.getVar('GRUB_MOKVERIFY_PATCH', True) if d.getVar('UEFI_SELOADER', True) == '1' else ''} \
"

# functions efi_call_foo and efi_shim_exit are not implemented for arm64 yet
COMPATIBLE_HOST:aarch64 = 'null'

GRUB_SECURE_BOOT_MODULES += "${@'efivar password_pbkdf2 ' if d.getVar('UEFI_SB', True) == '1' else ''}"

GRUB_SIGNING_MODULES += "${@'pgp gcry_rsa gcry_sha256 gcry_sha512 --pubkey %s ' % d.getVar('GRUB_PUB_KEY', True) \
  if d.getVar('GRUB_SIGN_VERIFY', True) == '1' else ''}"

GRUB_SELOADER_MODULES += "${@'mok2verify ' if d.getVar('UEFI_SELOADER', True) == '1' else ''}"

GRUB_TPM_MODULES += "${@bb.utils.contains('DISTRO_FEATURES', 'tpm' , 'tpm', '', d)}"
GRUB_TPM_MODULES += "${@bb.utils.contains('DISTRO_FEATURES', 'tpm2', 'tpm', '', d)}"

GRUB_SECURE_BUILDIN ??= ""
GRUB_SECURE_BUILDIN:append:class-target = " \
  tftp reboot chain \
  ${GRUB_SECURE_BOOT_MODULES} \
  ${GRUB_SIGNING_MODULES} \
  ${GRUB_SELOADER_MODULES} \
  ${GRUB_TPM_MODULES}"

# For efi_call_foo and efi_shim_exit
CFLAGS:append:class-target = " -fno-toplevel-reorder"

# Set a default root specifier.
inherit user-key-store

python __anonymous () {
    if d.getVar('UEFI_SB', True) != "1":
        return

    # Override the default filename if efi-secure-boot enabled.
    # grub-efi must be renamed as grub${arch}.efi for working with shim
    # or SELoader.
    import re

    target = d.getVar('TARGET_ARCH', True)
    if target == "x86_64":
        grubimage = "grubx64.efi"
    elif re.match('i.86', target):
        grubimage = "grubia32.efi"
    else:
        raise bb.parse.SkipPackage("grub-efi is incompatible with target %s" % target)

    d.setVar("GRUB_IMAGE", grubimage)
}

do_compile:append:class-target() {
	if [ "${GRUB_SIGN_VERIFY}" = "1" -a "${GRUB_SIGN_VERIFY_STRICT}" = "1" ] ; then
		cat<<EOF>${WORKDIR}/cfg
set strict_security=1
EOF
  else
    > ${WORKDIR}/cfg
	fi
	cat<<EOF>>${WORKDIR}/cfg
search.file ${EFIDIR}/grub.cfg root
set prefix=(\$root)${EFIDIR}
EOF
}

do_compile:append:class-native() {
    make grub-editenv
}

do_install:append:class-native() {
    install -m 0755 grub-editenv "${D}${bindir}"
}

do_install:append:class-target() {

    install -d "${D}${EFI_FILES_PATH}/${GRUB_TARGET}-efi"
    grub-mkimage -c ../cfg -p "${EFIDIR}" -d "./grub-core" \
        -O "${GRUB_TARGET}-efi" -o "${B}/${GRUB_IMAGE}" \
        ${GRUB_BUILDIN} ${GRUB_SECURE_BUILDIN}

    install -m 0644 "${B}/${GRUB_IMAGE}" "${D}${EFI_FILES_PATH}/${GRUB_IMAGE}"

    # Install the modules to grub-efi's search path
    make -C grub-core install DESTDIR="${D}${EFI_FILES_PATH}" pkglibdir=""

    # Remove .module
    rm -f ${D}${EFI_FILES_PATH}/${GRUB_TARGET}-efi/*.module
}

python do_sign:prepend:class-target() {
    bb.build.exec_func("check_deploy_keys", d)
    if d.getVar('GRUB_SIGN_VERIFY') == '1':
        bb.build.exec_func("check_boot_public_key", d)
}

fakeroot python do_sign:class-target() {
    image_dir = d.getVar('D', True)
    efi_boot_path = d.getVar('EFI_FILES_PATH', True)
    grub_image = d.getVar('GRUB_IMAGE', True)
    dir = image_dir + efi_boot_path + '/'

    sb_sign(dir + grub_image, dir + grub_image, d)
}

python do_sign() {
}
addtask sign after do_install before do_deploy do_package

do_deploy:append:class-target() {
    install -m 0644 "${D}${EFI_FILES_PATH}/${GRUB_IMAGE}" "${DEPLOYDIR}"

    install -d "${DEPLOYDIR}/efi-unsigned"
    install -m 0644 "${B}/${GRUB_IMAGE}" "${DEPLOYDIR}/efi-unsigned"
    PSEUDO_DISABLED=1 cp -af "${D}${EFI_FILES_PATH}/${GRUB_TARGET}-efi" "${DEPLOYDIR}/efi-unsigned"
}

FILES:${PN} += "${EFI_FILES_PATH}"
