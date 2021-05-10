#ifndef __ACDB_INIT_UTILITY_H__
#define __ACDB_INIT_UTILITY_H__
/**
*=============================================================================
* \file acdb_init_utility.h
*
* \brief
*		Contains utility fucntions for ACDB SW initialiation. This inclues
*		initializaing the ACDB Data and ACDB Delta Data files.
*
* \copyright
*  Copyright (c) 2018-2020 Qualcomm Technologies, Inc.
*  All Rights Reserved.
*  Confidential and Proprietary - Qualcomm Technologies, Inc.
*
*=============================================================================
*/

/* ---------------------------------------------------------------------------
 * Include Files
 *--------------------------------------------------------------------------- */

#include "acdb.h"
#include "acdb_utility.h"

/* ---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Type Declarations
 *--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Class Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Function Declarations and Documentation
 *--------------------------------------------------------------------------- */

#define ACDB_UTILITY_INIT_SUCCESS 0
#define ACDB_UTILITY_INIT_FAILURE -1

void AcdbLogSwVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl);

void AcdbLogDeltaSwVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl);

void AcdbLogFileVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl);

void AcdbLogDeltaFileVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl);

int32_t AcdbInitUtilGetFileDataOLD(const char *pFilename, ar_fhandle *fhandle,uint32_t *pFileSize);

int32_t AcdbInitUtilGetFileData(const char *fname, ar_fhandle *fhandle, uint32_t *fsize, void **fbuffer);

/* ---------------------------------------------------------------------------
 * AcdbIsPersistenceSupported() Documentation
 *--------------------------------------------------------------------------- */

/** @addtogroup AcdbIsPersistenceSupported
@{ */

/**
   Function signature to know if ACDB persistence supported.

   @return
   - ACDB_UTILITY_INIT_SUCCESS -- ACDB persistence is supported.
   - ACDB_UTILITY_INIT_FAILURE -- ACDB persistence is not supported.

   @sa
   acdb_init_utility \n
*/

int32_t AcdbIsPersistenceSupported(void);

/* ---------------------------------------------------------------------------
 * AcdbInitUtilOpenDeltaFile() Documentation
 *--------------------------------------------------------------------------- */

/** @addtogroup AcdbInitUtilOpenDeltaFile
@{ */

/**
   Function signature to know if delta acdb file is present for existing acdb file.
   If present, return with the length of corresponding delta ACDB file data length.

   @param[in] pFilename
         ACDB file name.

   @param[in] fileNameLen
         ACDB file name length.

	@param[out] pDeltaFileDataLen
         Delta ACDB file length (if present)

   @return
   - ACDB_UTILITY_INIT_SUCCESS -- Delta ACDB file is present.
   - ACDB_UTILITY_INIT_FAILURE -- Delta ACDB file is not present.

   @sa
   acdb_init_utility \n
*/

int32_t AcdbInitUtilOpenDeltaFile (const char* pFilename, uint32_t fileNameLen, AcdbFile* deltaFilePath, uint32_t* pDeltaFileDataLen);

/* ---------------------------------------------------------------------------
 * AcdbInitUtilGetDeltaFileData() Documentation
 *--------------------------------------------------------------------------- */

/** @addtogroup AcdbInitUtilGetDeltaFileData
@{ */

/**
   Function signature to know get delta acdb file data for the acdb file.

   @param[in] pFilename
         ACDB file name.

   @param[in] fileNameLen
         ACDB file name length.

   @param[out] fhandle
         Pointer to Delta file content.

   @param[in] nFileBfrSize
         Delta file content length.

   @return
   - ACDB_UTILITY_INIT_SUCCESS -- Operation successful.
   - ACDB_UTILITY_INIT_FAILURE -- Operation failure.

   @sa
   acdb_init_utility \n
*/

int32_t AcdbInitUtilGetDeltaFileData (const char* pFilename, uint32_t fileNameLen, ar_fhandle *fhandle, AcdbFile* deltaFilePath);

/* ---------------------------------------------------------------------------
 * AcdbInitUtilDeleteDeltaFileData() Documentation
 *--------------------------------------------------------------------------- */

/** @addtogroup AcdbInitUtilDeleteDeltaFileData
@{ */

/**
   Function signature to delete delta acdb file for existing acdb file.

   @param[in] pFilename
         ACDB file name.

   @param[in] fileNameLen
         ACDB file name length.

   @return
   - ACDB_UTILITY_INIT_SUCCESS -- Operation successful.
   - ACDB_UTILITY_INIT_FAILURE -- Operation failure.

   @sa
   acdb_init_utility \n
*/

//int32_t AcdbInitUtilDeleteDeltaFileData (const char* pFilename, uint32_t fileNameLen, AcdbFile deltaFilePath);
int32_t AcdbInitUtilDeleteDeltaFileData(const char* pFilename, uint32_t fileNameLen, AcdbFile* deltaFilePath);

#endif /* __ACDB_INIT_UTILITY_H__ */
