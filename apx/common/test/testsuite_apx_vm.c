/*****************************************************************************
* \file      testsuite_apx_vm.c
* \author    Conny Gustafsson
* \date      2019-02-24
* \brief     Unit tests for APX Virtual Machine
*
* Copyright (c) 2019 Conny Gustafsson
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
* the Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:

* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.

* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
* CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
******************************************************************************/
//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "CuTest.h"
#include "apx_compiler.h"
#include "apx_parser.h"
#include "apx_vm.h"
#ifdef MEM_LEAK_CHECK
#include "CMemLeak.h"
#endif


//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void test_apx_vm_create(CuTest* tc);
static void test_apx_vm_decodeProgramHeader(CuTest* tc);
static void test_apx_vm_selectProgram(CuTest* tc);
static void test_apx_vm_packU8(CuTest* tc);
static void test_apx_vm_unpackU8(CuTest* tc);
static void test_apx_vm_packU16(CuTest* tc);
static void test_apx_vm_unpackU16(CuTest* tc);
static void test_apx_vm_packU32(CuTest* tc);
static void test_apx_vm_unpackU32(CuTest* tc);
static void test_apx_vm_packU8FixArray(CuTest* tc);
static void test_apx_vm_packU8DynArray(CuTest* tc);

//////////////////////////////////////////////////////////////////////////////
// PRIVATE VARIABLES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
CuSuite* testSuite_apx_vm(void)
{
   CuSuite* suite = CuSuiteNew();

   SUITE_ADD_TEST(suite, test_apx_vm_create);
   SUITE_ADD_TEST(suite, test_apx_vm_decodeProgramHeader);
   SUITE_ADD_TEST(suite, test_apx_vm_selectProgram);
   SUITE_ADD_TEST(suite, test_apx_vm_packU8);
   SUITE_ADD_TEST(suite, test_apx_vm_unpackU8);
   SUITE_ADD_TEST(suite, test_apx_vm_packU16);
   SUITE_ADD_TEST(suite, test_apx_vm_unpackU16);
   SUITE_ADD_TEST(suite, test_apx_vm_packU32);
   SUITE_ADD_TEST(suite, test_apx_vm_unpackU32);
   SUITE_ADD_TEST(suite, test_apx_vm_packU8FixArray);
   SUITE_ADD_TEST(suite, test_apx_vm_packU8DynArray);



   return suite;
}

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
static void test_apx_vm_create(CuTest* tc)
{
   apx_vm_t *vm = apx_vm_new();
   CuAssertPtrNotNull(tc, vm);
   apx_vm_delete(vm);
}

static void test_apx_vm_decodeProgramHeader(CuTest* tc)
{
   apx_compiler_t *compiler;
   uint8_t majorVersion;
   uint8_t minorVersion;
   uint32_t dataSize;
   uint8_t progType;
   adt_bytes_t *storedProgram;
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   compiler =  apx_compiler_new();
   CuAssertPtrNotNull(tc, compiler);
   apx_compiler_begin(compiler, compiledProgram);
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_encodePackProgramHeader(compiler, APX_VM_MAJOR_VERSION, APX_VM_MINOR_VERSION, 0x12345678));
   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_decodeProgramHeader(storedProgram, &majorVersion, &minorVersion, &progType, &dataSize));
   CuAssertUIntEquals(tc, APX_VM_MAJOR_VERSION, majorVersion);
   CuAssertUIntEquals(tc, APX_VM_MINOR_VERSION, minorVersion);
   CuAssertUIntEquals(tc, APX_VM_HEADER_PACK_PROG, progType);
   CuAssertUIntEquals(tc,  0x12345678, dataSize);

   apx_compiler_delete(compiler);
   adt_bytearray_delete(compiledProgram);
   adt_bytes_delete(storedProgram);
}

static void test_apx_vm_selectProgram(CuTest* tc)
{
   apx_vm_t *vm = apx_vm_new();
   adt_bytes_t *storedProgram;
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element = apx_dataElement_new(APX_BASE_TYPE_UINT8, NULL);
   apx_compiler_t *compiler = apx_compiler_new();
   CuAssertPtrNotNull(tc, compiler);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_packProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compilePackDataElement(compiler, element));
   apx_compiler_end(compiler);
   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertUIntEquals(tc, APX_VM_HEADER_PACK_PROG, apx_vm_getProgType(vm));
   CuAssertUIntEquals(tc, UINT8_SIZE, apx_vm_getProgDataSize(vm));

   apx_compiler_delete(compiler);
   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   adt_bytes_delete(storedProgram);
}

static void test_apx_vm_packU8(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element = apx_dataElement_new(APX_BASE_TYPE_UINT8, NULL);
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_sv_t *sv = dtl_sv_new();
   uint8_t dataBuffer[UINT8_SIZE];

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_packProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compilePackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);
   storedProgram = adt_bytearray_bytes(compiledProgram);
   dataBuffer[0]=0xff;
   dtl_sv_set_u32(sv, 0u);

   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, dataBuffer, (apx_size_t) sizeof(dataBuffer)));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, 1u, apx_vm_getBytesWritten(vm));
   CuAssertUIntEquals(tc, 0u, dataBuffer[0]);

   dtl_sv_set_u32(sv, 255u);
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, dataBuffer, (apx_size_t) sizeof(dataBuffer)));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, 1u, apx_vm_getBytesWritten(vm));
   CuAssertUIntEquals(tc, 255u, dataBuffer[0]);

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   dtl_dec_ref(sv);
   adt_bytes_delete(storedProgram);
}

