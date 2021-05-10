/**
*=============================================================================
* \file acdb_init.c
*
* \brief
*		Handles the initialization and de-initialization of ACDB SW.
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

#include "acdb_init.h"
#include "acdb_init_utility.h"
#include "acdb_parser.h"
#include "acdb_delta_parser.h"
#include "acdb_common.h"
#include <string.h>
/* ---------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Type Declarations
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Global Data Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Static Variable Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Static Function Declarations and Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Externalized Function Definitions
*--------------------------------------------------------------------------- */
//const char* ACDB_AMDB_AVS_FILE_NAME ="avs_config.acdb";// MDF change, AMDB file can of any processor id, so compare only trailing string
const char* WKSP_FILE_NAME_EXT = ".qwsp";
const char* ACDB_FILE_NAME_EXT = ".acdb";

int32_t AcdbInit(AcdbCmdFileInfo *acdb_cmd_finfo, AcdbFileInfo *acdb_finfo)
{
	int32_t result = ACDB_INIT_SUCCESS;
	ar_fhandle *fhandle = &acdb_cmd_finfo->file_handle;

	if (ACDB_FIND_STR(acdb_cmd_finfo->chFileName, WKSP_FILE_NAME_EXT) != NULL)
	{
        /* The workspace file is only opened when ARCT(or other ACDB client)
         * connects and querries for file information
         */
        acdb_cmd_finfo->file_type = ACDB_FILE_TYPE_WORKSPACE;
        acdb_finfo->file_type = ACDB_FILE_TYPE_WORKSPACE;
	}
	else if (ACDB_FIND_STR(acdb_cmd_finfo->chFileName, ACDB_FILE_NAME_EXT) != NULL)
	{
        acdb_cmd_finfo->file_type = ACDB_FILE_TYPE_DATABASE;

        result = AcdbInitUtilGetFileData(
            acdb_cmd_finfo->chFileName,
            fhandle,
            &acdb_cmd_finfo->file_size,
            &acdb_cmd_finfo->file_buffer
        );

        if (result != ACDB_PARSE_SUCCESS || fhandle == NULL)
            return ACDB_INIT_FAILURE;

		if (ACDB_PARSE_SUCCESS != IsAcdbFileValid(*fhandle,
            acdb_cmd_finfo->file_size))
			return ACDB_INIT_FAILURE;

		acdb_finfo->file_type = ACDB_FILE_TYPE_DATABASE;
		acdb_finfo->major = 1;
		acdb_finfo->minor = 0;
		acdb_finfo->revision = 0;
		acdb_finfo->cplInfo = 0;

        AcdbLogFileVersion(
            acdb_finfo->major,
            acdb_finfo->minor,
            acdb_finfo->revision,
            acdb_finfo->cplInfo);
	}
    else
    {
        //Provided file is not an acdb or workspace file
        ACDB_ERR("%s is not a *.qwsp or *.acdb file.",
            acdb_cmd_finfo->chFileName);
        return ACDB_INIT_FAILURE;
    }

	fhandle = NULL;
	return result;
}

int32_t AcdbDeltaInit(AcdbCmdFileInfo *acdb_cmd_finfo, AcdbCmdDeltaFileInfo *acdb_cmd_delta_finfo, AcdbFile *delta_fpath)
{
	int32_t result = ACDB_INIT_SUCCESS;
	uint32_t deltaFileSize = 0;
	int32_t deltaFileExists = ACDB_UTILITY_INIT_FAILURE;
	ar_fhandle *fhandle = &acdb_cmd_delta_finfo->file_handle;

	acdb_cmd_delta_finfo->delta_file_path.fileNameLen = delta_fpath->fileNameLen;

	result = ACDB_STR_CPY_SAFE(
		acdb_cmd_delta_finfo->delta_file_path.fileName,
        MAX_FILENAME_LENGTH,
		delta_fpath->fileName,
		delta_fpath->fileNameLen);

	if (ACDB_UTILITY_INIT_SUCCESS != result)
	{
		fhandle = NULL;
		ACDB_ERR_MSG_1("Failed to copy delta file path string", result);
		return result;
	}

	deltaFileExists = AcdbInitUtilOpenDeltaFile(
		acdb_cmd_finfo->chFileName, acdb_cmd_finfo->filename_len,
		delta_fpath, &deltaFileSize);

	if (ACDB_UTILITY_INIT_SUCCESS == deltaFileExists)
	{
		result = AcdbInitUtilGetDeltaFileData(
			acdb_cmd_finfo->chFileName,
			acdb_cmd_finfo->filename_len,
			fhandle, delta_fpath);

		if (result != ACDB_PARSE_SUCCESS || *fhandle == NULL)
		{
			return result;
		}

		//If a new delta file is created bypass the validation
		if (0 != deltaFileSize)
		{
			if (ACDB_PARSE_SUCCESS != IsAcdbDeltaFileValid(*fhandle, deltaFileSize))
			{
				return ACDB_INIT_FAILURE;
			}

			result = AcdbDeltaParserGetSwVersion(*fhandle, deltaFileSize,
				&acdb_cmd_delta_finfo->file_info);

			if (AR_EOK != result)
			{
				return ACDB_INIT_FAILURE;
			}

            AcdbLogDeltaFileVersion(
                acdb_cmd_delta_finfo->file_info.major,
                acdb_cmd_delta_finfo->file_info.minor,
                acdb_cmd_delta_finfo->file_info.revision,
                acdb_cmd_delta_finfo->file_info.cplInfo
            );
		}

		acdb_cmd_delta_finfo->exists = TRUE;
		acdb_cmd_delta_finfo->file_size = deltaFileSize;
	}
	else
	{
		fhandle = NULL;
		acdb_cmd_delta_finfo->exists = FALSE;
	}

	return result;
}

bool_t AcdbInitDoSwVersionsMatch(AcdbFileInfo *acdb_finfo, AcdbDeltaFileInfo *acdb_delta_finfo)
{
	if ((acdb_finfo->major == acdb_delta_finfo->major) &&
		(acdb_finfo->minor == acdb_delta_finfo->minor) &&
		(acdb_finfo->revision == acdb_delta_finfo->revision) &&
		(acdb_finfo->cplInfo  == INF || acdb_finfo->cplInfo == acdb_delta_finfo->cplInfo))
	{
		return TRUE;
	}
	return FALSE;
}
