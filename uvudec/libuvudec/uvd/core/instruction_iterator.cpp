/*
UVNet Universal Decompiler (uvudec)
Copyright 2008 John McMaster <JohnDMcMaster@gmail.com>
Licensed under the terms of the LGPL V3 or later, see COPYING for details
*/

#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <vector>
#include "uvd/architecture/architecture.h"
#include "uvd/assembly/address.h"
#include "uvd/assembly/instruction.h"
#include "uvd/core/analysis.h"
#include "uvd/core/runtime.h"
#include "uvd/core/uvd.h"
#include "uvd/data/data.h"
#include "uvd/language/format.h"
#include "uvd/string/engine.h"
#include "uvd/string/string.h"
#include "uvd/util/benchmark.h"
#include "uvd/util/debug.h"
#include "uvd/util/error.h"
#include "uvd/util/types.h"
#include "uvd/util/util.h"

UVDInstructionIterator::UVDInstructionIterator()
{
	m_instruction = NULL;
	m_uvd = NULL;
	m_addressSpace = NULL;
	m_nextPosition = 0;
	//m_isEnd = FALSE;
	m_currentSize = 0;
}

/*
UVDInstructionIterator::UVDInstructionIterator(UVD *disassembler)
{
	UV_DEBUG(init(disassembler));
}

UVDInstructionIterator::UVDInstructionIterator(UVD *disassembler, uv_addr_t position, uint32_t index)
{
	UV_DEBUG(init(disassembler, position, index));
}
*/

UVDInstructionIterator::~UVDInstructionIterator()
{
	UV_DEBUG(deinit());
}

uv_err_t UVDInstructionIterator::init(UVD *uvd, UVDAddressSpace *addressSpace)
{
	uv_addr_t minAddress = 0;
	
	uv_assert_ret(uvd);
	uv_assert_ret(addressSpace);	
	uv_assert_err_ret(addressSpace->getMinValidAddress(&minAddress));
	
	return UV_DEBUG(init(uvd, UVDAddress(minAddress, addressSpace)));
}

uv_err_t UVDInstructionIterator::init(UVD *uvd, UVDAddress address)
{
	//m_isEnd = FALSE;
	m_currentSize = 0;
	m_addressSpace = address.m_space;
	uv_assert_ret(m_addressSpace);
	m_uvd = uvd;
	m_nextPosition = address.m_addr;
	uv_assert_err_ret(prime());
	return UV_ERR_OK;
}

uv_err_t UVDInstructionIterator::prime()
{
	//FIXME: this should probably be removed
	
	uint32_t holdPosition = m_nextPosition;
	uv_err_t rcTemp = UV_ERR_GENERAL;
	
	printf_debug("Priming iterator\n");
	rcTemp = nextValidExecutableAddressIncludingCurrent();
	uv_assert_err_ret(rcTemp);
	//Eh this should be rare
	//We don't prime
	if( rcTemp == UV_ERR_DONE )
	{
		uv_assert_err_ret(makeEnd());
	}
	//This will cause last instruction (which doesn't exist) to be negated
	m_currentSize = 0;
	if( UV_FAILED(next()) )
	{
		printf_error("Failed to prime iterator!\n");
		return UV_DEBUG(UV_ERR_GENERAL);
	}
	m_nextPosition = holdPosition;
	return UV_ERR_OK;
}

uv_err_t UVDInstructionIterator::deinit()
{
	m_addressSpace = NULL;
	m_uvd = NULL;
	return UV_ERR_OK;
}

uv_err_t UVDInstructionIterator::makeEnd()
{
	//uv_assert_err_ret(makeNextEnd());
	//Seems reasonable enough for now
	//Change to invalidating m_data or something later if needed
	m_nextPosition = UINT_MAX;	
	return UV_ERR_OK;
}

bool UVDInstructionIterator::isEnd()
{
	return m_nextPosition == UINT_MAX;
}

/*
uv_err_t UVDInstructionIterator::makeNextEnd()
{
	m_nextPosition = 0;
	m_isEnd = TRUE;
	return UV_ERR_OK;
}
*/

uv_addr_t UVDInstructionIterator::getPosition()
{
	return m_nextPosition;
}

uv_err_t UVDInstructionIterator::next()
{
	//uv_err_t rc = UV_ERR_GENERAL;
	//UVDBenchmark nextInstructionBenchmark;
	//uv_addr_t absoluteMaxAddress = 0;
	
	//uv_assert_err_ret(m_uvd->m_analyzer->getAddressMax(&absoluteMaxAddress));	
	//uv_assert_ret(*this != m_uvd->end());

	printf_debug("m_nextPosition: 0x%.8X\n", m_nextPosition);
		
	//Global debug sort of cutoff
	//Force to end
	//This shouldn't happen actually
	/*
	if( m_nextPosition > absoluteMaxAddress )
	{
		uv_assert_err_ret(makeEnd());
		rc = UV_ERR_DONE;
		goto error;
	}
	*/
	//But we should check that we aren't at end otherwise we will loop since end() has address set to 0
	/*
	if( m_isEnd )
	{
		rc = UV_ERR_DONE;
		//Although we are flagged for no more instructions, we need to make it bitwise end for iter==end() checks
		uv_assert_err_ret(makeEnd());
		goto error;
	}
	*/

	//Otherwise, get next output cluster
	
	//Advance to next position, don't save parsed instruction
	//Maybe we should save parsed instruction to the iter?  Seems important enough
	uv_assert_err_ret(nextCore());
	return UV_ERR_OK;
//error:
	//nextInstructionBenchmark.stop();
	//printf_debug_level(UVD_DEBUG_PASSES, "next() time: %s\n", nextInstructionBenchmark.toString().c_str());
//	return rc;
}

