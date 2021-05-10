#ifndef __ACDB_PARSER_H__
#define __ACDB_PARSER_H__
/**
*=============================================================================
* \file acdb_parser.h
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
#include "ar_osal_types.h"
#include "ar_osal_file_io.h"

/* ---------------------------------------------------------------------------
* Preprocessor Definitions and Constants
*--------------------------------------------------------------------------- */

#define ACDB_PARSE_COMPRESSED 1
#define ACDB_PARSE_SUCCESS 0
#define ACDB_PARSE_FAILURE -1
#define ACDB_PARSE_CHUNK_NOT_FOUND -2
#define ACDB_PARSE_INVALID_FILE -3

//Graph Key Vector
#define ACDB_CHUNKID_GKVKEYTBL                  0x54564b47 //GKVT
#define ACDB_CHUNKID_GKVLUTTBL                  0x4c564b47 //GKVL

//Subgraph Connections
#define ACDB_CHUNKID_SGCONNLUT                  0x554c4353 //SCLU
#define ACDB_CHUNKID_SGCONNDEF                  0x45444353 //SCDE
#define ACDB_CHUNKID_SGCONNDOT                  0x4f444353 //SCDO

//Subgraph Calibration
#define ACDB_CHUNKID_CALSGLUT                   0x554c5343 //CSLU
#define ACDB_CHUNKID_CALKEYTBL                  0x544b4143 //CAKT
#define ACDB_CHUNKID_CALDATALUT                 0x554c4443 //CDLU
#define ACDB_CHUNKID_CALDATADEF                 0x45444443 //CDDE
#define ACDB_CHUNKID_CALDATADOT                 0x4f444443 //CDD0
#define ACDB_CHUNKID_CALDATADEF2                0x32444443 //CDD2

//PID Persistance
#define ACDB_CHUNKID_PIDPERSIST                 0x43545050 //PPTC
#define ACDB_CHUNKID_GLB_PID_PERSIST_MAP        0x4750504d //GPPM (Global PID Persist Map)

//Datapool
#define ACDB_CHUNKID_DATAPOOL                   0x4c4f4f50 //POOL

//Module Tag Data
#define ACDB_CHUNKID_MODULE_TAGDATA_KEYTABLE    0x544b544d //MTKT
#define ACDB_CHUNKID_MODULE_TAG_KEY_LIST_LUT    0x4c4b544d //MTKL
#define ACDB_CHUNKID_MODULE_TAGDATA_LUT         0x554c544d //MTLU
#define ACDB_CHUNKID_MODULE_TAGDATA_DEF         0x4544544d //MTDE
#define ACDB_CHUNKID_MODULE_TAGDATA_DOT         0x4f44544d //MTDO

//Tagged Modules
#define ACDB_CHUNKID_TAGGED_MODULE_LUT          0x554c4d54 //TMLU
#define ACDB_CHUNKID_TAGGED_MODULE_DEF          0x45444d54 //TMDE
#define ACDB_CHUNKID_TAGGED_MODULE_MAP          0x204c544d

//GSL Calibration Data
#define ACDB_CHUNKID_GSL_CAL_LUT                0x554c4347 //GCLU
#define ACDB_CHUNKID_GSL_CALKEY_TBL             0x544b4347 //GCKT
#define ACDB_CHUNKID_GSL_CALDATA_TBL            0x54444347 //GCDT
#define ACDB_CHUNKID_GSL_DEF                    0x45444347 //GCDE
#define ACDB_CHUNKID_GSL_DOT                    0x4f444347 //GCDO

//Subgraph Module Map
#define ACDB_CHUNKID_SG_IID_MAP                 0x54494753 //SGIT

//AMDB
#define ACDB_CHUNKID_AMDB_REG_DATA              0x4D444F4D //MODM
#define ACDB_CHUNKID_BOOTUP_LOAD_MODULES        0x50555442 //BTUP

//VCPM
#define ACDB_CHUNKID_VCPM_CAL_DATA              0x44434356  //VCCD
#define ACDB_CHUNKID_VCPM_MASTER_KEY            0x4b4d4356  //VCMK
#define ACDB_CHUNKID_VCPM_CALKEY_TABLE          0x544b4356  //VCKT
#define ACDB_CHUNKID_VCPM_CALDATA_LUT           0x554c4356  //VCLU
#define ACDB_CHUNKID_VCPM_CALDATA_DEF           0x45444356  //VCDE

/*
#define ACDB 0x42444341
#define HEAD 0x44414548
*/


/* ---------------------------------------------------------------------------
* Type Declarations
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Class Definitions
*--------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
* Function Declarations and Documentation
*--------------------------------------------------------------------------- */

int32_t AcdbFileGetChunkData(
    ar_fhandle fp, const uint32_t nFileBufLen,
    uint32_t chunkID, uint32_t *pChkBuf, uint32_t *pChkLen);

int32_t AcdbFileGetChunkInfo(
    void* file_buffer, const uint32_t buffer_length,
    uint32_t chunkID, uint32_t *pChkBuf, uint32_t *pChkLen);

int32_t IsAcdbFileValid(ar_fhandle fp, const uint32_t file_size);

#endif /* __ACDB_PARSER_H__ */
