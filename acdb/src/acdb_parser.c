/**
*=============================================================================
* \file acdb_parser.c
*
* \brief
*		Handle parsing the ACDB Data files.
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

#include "acdb_parser.h"
#include "acdb_common.h"
#include "ar_osal_file_io.h"
#include "ar_osal_error.h"
#include "ar_osal_mem_op.h"

/* ---------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*--------------------------------------------------------------------------- */

#define ACDB_CHUNK_ID_SIZE 4
#define ACDB_CHUNK_SIZE 4
#define ACDH_CHUNK_HEADER_SIZE (ACDB_CHUNK_ID_SIZE + ACDB_CHUNK_SIZE)

#define ACDB_FILE_TYPE 0x0
#define ACDB_AMDB_FILE_TYPE 0x42444d41 // AMDB
#define ACDB_FILE_ID 0x42444341 // ACDB

#define ACDB_FILE_COMPRESSED 0x1

#define ACDB_FILE_VERSION_MAJOR 0x00000001
#define ACDB_FILE_VERSION_MINOR 0x00000000

/* ---------------------------------------------------------------------------
* Type Declarations
*--------------------------------------------------------------------------- */

typedef struct _AcdbFileProperties
{
	uint32_t file_ID;
	uint32_t file_type;
	uint32_t acdb_data_len;
} AcdbFileProperties;

typedef struct _AcdbSwVersion
{
	uint32_t major;
	uint32_t minor;
	uint32_t rev;
}AcdbSwVersion;

typedef struct _AcdbSwVersionV2
{
	uint32_t major;
	uint32_t minor;
	uint32_t rev;
   uint32_t cplInfo;
}AcdbSwVersionV2;

#include "acdb_begin_pack.h"
struct _AcdbChunkHeader
{
	uint32_t id;
	uint32_t length;
}
#include "acdb_end_pack.h"
;

typedef struct _AcdbChunkHeader AcdbChunkHeader;

/* ---------------------------------------------------------------------------
* Global Data Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Static Variable Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Static Function Declarations and Definitions
*--------------------------------------------------------------------------- */

int32_t AcdbFileGetChunkData(ar_fhandle fp,const uint32_t nFileBufLen,uint32_t chunkID, uint32_t *pChkBuf,uint32_t *pChkLen)
{
	int32_t result = ACDB_PARSE_CHUNK_NOT_FOUND;
	uint32_t offset = 0;
	size_t bytes_read = 0;

	if (ACDB_PARSE_SUCCESS != IsAcdbFileValid(fp, nFileBufLen))
	{
		return ACDB_PARSE_INVALID_FILE;
	}

	// skip the file properties part of the acdb file to point to the first chunk
	offset += sizeof(AcdbFileProperties);
	while(offset < nFileBufLen && ((nFileBufLen-offset)>=sizeof(AcdbChunkHeader)))
	{
		AcdbChunkHeader ChkHdr;
		AcdbChunkHeader *pChkHdr = &ChkHdr;
		memset(&ChkHdr, 0, sizeof(ChkHdr));

		int ret = ar_fseek(fp, offset, AR_FSEEK_BEGIN);
		if (ret != 0)
		{
			ACDB_ERR("AcdbFileGetChunkData() failed, file seek to %d was unsuccessful", offset);
			return AR_EFAILED;
		}
		ret = ar_fread(fp, &ChkHdr, sizeof(AcdbChunkHeader), &bytes_read);
		if (ret != 0)
		{
			ACDB_ERR("AcdbFileGetChunkData() failed, ChkHdr read was unsuccessful");
			return AR_EFAILED;
		}

		offset += ACDH_CHUNK_HEADER_SIZE;
		if(pChkHdr->id == chunkID)
		{
			*pChkBuf = offset;
			*pChkLen = pChkHdr->length;
			result = ACDB_PARSE_SUCCESS;
			break;
		}
		offset += pChkHdr->length;
	}
	return result;
}

int32_t AcdbFileGetChunkInfo(
    void* file_buffer, const uint32_t buffer_length,
    uint32_t chunkID, uint32_t *pChkBuf, uint32_t *pChkLen)
{
    int32_t result = ACDB_PARSE_CHUNK_NOT_FOUND;
    uint8_t *start_ptr = NULL;
    uint8_t *end_ptr = NULL;
    AcdbChunkHeader *header = NULL;

    if (file_buffer == NULL || buffer_length == 0 || buffer_length < sizeof(AcdbFileProperties))
    {
        return result;
    }

    // skip the file properties part of the acdb file to point to the first chunk
    start_ptr = (uint8_t*)file_buffer + sizeof(AcdbFileProperties);
    end_ptr = (uint8_t*)file_buffer + buffer_length;


    while (start_ptr < end_ptr)
    {
        header = (AcdbChunkHeader*)start_ptr;

        if (header->id == chunkID)
        {
            *pChkBuf = (uint32_t)(start_ptr - (uint8_t*)file_buffer + sizeof(AcdbChunkHeader));
            *pChkLen = header->length;
            result = ACDB_PARSE_SUCCESS;
            break;
        }

        start_ptr += sizeof(AcdbChunkHeader) + header->length;
    }
    return result;
}

int32_t IsAcdbFileValid(ar_fhandle fp,const uint32_t file_size)
{
	size_t bytes_read = 0;
	AcdbFileProperties FileProp;
	AcdbFileProperties *pFileProp = &FileProp;
	memset(&FileProp, 0, sizeof(FileProp));
	int ret = ar_fseek(fp, 0, AR_FSEEK_BEGIN);
	if (ret != 0)
	{
		ACDB_ERR("IsAcdbFileValid() failed, file seek to %d was unsuccessful", 0);
		return AR_EFAILED;
	}
	ret = ar_fread(fp, &FileProp, sizeof(AcdbFileProperties), &bytes_read);
	if (ret != 0)
	{
		ACDB_ERR("IsAcdbFileValid() failed, FileProp read was unsuccessful");
		return AR_EFAILED;
	}

	if (file_size < sizeof(AcdbFileProperties))
	{
		ACDB_ERR("IsAcdbFileValid() failed, filesize = %d is less than size of AcdbFileProperties", file_size);
		return ACDB_PARSE_INVALID_FILE;
	}

	if (pFileProp->file_ID != ACDB_FILE_ID && pFileProp->file_type != ACDB_FILE_TYPE)
	{
		ACDB_ERR("IsAcdbFileValid() failed, fileID = %d is invalid", pFileProp->file_ID);
		return ACDB_PARSE_INVALID_FILE;
	}

	if (file_size != (pFileProp->acdb_data_len + sizeof(AcdbFileProperties)))
	{
		ACDB_ERR("IsAcdbFileValid() failed, filesize = %d is invalid", file_size);
		return ACDB_PARSE_INVALID_FILE;
	}

	return ACDB_PARSE_SUCCESS;
}



