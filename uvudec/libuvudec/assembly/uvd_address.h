/*
UVNet Universal Decompiler (uvudec)
Copyright 2008 John McMaster <JohnDMcMaster@gmail.com>
Licensed under the terms of the LGPL V3 or later, see COPYING for details
*/

#ifndef UVD_ADDRESS_H
#define UVD_ADDRESS_H

#include <string>
#include "uvd_data.h"
//uv_addr_t is somewhat arbitrarily defined in here
#include "uvd_types.h"

//XXX: shouldn't this extend UVDAddressSpace so we can use the two transparently?
//Currently this seems only for uvdasm plugin p
#if 0
class UVDAddressSpace;
class UVDAddressSpaceMapper
{
public:
	UVDAddressSpaceMapper();
	
	uv_err_t finalizeConfig();
	
public:
	//Other address space
	//These are not owned by this object, only mapped
	UVDAddressSpace *m_dst_shared;
	UVDAddressSpace *m_src_shared;
	
	//Source memory
	//Start map address
	uint32_t m_src_min_addr;
	//Stop map address
	uint32_t m_src_max_addr;
	//Destination memory
	uint32_t m_dst_min_addr;
	uint32_t m_dst_max_addr;
};
#endif

//Shared information about an address space
class UVDAddressSpace
{
public:
	UVDAddressSpace();
	~UVDAddressSpace();
	uv_err_t deinit();
	
	uv_err_t setEquivMemName(uint32_t addr, const std::string &name);
	uv_err_t getEquivMemName(uint32_t addr, std::string &name);
	
public:
	/* short name */
	std::string m_name;
	/* longer description */
	std::string m_desc;
	/* EPROM, RAM, etc.  Defines prefixed with UV_DISASM_MEM_ */
	unsigned int m_type;
	/* Valid addresses */
	unsigned int m_min_addr;
	unsigned int m_max_addr;
	/* Used for output */
	std::string m_print_prefix;
	std::string m_print_suffix;
	/*
	Capabilities, how the memory behaves as a whole.  Read, write, etc
	Does not include policy based capabilities such as process 1 cannot write to address 0x1234
	*/
	unsigned int m_cap;
	
	/*
	Minimum amount of memory that can be transferred in bits
	Note that single bits is observed on processors such as 8051
	Might also be useful for CPU flags
	*/
	unsigned int m_word_size;
	unsigned int m_word_alignment;
	
	/*
	Address synonyms
	Ex: 8051 direct addresses IRAM @ F0 is B register
	Should these be address space mappings instead?
	*/
	std::map<uint32_t, std::string> m_synonyms;
	/*
	Does this map to something more absolute?
	If so, address that this is mapped to
	FIXME: implement this with polymorphism instead
	*/
	//std::vector<UVDAddressSpaceMapper *> m_mappers;
	/*
	struct uv_disasm_mem_shared_t *mapped;
	Start address, in target address space words
	unsigned int mapped_start_addr;
	Start address bit offet from above
	unsigned int mapped_start_addr_bit_offset;
	*/
};

/*
A fully qualified memory range
*/
class UVDAddressRange
{
public:
	UVDAddressRange();
	UVDAddressRange(unsigned int min_addr);
	UVDAddressRange(unsigned int min_addr, unsigned int max_addr, UVDAddressSpace *space = NULL);
	bool intersects(UVDAddressRange other) const;

	//By minimum address
	static int compareStatic(const UVDAddressRange *l, const UVDAddressRange *r);	
	int compare(const UVDAddressRange *other) const;	
	bool operator<(const UVDAddressRange *other) const;
	bool operator>(const UVDAddressRange *other) const;
	bool operator==(const UVDAddressRange *other) const;

public:
	/* Address space */
	UVDAddressSpace *m_space;
	/* Address */
	uint32_t m_min_addr;
	uint32_t m_max_addr;
};

#endif

