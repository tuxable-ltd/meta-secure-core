require conf/image-uefi.conf

inherit user-key-store

PACKAGECONFIG:append = " secureboot"

# For SELoader
do_compile:class-target:append() {
    if ${@bb.utils.contains('PACKAGECONFIG', 'secureboot', 'true', 'false', d)}; then
	secbuild_dir="${S}/Build/SecurityPkg/RELEASE_${FIXED_GCCVER}"
        ${S}/OvmfPkg/build.sh $PARALLEL_JOBS -a $OVMF_ARCH -b RELEASE -t ${FIXED_GCCVER} ${OVMF_SECURE_BOOT_FLAGS} -p SecurityPkg/SecurityPkg.dsc
	ln ${secbuild_dir}/${OVMF_ARCH}/Hash2DxeCrypto.efi ${WORKDIR}/ovmf/
	ln ${secbuild_dir}/${OVMF_ARCH}/Pkcs7VerifyDxe.efi ${WORKDIR}/ovmf/
    fi
}

do_install:class-target:append() {
    if ${@bb.utils.contains('PACKAGECONFIG', 'secureboot', 'true', 'false', d)}; then
        mkdir -p ${D}${EFI_FILES_PATH}
        if [ x"${UEFI_SB}" = x"1" ]; then
            install ${WORKDIR}/ovmf/Hash2DxeCrypto.efi.signed ${D}${EFI_FILES_PATH}/Hash2DxeCrypto.efi
            install ${WORKDIR}/ovmf/Pkcs7VerifyDxe.efi.signed ${D}${EFI_FILES_PATH}/Pkcs7VerifyDxe.efi
        else
            install ${WORKDIR}/ovmf/Hash2DxeCrypto.efi ${D}${EFI_FILES_PATH}/Hash2DxeCrypto.efi
            install ${WORKDIR}/ovmf/Pkcs7VerifyDxe.efi ${D}${EFI_FILES_PATH}/Pkcs7VerifyDxe.efi
        fi
    fi
}

python do_sign() {
}

python do_sign:class-target() {
    sb_sign(d.expand('${WORKDIR}/ovmf/Hash2DxeCrypto.efi'), d.expand('${WORKDIR}/ovmf/Hash2DxeCrypto.efi.signed'), d)
    sb_sign(d.expand('${WORKDIR}/ovmf/Pkcs7VerifyDxe.efi'), d.expand('${WORKDIR}/ovmf/Pkcs7VerifyDxe.efi.signed'), d)
}
addtask sign after do_compile before do_install do_deploy

do_deploy:class-target:append() {
    if [ x"${UEFI_SB}" = x"1" ]; then
        install -d ${DEPLOYDIR}/efi-unsigned
        install ${WORKDIR}/ovmf/Pkcs7VerifyDxe.efi "${DEPLOYDIR}/efi-unsigned/Pkcs7VerifyDxe.efi"
        install ${WORKDIR}/ovmf/Hash2DxeCrypto.efi "${DEPLOYDIR}/efi-unsigned/Hash2DxeCrypto.efi"
        install ${WORKDIR}/ovmf/Pkcs7VerifyDxe.efi.signed "${DEPLOYDIR}/Pkcs7VerifyDxe.efi"
        install ${WORKDIR}/ovmf/Hash2DxeCrypto.efi.signed "${DEPLOYDIR}/Hash2DxeCrypto.efi"
    else
        install ${WORKDIR}/ovmf/Pkcs7VerifyDxe.efi "${DEPLOYDIR}/Pkcs7VerifyDxe.efi"
        install ${WORKDIR}/ovmf/Hash2DxeCrypto.efi "${DEPLOYDIR}/Hash2DxeCrypto.efi"
    fi
}

PACKAGES += " \
   ovmf-pkcs7-efi \
"

FILES:ovmf-pkcs7-efi += " \
    ${EFI_FILES_PATH}/Hash2DxeCrypto.efi \
    ${EFI_FILES_PATH}/Pkcs7VerifyDxe.efi \
"