uv_err_t UVDInstructionIterator::nextCore()
{
	/*
	Gets the next logical print group
	These all should be associated with a small peice of data, such as a single instruction
	Ex: an address on line above + call count + disassembled instruction
	*/

	UVD *uvd = NULL;
	UVDAnalyzer *analyzer = NULL;
	UVDFormat *format = NULL;
	//UVDBenchmark nextInstructionBenchmark;
	uv_err_t rcTemp = UV_ERR_GENERAL;
			
	uvd = m_uvd;
	uv_assert_ret(uvd);
	analyzer = uvd->m_analyzer;
	uv_assert_ret(analyzer);
	format = uvd->m_format;
	uv_assert_ret(format);
	
	printf_debug("previous position we are advancing from (m_nextPosition): 0x%.8X\n", m_nextPosition);
	
	//begin() has special processing that goes directly to nextInstruction()
	//We must go past the current instruction and parse the next
	//Also, be careful that we do not land on a non-executable address
/*
static int count = 0;
++count;
*/
//printf("m_nextPosition: 0x%08X, current size: 0x%08X\n", m_nextPosition, m_currentSize);
//fflush(stdout);
	m_nextPosition += m_currentSize;
//if( count == 5 )
//UVD_BREAK();
	rcTemp = nextValidExecutableAddressIncludingCurrent();
	uv_assert_err_ret(rcTemp);
	if( rcTemp == UV_ERR_DONE )
	{
		uv_assert_err_ret(makeEnd());
		return UV_ERR_DONE;
	}
	
	//nextInstructionBenchmark.start();
	//Currently it seems we do not need to store if the instruction was properly decoded or not
	//this can be caused in a multitude of ways by a multibyte instruction
	if( UV_FAILED(parseCurrentInstruction()) )
	{
		printf_debug("Failed to get next instruction\n");
		return UV_DEBUG(UV_ERR_GENERAL);
	}	
	//nextInstructionBenchmark.stop();
	//printf_debug_level(UVD_DEBUG_SUMMARY, "nextInstruction() time: %s\n", nextInstructionBenchmark.toString().c_str());

	return UV_ERR_OK;
}

uv_err_t UVDInstructionIterator::nextValidExecutableAddress()
{
	/*
	//Should this actually be an error?
	if( m_isEnd )
	{
		return UV_ERR_DONE;
	}
	*/

	//This may put us into an invalid area, but we will find the next valid if availible
	++m_nextPosition;
	return UV_DEBUG(nextValidExecutableAddressIncludingCurrent());
}

uv_err_t UVDInstructionIterator::nextValidExecutableAddressIncludingCurrent()
{
	uv_err_t rcNextAddress = UV_ERR_GENERAL;
	
	//FIXME: look into considerations for an instruction split across areas, which probably doesn't make sense
	uv_assert_ret(m_addressSpace);
	rcNextAddress = m_addressSpace->nextValidExecutableAddress(m_nextPosition, &m_nextPosition);
	uv_assert_err_ret(rcNextAddress);
	if( rcNextAddress == UV_ERR_DONE )
	{
		//Don't do this, we might be partial through an instruction and don't want to mess up address
		//Don't try to advance further then
		//But we are in middle of decoding, so let caller figure out what to do with buffers
		//uv_assert_err_ret(makeNextEnd());
		//uv_assert_err_ret(makeEnd());
		return UV_ERR_DONE;
	}

	return UV_ERR_OK;
}

uv_err_t UVDInstructionIterator::consumeCurrentExecutableAddress(uint8_t *out)
{
	//Current address should always be valid unless we are at end()
	//This is not a hard error because it can happen from malformed opcodes in the input
	if( isEnd() )
	{
		return UV_ERR_DONE;
	}

//UVD_PRINT_STACK();	
	uv_assert_err_ret(m_addressSpace->m_data->readData(m_nextPosition, (char *)out));	
	++m_currentSize;
	//We don't care if next address leads to end
	//Current address was valid and it is up to next call to return done if required
	uv_assert_err_ret(nextValidExecutableAddress());
	
	return UV_ERR_OK;
}

uv_err_t UVDInstructionIterator::addWarning(const std::string &lineRaw)
{
	printf_warn("%s\n", lineRaw.c_str());
	return UV_ERR_OK;
}

uv_err_t UVDInstructionIterator::parseCurrentInstruction()
{
	return UV_DEBUG(m_uvd->m_runtime->m_architecture->parseCurrentInstruction(*this));
}	

UVDInstructionIterator UVDInstructionIterator::operator++()
{
	next();
	return *this;
}

bool UVDInstructionIterator::operator==(const UVDInstructionIterator &other) const
{
	
	printf_debug("UVDInstructionIterator::operator==:\n");
	//debugPrint();
	//other.debugPrint();

	//Should comapre m_uvd as well?
	return /*m_isEnd == other.m_isEnd
			&& */m_nextPosition == other.m_nextPosition;
}

bool UVDInstructionIterator::operator!=(const UVDInstructionIterator &other) const
{
	return !operator==(other);
}

uv_err_t UVDInstructionIterator::getEnd(UVD *uvd, UVDInstructionIterator &iter)
{
	uv_assert_ret(uvd);
	iter.m_uvd = uvd;
	uv_assert_err_ret(iter.makeEnd());
	return UV_ERR_OK;
}

