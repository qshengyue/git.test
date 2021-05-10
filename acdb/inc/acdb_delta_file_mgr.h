#ifndef __ACDB_DELTA_FILE_MGR_H__
#define __ACDB_DELTA_FILE_MGR_H__
/**
*=============================================================================
* \file acdb_delta_file_mgr.h
*
* \brief
*		This file contains the implementation of the delta acdb file accessor 
*		interfaces.
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

#include "acdb_file_mgr.h"
#include "acdb_utility.h"
#include "ar_osal_file_io.h"
#include "acdb_delta_parser.h"

/* ---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *--------------------------------------------------------------------------- */
enum AcdbDeltaDataCmds {
	ACDB_DELTA_DATA_CMD_INIT = 0,
	ACDB_DELTA_DATA_CMD_RESET,
	ACDB_DELTA_DATA_CMD_CLEAR_FILE_BUFFER,
	ACDB_DELTA_DATA_CMD_GET_FILE_VERSION,
	ACDB_DELTA_DATA_CMD_INIT_HEAP,
	ACDB_DELTA_DATA_CMD_SET_DATA,
	ACDB_DELTA_DATA_CMD_GET_DATA,
	ACDB_DELTA_DATA_CMD_SAVE,
    ACDB_DELTA_DATA_CMD_DELETE_ALL_FILES
};


/* ---------------------------------------------------------------------------
 * Type Declarations
 *--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Class Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Function Declarations and Documentation
 *--------------------------------------------------------------------------- */

//These structures should be handled by the parser
//typedef struct _AcdbDeltaFileVersion AcdbDeltaFileVersion;
//#include "acdb_begin_pack.h"
//struct _AcdbDeltaFileVersion{
//   uint32_t majorVersion;
//   uint32_t minorVersion;
//}
//#include "acdb_end_pack.h"
//;
//
//
//typedef struct _AcdbCmdDeltaFileInfo AcdbCmdDeltaFileInfo;
//#include "acdb_begin_pack.h"
//struct _AcdbCmdDeltaFileInfo{
//   uint32_t fileIndex;//What is fiel index?
//   AcdbFileInfo nFileInfo;
//   uint32_t isFileUpdated;
//   uint32_t deltaFileExists;
//   uint32_t file_size;
//   ar_fhandle file_ptr;
//}
//#include "acdb_end_pack.h"
//;

int32_t acdb_delta_data_ioctl (uint32_t nCommandId,
                        uint8_t *pInput,
                        uint32_t nInputSize,
                        uint8_t *pOutput);

#endif /* __ACDB_DELTA_FILE_MGR_H__ */

