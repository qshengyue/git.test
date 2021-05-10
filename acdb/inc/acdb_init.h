#ifndef __ACDB_INIT_H__
#define __ACDB_INIT_H__
/**
*=============================================================================
* \file acdb_init.h
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
#include "acdb.h"
#include "acdb_file_mgr.h"
#include "acdb_delta_file_mgr.h"
/* ---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *--------------------------------------------------------------------------- */

#define ACDB_INIT_SUCCESS 0
#define ACDB_INIT_FAILURE -1

/* ---------------------------------------------------------------------------
* Type Declarations
*--------------------------------------------------------------------------- */
extern const char* WKSP_FILE_NAME_EXT;
/* ---------------------------------------------------------------------------
* Class Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Function Declarations and Documentation
 *--------------------------------------------------------------------------- */

int32_t AcdbInit(AcdbCmdFileInfo *acdb_cmd_finfo, AcdbFileInfo *acdb_finfo);

int32_t AcdbDeltaInit(AcdbCmdFileInfo *acdb_cmd_finfo, AcdbCmdDeltaFileInfo *acdb_cmd_delta_finfo, AcdbFile *delta_fpath);

bool_t AcdbInitDoSwVersionsMatch(AcdbFileInfo *acdb_finfo, AcdbDeltaFileInfo *acdb_delta_finfo);
#endif /* __ACDB_INIT_H__ */
