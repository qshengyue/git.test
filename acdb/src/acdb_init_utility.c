/**
*=============================================================================
* \file acdb_init_utility.c
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

#include "acdb_init_utility.h"
#include "acdb_common.h"
#include "ar_osal_error.h"
#include "ar_osal_file_io.h"

extern int32_t g_persistenceStatus;

void AcdbLogSwVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl)
{
	ACDB_PKT_LOG_DATA("ACDB_SW_MAJOR", &(major), sizeof(major));
	ACDB_PKT_LOG_DATA("ACDB_SW_MINOR", &(minor), sizeof(minor));
	ACDB_PKT_LOG_DATA("ACDB_SW_REVISION", &(revision), sizeof(revision));

	if (cpl != INF)
	{
		ACDB_PKT_LOG_DATA("ACDB_SW_CPL", &(cpl), sizeof(cpl));
        ACDB_VERBOSE("ACDB SW Version %u.%u.%u.%u", major, minor, revision, cpl);
	}
	else
	{
        ACDB_VERBOSE("ACDB SW Version %u.%u.%u", major, minor, revision);
	}
}

void AcdbLogDeltaSwVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl)
{
	ACDB_PKT_LOG_DATA("ACDB_DELTA_SW_MAJOR", &(major), sizeof(major));
	ACDB_PKT_LOG_DATA("ACDB_DELTA_SW_MINOR", &(minor), sizeof(minor));
	ACDB_PKT_LOG_DATA("ACDB_DELTA_SW_REVISION", &(revision), sizeof(revision));

	if (cpl != INF)
	{
		ACDB_PKT_LOG_DATA("ACDB_DELTA_SW_CPL", &(cpl), sizeof(cpl));
        ACDB_VERBOSE("ACDB Delta SW Version %u.%u.%u.%u", major, minor, revision, cpl);
	}
	else
	{
        ACDB_VERBOSE("ACDB Delta SW Version %u.%u.%u", major, minor, revision);
	}
}

void AcdbLogFileVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl)
{
	ACDB_PKT_LOG_DATA("ACDB_FILE_MAJOR", &(major), sizeof(major));
	ACDB_PKT_LOG_DATA("ACDB_FILE_MINOR", &(minor), sizeof(minor));
	ACDB_PKT_LOG_DATA("ACDB_FILE_REVISION", &(revision), sizeof(revision));

	if (cpl != INF)
	{
		ACDB_PKT_LOG_DATA("ACDB_FILE_CPL", &(cpl), sizeof(cpl));
        ACDB_VERBOSE("ACDB File Version %u.%u.%u.%u", major, minor, revision, cpl);
	}
	else
	{
        ACDB_VERBOSE("ACDB File Version %u.%u.%u", major, minor, revision);
	}
}

void AcdbLogDeltaFileVersion(uint32_t major, uint32_t minor, uint32_t revision, uint32_t cpl)
{
	ACDB_PKT_LOG_DATA("ACDB_DELTA_FILE_MAJOR", &(major), sizeof(major));
	ACDB_PKT_LOG_DATA("ACDB_DELTA_FILE_MINOR", &(minor), sizeof(minor));
	ACDB_PKT_LOG_DATA("ACDB_DELTA_FILE_REVISION", &(revision), sizeof(revision));

	if (cpl != INF)
	{
		ACDB_PKT_LOG_DATA("ACDB_DELTA_FILE_CPL", &(cpl), sizeof(cpl));
        ACDB_VERBOSE("ACDB Delta File Version %u.%u.%u.%u", major, minor, revision, cpl);
	}
	else
	{
        ACDB_VERBOSE("ACDB Delta File Version %u.%u.%u", major, minor, revision);
	}
}

int32_t AcdbInitUtilGetFileDataOLD(const char* pFilename, ar_fhandle* fhandle, uint32_t *pFileSize)
{
	int32_t result = ACDB_UTILITY_INIT_SUCCESS;

	if (pFilename == NULL)
	{
		ACDB_ERR("AcdbInitUtilGetFileData() failed, filename is null");
		return ACDB_UTILITY_INIT_FAILURE;
	}
	else
	{

		int err = ar_fopen(fhandle, pFilename, AR_FOPEN_READ_ONLY);
		if (err != 0 || fhandle == NULL)
		{
			ACDB_ERR("AcdbInitUtilGetFileData() failed, fopen failed with error = %d", err);
			return ACDB_UTILITY_INIT_FAILURE;
		}

		*pFileSize = (uint32_t)ar_fsize(*fhandle);

		int ret = ar_fseek(*fhandle, 0, AR_FSEEK_BEGIN);
		if (ret != 0)
		{
			ACDB_ERR("AcdbInitUtilGetFileData() failed, file seek to %d was unsuccessful", 0);
			return AR_EFAILED;
		}

	}
	return result;
}

int32_t AcdbInitUtilGetFileData(const char* fname, ar_fhandle* fhandle, uint32_t *fsize, void** fbuffer)
{
    int32_t status = AR_EOK;
    size_t bytes_read = 0;

    if (fname == NULL)
    {
        ACDB_ERR("Error[%d]: The file name provided is null", AR_EBADPARAM);
        return AR_EBADPARAM;
    }
    else
    {

        status = ar_fopen(fhandle, fname, AR_FOPEN_READ_ONLY);
        if (AR_FAILED(status) || fhandle == NULL)
        {
            ACDB_ERR("Error[%d]: Unable to open file: %s", status, fname);
            return status;
        }

        status = ar_fseek(*fhandle, 0, AR_FSEEK_BEGIN);
        if (AR_FAILED(status))
        {
            ACDB_ERR("Error[%d]: Unable to fseek file: %s", status, fname);
            return AR_EFAILED;
        }

        *fsize = (uint32_t)ar_fsize(*fhandle);
        if (*fsize != 0)
        {
            *fbuffer = (void*)ACDB_MALLOC(uint8_t, *fsize);
        }

        status = ar_fread(*fhandle, *fbuffer, *fsize, &bytes_read);
        if (AR_FAILED(status))
        {
            ACDB_FREE(*fbuffer);
            ACDB_ERR("Error[%d]: Unable to read file: %s", status, fname);
            return status;
        }

        if (bytes_read != *fsize)
        {
            status = AR_EBADPARAM;
            ACDB_FREE(*fbuffer);
            ACDB_ERR("Error[%d]: File size does not match "
                "the number of bytes read: %s", status, fname);
        }
    }

    return status;
}

int32_t AcdbIsPersistenceSupported(void)
{
	return g_persistenceStatus;
}

char *GetDeltaFileName(const char* fname, size_t fname_len, AcdbFile* delta_fpath)
{
	char* delta_fname = NULL;
    char* delta_fname_ext = "delta";
    size_t ext_size = 6;
    size_t slash_size = 0;
	size_t delta_fname_size = 0;
	size_t remaining_size = 0;

	if (fname == NULL || fname_len == 0)
	{
		return NULL;
	}

	if (delta_fpath == NULL)
	{
		//Delta file path is null, so use acdb file path and concatenate "delta"
		delta_fname_size = fname_len + ar_strlen(delta_fname_ext, ext_size) + 1;
		delta_fname = ACDB_MALLOC(char, delta_fname_size);

		if (IsNull(delta_fname)) return NULL;

		if (delta_fname != NULL)
		{
			ACDB_STR_CPY_SAFE(delta_fname, delta_fname_size, fname, fname_len);
			ACDB_STR_CAT_SAFE(delta_fname, delta_fname_size, delta_fname_ext,
                ar_strlen(delta_fname_ext, ext_size));
		}
	}
	else
	{
		//Delta file path is provided, so form the full path
		//Ex. ..\acdb_data + \ + acdb_data.acdb + delta
		uint32_t acdb_file_name_len = 0;
		uint32_t offset = 0;
		int32_t char_index = (int32_t)fname_len;
		char *slash = NULL;

		/* Detect windows\linux style path prefixes
		Absolute Paths
			Ex. C:\acdb_data\ar_acdb.acdb
			Ex. /acdb_data/ar_acdb.acdb
		Relative paths
			Ex. ..\acdb_data\ar_acdb.acdb
			Ex. ../acdb_data/ar_acdb.acdb
		*/
		//if (!((fname[0] == '.' && fname[1] == '.') ||
		//	(fname[1] == ':' && fname[2] == '\\') ||
		//	fname[0] == '/'))
		//{
		//	char_index = 0;
		//}

		while (--char_index > 0)
		{
			if (fname[char_index] == '\\' ||
				fname[char_index] == '/')
			{
                if (fname[char_index] == '/')
                {
                    slash = "/";
                    slash_size = 2;
                }

                if (fname[char_index] == '\\')
                {
                    slash = "\\";
                    slash_size = 3;
                }

				char_index++;
				break;
			}
		}

		acdb_file_name_len = (uint32_t)fname_len - char_index;

		if (IsNull(slash))
		{
			delta_fname_size =
				(size_t)delta_fpath->fileNameLen
				+ (size_t)acdb_file_name_len
				+ ar_strlen(delta_fname_ext, ext_size) + 1UL;
		}
		else
		{
			delta_fname_size =  delta_fpath->fileNameLen
                + ar_strlen(slash, slash_size)
                + acdb_file_name_len
                + (ar_strlen(delta_fname_ext, ext_size) + 1);
		}

		delta_fname = ACDB_MALLOC(char, delta_fname_size);//account for \0

		if (IsNull(delta_fname)) return NULL;

		//delta file path: ..\acdb_data
		ACDB_STR_CPY_SAFE(delta_fname + offset, delta_fname_size,
            delta_fpath->fileName, delta_fpath->fileNameLen);
		offset += delta_fpath->fileNameLen;
		remaining_size = delta_fname_size - delta_fpath->fileNameLen;

		//slash
		if (!IsNull(slash))
		{
			ACDB_STR_CPY_SAFE(delta_fname + offset, remaining_size,
                slash, ar_strlen(slash, slash_size));
			offset += (uint32_t)ar_strlen(slash, slash_size);
			remaining_size = delta_fname_size
                - delta_fpath->fileNameLen - ar_strlen(slash, slash_size);
		}

		//acdb_data.acdbdelta
		ACDB_STR_CPY_SAFE(delta_fname + offset, remaining_size,
            fname + char_index, acdb_file_name_len);
		offset += acdb_file_name_len;
		if (IsNull(slash))
		{
			remaining_size = delta_fname_size
                - delta_fpath->fileNameLen - acdb_file_name_len;
		}
		else
		{
			remaining_size = delta_fname_size
                - delta_fpath->fileNameLen
                - ar_strlen(slash, slash_size) - acdb_file_name_len;
		}

        ACDB_STR_CAT_SAFE(delta_fname, delta_fname_size,
            delta_fname_ext, ar_strlen(delta_fname_ext, ext_size));

		uint32_t delta_file_name_len =
            (uint32_t)ar_strlen(delta_fname, delta_fname_size);
		if ((uint32_t)delta_fname_size != delta_file_name_len + 1UL)
		{
			ACDB_FREE(delta_fname);
			delta_fname = NULL;
			return delta_fname;
		}
	}

	return delta_fname;
}