static void test_apx_vm_unpackU8(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element = apx_dataElement_new(APX_BASE_TYPE_UINT8, NULL);
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_sv_t *sv;
   dtl_dv_t *dv;
   uint8_t dataBuffer[UINT8_SIZE*3] = {0x00, 0xab, 0xff};

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_unpackProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compileUnpackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);

   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[0], (apx_size_t) UINT8_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, 1u, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, dataBuffer[0], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[1], (apx_size_t) UINT8_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, 1u, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, dataBuffer[1], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[2], (apx_size_t) UINT8_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, 1u, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, dataBuffer[2], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   adt_bytes_delete(storedProgram);

}

static void test_apx_vm_packU16(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element = apx_dataElement_new(APX_BASE_TYPE_UINT16, NULL);
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_sv_t *sv = dtl_sv_new();
   uint16_t valuesToPack[3] = {0x0000, 0x1234, 0xffff};
   uint8_t expectedBuffer[UINT16_SIZE*3] = {0x00, 0x00, 0x34, 0x12, 0xff, 0xff};
   uint8_t dataBuffer[UINT16_SIZE*3];

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_packProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compilePackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);
   dtl_sv_set_u32(sv, valuesToPack[0]);

   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, &dataBuffer[0], (apx_size_t) UINT16_SIZE));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, UINT16_SIZE, apx_vm_getBytesWritten(vm));
   CuAssertIntEquals(tc, 0, memcmp(&expectedBuffer[0], &dataBuffer[0], (size_t) UINT16_SIZE));

   dtl_sv_set_u32(sv, valuesToPack[1]);
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, &dataBuffer[2], (apx_size_t) UINT16_SIZE));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, UINT16_SIZE, apx_vm_getBytesWritten(vm));
   CuAssertIntEquals(tc, 0, memcmp(&expectedBuffer[2], &dataBuffer[2], (size_t) UINT16_SIZE));

   dtl_sv_set_u32(sv, valuesToPack[2]);
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, &dataBuffer[4], (apx_size_t) UINT16_SIZE));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, UINT16_SIZE, apx_vm_getBytesWritten(vm));
   CuAssertIntEquals(tc, 0, memcmp(&expectedBuffer[4], &dataBuffer[4], (size_t) UINT16_SIZE));

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   dtl_dec_ref(sv);
   adt_bytes_delete(storedProgram);
}

static void test_apx_vm_unpackU16(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element = apx_dataElement_new(APX_BASE_TYPE_UINT16, NULL);
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_sv_t *sv;
   dtl_dv_t *dv;
   uint16_t expectedValues[3] = {0x0000, 0x1234, 0xffff};
   uint8_t dataBuffer[UINT16_SIZE*3] = {0x00, 0x00, 0x34, 0x12, 0xff, 0xff};

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_unpackProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compileUnpackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);

   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[0], (apx_size_t) UINT16_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, UINT16_SIZE, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, expectedValues[0], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[2], (apx_size_t) UINT16_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, UINT16_SIZE, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, expectedValues[1], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[4], (apx_size_t) UINT16_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, UINT16_SIZE, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, expectedValues[2], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   adt_bytes_delete(storedProgram);
}

static void test_apx_vm_packU32(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element = apx_dataElement_new(APX_BASE_TYPE_UINT32, NULL);
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_sv_t *sv = dtl_sv_new();
   uint32_t valuesToPack[3] = {0x00000000, 0x12345678, 0xffffffff};
   uint8_t expectedBuffer[UINT32_SIZE*3] = {0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0xff, 0xff, 0xff, 0xff};
   uint8_t dataBuffer[UINT32_SIZE*3];

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_packProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compilePackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);
   dtl_sv_set_u32(sv, valuesToPack[0]);

   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, &dataBuffer[0], (apx_size_t) UINT32_SIZE));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, UINT32_SIZE, apx_vm_getBytesWritten(vm));
   CuAssertIntEquals(tc, 0, memcmp(&expectedBuffer[0], &dataBuffer[0], (size_t) UINT32_SIZE));

   dtl_sv_set_u32(sv, valuesToPack[1]);
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, &dataBuffer[4], (apx_size_t) UINT32_SIZE));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, UINT32_SIZE, apx_vm_getBytesWritten(vm));
   CuAssertIntEquals(tc, 0, memcmp(&expectedBuffer[2], &dataBuffer[2], (size_t) UINT32_SIZE));

   dtl_sv_set_u32(sv, valuesToPack[2]);
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, &dataBuffer[8], (apx_size_t) UINT32_SIZE));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) sv));
   CuAssertUIntEquals(tc, UINT32_SIZE, apx_vm_getBytesWritten(vm));
   CuAssertIntEquals(tc, 0, memcmp(&expectedBuffer[4], &dataBuffer[4], (size_t) UINT32_SIZE));

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   dtl_dec_ref(sv);
   adt_bytes_delete(storedProgram);
}

