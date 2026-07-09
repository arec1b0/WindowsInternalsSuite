# Minimal CPack configuration producing a portable ZIP artifact.

set(CPACK_PACKAGE_NAME               "WindowsInternalsSuite")
set(CPACK_PACKAGE_VENDOR             "peoNova")
set(CPACK_PACKAGE_VERSION            "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_GENERATOR                  "ZIP")
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-win64")

include(CPack)