int32_t AcdbInitUtilOpenDeltaFile(const char* pFilename, uint32_t fileNameLen, AcdbFile* deltaFilePath, uint32_t* pDeltaFileDataLen)
{
	ar_fhandle* fhandle = ACDB_MALLOC(ar_fhandle, 1);
	if (fhandle == NULL)
	{
		ACDB_ERR("File handle memory allocation failed");
		return ACDB_UTILITY_INIT_FAILURE;
	}

	*pDeltaFileDataLen = 0;
	if (pFilename == NULL || fileNameLen == 0)
	{
		ACDB_ERR("Acdb filename is null");
		return ACDB_UTILITY_INIT_FAILURE;
	}
	else
	{
		char *pDeltaFileName = NULL;
		pDeltaFileName = GetDeltaFileName(pFilename, (size_t)fileNameLen, deltaFilePath);

		if (pDeltaFileName == NULL)
		{
			ACDB_ERR("Delta filename is null");
			return ACDB_UTILITY_INIT_FAILURE;
		}
		else
		{
			//Check to see if file exists otherwise create it
			int32_t status = ar_fopen(fhandle, pDeltaFileName, AR_FOPEN_READ_ONLY_WRITE);
			if (AR_FAILED(status) || fhandle == NULL)
			{
                ACDB_DBG("Delta Acdb file does not exist. Creating new file...");
				status = ar_fopen(fhandle, pDeltaFileName, AR_FOPEN_WRITE_ONLY);
				if (status != 0 || fhandle == NULL)
				{
					ACDB_FREE(pDeltaFileName);

					ACDB_ERR("Error(%d): Unable to create a new delta acdb file", status);
					return ACDB_UTILITY_INIT_FAILURE;
				}
			}

			*pDeltaFileDataLen = (uint32_t)ar_fsize(*fhandle);
			status = ar_fclose(*fhandle);
			if (status != 0)
			{
				ACDB_ERR("Error(%d): Unable to close the delta file close", status);
				return AR_EFAILED;
			}

			ACDB_FREE(pDeltaFileName);
		}
	}

	ACDB_FREE(fhandle);

	return ACDB_UTILITY_INIT_SUCCESS;
}

