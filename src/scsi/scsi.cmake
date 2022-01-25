set(PCEM_PRIVATE_API ${PCEM_PRIVATE_API}
        ${CMAKE_SOURCE_DIR}/includes/private/scsi/scsi_53c400.h
        ${CMAKE_SOURCE_DIR}/includes/private/scsi/scsi_aha1540.h
        ${CMAKE_SOURCE_DIR}/includes/private/scsi/scsi_cd.h
        ${CMAKE_SOURCE_DIR}/includes/private/scsi/scsi.h
        ${CMAKE_SOURCE_DIR}/includes/private/scsi/scsi_hd.h
        ${CMAKE_SOURCE_DIR}/includes/private/scsi/scsi_ibm.h
        ${CMAKE_SOURCE_DIR}/includes/private/scsi/scsi_zip.h
        )

set(PCEM_SRC ${PCEM_SRC}
        scsi/scsi.c
        scsi/scsi_53c400.c
        scsi/scsi_aha1540.c
        scsi/scsi_cd.c
        scsi/scsi_hd.c
        scsi/scsi_ibm.c
        scsi/scsi_zip.c
        )