static void test_apx_vm_unpackU32(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element = apx_dataElement_new(APX_BASE_TYPE_UINT32, NULL);
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_sv_t *sv;
   dtl_dv_t *dv;
   uint32_t expectedValues[3] = {0x00000000, 0x12345678, 0xffffffff};
   uint8_t dataBuffer[UINT32_SIZE*3] = {0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0xff, 0xff, 0xff, 0xff};

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_unpackProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compileUnpackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);

   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[0], (apx_size_t) UINT32_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, UINT32_SIZE, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, expectedValues[0], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[4], (apx_size_t) UINT32_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, UINT32_SIZE, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, expectedValues[1], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setReadBuffer(vm, &dataBuffer[8], (apx_size_t) UINT32_SIZE ));
   dv = (dtl_dv_t*) 0;
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_unpackValue(vm, &dv));
   CuAssertUIntEquals(tc, UINT32_SIZE, apx_vm_getBytesRead(vm));
   CuAssertPtrNotNull(tc, dv);
   CuAssertUIntEquals(tc, DTL_DV_SCALAR, dtl_dv_type(dv));
   sv = (dtl_sv_t*) dv;
   CuAssertUIntEquals(tc, expectedValues[2], dtl_sv_to_u32(sv, NULL));
   dtl_dec_ref(sv);

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   adt_bytes_delete(storedProgram);

}

static void test_apx_vm_packU8FixArray(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element;
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_av_t *av = dtl_av_new();
   uint8_t dataBuffer[UINT8_SIZE*4];

   element = apx_dataElement_new(APX_BASE_TYPE_UINT8, NULL);
   apx_dataElement_setArrayLen(element, 4);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_packProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compilePackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);
   memset(&dataBuffer[0], 0xff, sizeof(dataBuffer));
   dtl_av_push(av, (dtl_dv_t*) dtl_sv_make_u32(1u), false);
   dtl_av_push(av, (dtl_dv_t*) dtl_sv_make_u32(2u), false);
   dtl_av_push(av, (dtl_dv_t*) dtl_sv_make_u32(3u), false);
   dtl_av_push(av, (dtl_dv_t*) dtl_sv_make_u32(4u), false);

   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, dataBuffer, (apx_size_t) sizeof(dataBuffer)));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) av));
   CuAssertUIntEquals(tc, 4u, apx_vm_getBytesWritten(vm));
   CuAssertUIntEquals(tc, 1u, dataBuffer[0]);
   CuAssertUIntEquals(tc, 2u, dataBuffer[1]);
   CuAssertUIntEquals(tc, 3u, dataBuffer[2]);
   CuAssertUIntEquals(tc, 4u, dataBuffer[3]);

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   dtl_dec_ref(av);
   adt_bytes_delete(storedProgram);
}

static void test_apx_vm_packU8DynArray(CuTest* tc)
{
   adt_bytes_t *storedProgram;
   apx_vm_t *vm = apx_vm_new();
   adt_bytearray_t *compiledProgram = adt_bytearray_new(APX_PROGRAM_GROW_SIZE);
   apx_dataElement_t *element;
   apx_compiler_t *compiler = apx_compiler_new();
   dtl_av_t *av = dtl_av_new();
   uint8_t dataBuffer[UINT8_SIZE*10];

   element = apx_dataElement_new(APX_BASE_TYPE_UINT8, NULL);
   apx_dataElement_setArrayLen(element, 10);
   apx_dataElement_setDynamicArray(element);

   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_begin_packProgram(compiler, compiledProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_compiler_compilePackDataElement(compiler, element));
   apx_compiler_end(compiler);
   apx_compiler_delete(compiler);
   memset(&dataBuffer[0], 0xff, sizeof(dataBuffer));
   dtl_av_push(av, (dtl_dv_t*) dtl_sv_make_u32(1u), false);
   storedProgram = adt_bytearray_bytes(compiledProgram);
   CuAssertUIntEquals(tc, APX_NO_ERROR, apx_vm_selectProgram(vm, storedProgram));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_setWriteBuffer(vm, dataBuffer, (apx_size_t) sizeof(dataBuffer)));
   CuAssertIntEquals(tc, APX_NO_ERROR, apx_vm_packValue(vm, (dtl_dv_t*) av));
   CuAssertUIntEquals(tc, 2u, apx_vm_getBytesWritten(vm));
   CuAssertUIntEquals(tc, 1u, dataBuffer[0]);
   CuAssertUIntEquals(tc, 1u, dataBuffer[1]);

   apx_vm_delete(vm);
   adt_bytearray_delete(compiledProgram);
   apx_dataElement_delete(element);
   dtl_dec_ref(av);
   adt_bytes_delete(storedProgram);

}