int32_t AcdbInitUtilGetDeltaFileData(const char* pFilename, uint32_t fileNameLen, ar_fhandle* fhandle,  AcdbFile* deltaFilePath)
{
	int32_t result = ACDB_UTILITY_INIT_SUCCESS;
	//size_t fileSize = 0;
	if (pFilename == NULL)
	{
		ACDB_ERR("AcdbInitUtilGetDeltaFileData() failed, filename is null");
		return ACDB_UTILITY_INIT_FAILURE;
	}
	else
	{
		char *pDeltaFileName = NULL;
		pDeltaFileName = GetDeltaFileName(pFilename, fileNameLen, deltaFilePath);
		if (pDeltaFileName == NULL)
		{
			ACDB_ERR("AcdbInitUtilGetDeltaFileData() failed, delta filename is null");
			return ACDB_UTILITY_INIT_FAILURE;
		}
		else
		{
			int err = ar_fopen(fhandle, pDeltaFileName, AR_FOPEN_READ_ONLY_WRITE);
			if (err != 0 || fhandle == NULL)
			{
				ACDB_FREE(pDeltaFileName);
				ACDB_ERR("AcdbInitUtilGetDeltaFileData() failed, unable to open delta acdb file %d", err);
				return ACDB_UTILITY_INIT_FAILURE;
			}

			//fileSize = (uint32_t)ar_fsize(*fhandle);

			int ret = ar_fseek(*fhandle, 0, AR_FSEEK_BEGIN);
			if (ret != 0)
			{
				ACDB_ERR("AcdbInitUtilGetDeltaFileData() failed, file seek was unsuccessful  %d", ret);
				return AR_EFAILED;
			}

			ACDB_FREE(pDeltaFileName);
		}
	}
	return result;
}

int32_t AcdbInitUtilDeleteDeltaFileData(const char* pFilename, uint32_t fileNameLen, AcdbFile* deltaFilePath)
{
	int32_t result = ACDB_UTILITY_INIT_SUCCESS;
	int32_t delFileResult = 0;
	char *pDeltaFileName = NULL;

	pDeltaFileName = GetDeltaFileName(pFilename, fileNameLen, deltaFilePath);
	if (pDeltaFileName == NULL)
	{
		ACDB_ERR("AcdbInitUtilDeleteDeltaFileData() failed, delta filename is null");
		return ACDB_UTILITY_INIT_FAILURE;
	}
	else
	{
		delFileResult = ar_fdelete(pDeltaFileName);
		if (delFileResult != 0)
		{
			ACDB_ERR("AcdbInitUtilDeleteDeltaFileData() failed, unable to delete delta acdb file %d", delFileResult);
			result = ACDB_UTILITY_INIT_FAILURE;
		}

		ACDB_FREE(pDeltaFileName);
	}

	return result;
}

