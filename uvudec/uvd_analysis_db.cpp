/*
Universal Decompiler (uvudec)
Copyright 2008 John McMaster
JohnDMcMaster@gmail.com
Licensed under the terms of the BSD license.  See LICENSE for details.
*/

#include "uvd_analysis_db.h"
#include "uvd_util.h"
#include "elf/uvd_elf.h"
#include <vector>

UVDAnalysisDB::UVDAnalysisDB()
{
}

UVDAnalysisDB::~UVDAnalysisDB()
{
}

uv_err_t UVDAnalysisDB::clear()
{
	//No way to clear without internal knowledge of DB
	return UV_DEBUG(UV_ERR_GENERAL);
}

UVDAnalysisDBArchive::UVDAnalysisDBArchive()
{
}

UVDAnalysisDBArchive::~UVDAnalysisDBArchive()
{
}

uv_err_t UVDAnalysisDBArchive::clear()
{
	//Just clear the function list
	m_functions.clear();
	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDB::queryFunctionByBinary(UVDDataChunk *dataChunk, UVDBinaryFunctionShared **func)
{
	std::vector<UVDBinaryFunctionShared *> funcs;
	
	uv_assert_err_ret(queryFunctionByBinary(dataChunk, funcs));
	uv_assert_ret(!funcs.empty());
	
	//Don't be picky for now, get what we can
	//Most should only have one impl registered anyway
	uv_assert_ret(func);
	*func = funcs[0];
	
	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDBArchive::loadData(std::string &file)
{
	uv_err_t rc = UV_ERR_GENERAL;
	std::vector<std::string> lines;
	std::vector< std::vector<std::string> > dbParts;
	std::string fileData;
	
	UV_ENTER();

	printf_debug("Reading DB file...\n");
	uv_assert_err_ret(readFile(file, fileData));	
	
	lines = split(fileData, '\n', false);

	uv_assert_err_ret(splitConfigLinesVector(lines, "NAME=", dbParts));
	printf_debug("DB parts: %d\n", dbParts.size());
	//Loop for each function
	for( std::vector< std::vector<std::string> >::size_type dbPartsIndex = 0; dbPartsIndex < dbParts.size(); ++dbPartsIndex )
	{
		std::vector<std::string> dbPart = dbParts[dbPartsIndex];
		std::vector<std::string> implLines;

		std::string valueName;
		std::string valueDesc;
		
		UVDBinaryFunctionShared *functionShared = NULL;
	
		//Go until we hit function specific impl
		for( std::vector<std::string>::size_type dbPartIndex = 0; dbPartIndex < dbPart.size(); ++dbPartIndex )
		{
			std::string key;
			std::string value;
			std::string line = dbPart[dbPartIndex];
			uv_err_t rc_temp = UV_ERR_GENERAL;

			printf_debug("Line: <%s>\n", line.c_str());
			rc_temp = uvdParseLine(line, key, value);
			if( UV_FAILED(rc_temp) )
			{
				return UV_ERR(rc);
			}
			else if( rc_temp == UV_ERR_BLANK )
			{
				continue;
			}

			if( key == "NAME" )
			{
				valueName = value;
			}
			else if( key == "DESC" )
			{
				valueDesc = value;
			}
			else if( key == "IMPL" )
			{
				//Transfer remaining to impl vector
				while( dbPartIndex < dbPart.size() )
				{
					implLines.push_back(line);
				}
				break;
			}
			else
			{
				printf_error("Unrecognized function line: %s\n", line.c_str());
				return UV_DEBUG(UV_ERR_GENERAL);
			}
		}

		functionShared = new UVDBinaryFunctionShared();
		uv_assert_ret(functionShared);
		
		functionShared->m_name = valueName;
		functionShared->m_description = valueDesc;
		
	
		std::vector< std::vector<std::string> > implParts;
		uv_assert_err_ret(splitConfigLinesVector(implLines, "BINARY_RAW=", implParts));
		//Loop for each function impl
		for( std::vector< std::vector<std::string> >::size_type implPartsIndex = 0; implPartsIndex < implParts.size(); ++implPartsIndex )
		{
			std::vector<std::string> implPart = implParts[implPartsIndex];

			std::string valueImpl;
			std::string valueSrc;

			for( std::vector<std::string>::size_type implPartIndex = 0; implPartIndex < implPart.size(); ++implPartIndex )
			{
				std::string key;
				std::string value;
				std::string line = implPart[implPartIndex];
				uv_err_t rc_temp = UV_ERR_GENERAL;

				printf_debug("Line: <%s>\n", line.c_str());
				rc_temp = uvdParseLine(line, key, value);
				if( UV_FAILED(rc_temp) )
				{
					return UV_ERR(rc);
				}
				else if( rc_temp == UV_ERR_BLANK )
				{
					continue;
				}

				if( key == "IMPL" )
				{
					valueImpl = value;
				}
				else if( key == "SRC" )
				{
					valueSrc = value;
				}
				else
				{
					printf_error("Unrecognized function implementation line: %s\n", line.c_str());
					return UV_DEBUG(UV_ERR_GENERAL);
				}
			}
			
			UVDBinaryFunctionInstance *codeShared = NULL;
			codeShared = new UVDBinaryFunctionInstance();
			uv_assert_ret(codeShared);
			
			UVDData *data = NULL;
			UVDDataChunk *dataChunk = NULL;
			uv_assert_err_ret(UVDDataFile::getUVDDataFile(&data, valueImpl));
			dataChunk = new UVDDataChunk();
			uv_assert_err_ret(dataChunk->init(data));
			codeShared->m_dataChunk = dataChunk;
			
			//Ignore valueSrc for now
			
			
			functionShared->m_representations.push_back(codeShared);
		}
	}

	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDBArchive::loadFunction(UVDBinaryFunctionShared *function)
{
	uv_assert_ret(function);
	printf_debug("loadFunction: %p\n", function);
	m_functions.push_back(function);
	return UV_ERR_OK;
}

//Raw function MD5
int g_computeFunctionMD5 = true;
//Relocatable function version MD5
//Relocatable parts should be 0'd
int g_computeFunctionRelocatableMD5 = true;

//Write a .bin file exactly as the function was found
int g_writeRawBinary = true;
//Write a .bin file with default relocatable values (MD5 should match config MD5)
//Implies writting out a complimentary file describing in text the relocations
int g_writeRelocatableBinary = true;
//Write an ELF format relocatable data
int g_writeElfFile = true;


uv_err_t UVDAnalysisDBArchive::saveFunctionCodeSharedData(UVDBinaryFunctionShared *function, UVDBinaryFunctionInstance *functionCode, const std::string &outputDir, int functionIndex, std::string &config)
{
	char buff[256];
	
	uv_assert_ret(functionCode);
	
	if( g_writeRawBinary )
	{
		//Raw binary
		std::string binRawFile;
		snprintf(buff, 256, "%s/%s_%d_raw.bin", outputDir.c_str(), function->m_name.c_str(), functionIndex);
		binRawFile = buff;
		config += "BINARY_RAW=" + binRawFile + "\n";
		uv_assert_ret(functionCode->m_dataChunk);
		uv_assert_err_ret(functionCode->m_dataChunk->saveToFile(binRawFile));
	}

#if RELOCATABLE_WRITE_BINRARIES
	if( g_writeRelocatableBinary )
	{
		//Relocatable fixed binary
		std::string binRelocatableFile;
		snprintf(buff, 256, "%s_%d_relocatable.bin", function->m_name.c_str(), functionIndex);
		binRelocatableFile = buff;
		config += "BINARY_RELOCATABLE=" + binRelocatableFile + "\n";
		uv_assert_ret(functionCode->m_dataChunk);
		uv_assert_err_ret(functionCode->m_dataChunk->saveToFile(outputDir + "/" + binRelocatableFile));
	
		//TODO: add config for relocations
		std::string binRelocatableConfigFile;
		snprintf(buff, 256, "%s/%s_%d_relocatable.cfg", outputDir.c_str(), function->m_name.c_str(), functionIndex);
		binRelocatableConfigFile = buff;
		config += "BINARY_RELOCATABLE_CONFIG=" + binRelocatableConfigFile + "\n";
		//Get relocations here and save file...
	}
#endif

	//Preferred relocation format
	if( g_writeElfFile)
	{
		UVDElf *elf = NULL;
		
		std::string elfFile;
		snprintf(buff, 256, "%s/%s_%d.elf", outputDir.c_str(), function->m_name.c_str(), functionIndex);
		elfFile = buff;
		config += "BINARY_ELF=" + elfFile + "\n";

		uv_assert_err_ret(functionCode->toUVDElf(&elf));
		uv_assert_ret(elf);
		//And save it
		uv_assert_err_ret(elf->saveToFile(elfFile));
		delete elf;
	}
	
	if( g_computeFunctionMD5 )
	{
		std::string md5;
		uv_assert_err_ret(functionCode->getHash(md5));
		config += "MD5=" + md5 + "\n";
	}

	if( g_computeFunctionRelocatableMD5 )
	{
		std::string md5;
		uv_assert_err_ret(functionCode->getRelocatableHash(md5));
		config += "MD5_RELOCATABLE=" + md5 + "\n";
	}

	//Code is optional, sometimes we just have binary and know its, say, printf
	if( !functionCode->m_code.empty() )
	{
		std::string srcFile;
		
		snprintf(buff, 256, "%s/%s_%d.c", outputDir.c_str(), function->m_name.c_str(), functionIndex);
		srcFile = buff;
		
		config += "SRC=" + srcFile + "\n";
		uv_assert_err_ret(writeFile(srcFile, functionCode->m_code));
	}

	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDBArchive::saveFunctionData(UVDBinaryFunctionShared *function, const std::string &outputDir, std::string &config)
{
	uv_assert_ret(function);
	
	//Write main config file
	//std::string configFile = outputDbFile + "/" + function->m_name + ".func";
	//std::string config;
	
	config += "NAME=" + function->m_name + "\n";
	config += "DESC=" + function->m_description + "\n";
	
	for( std::vector<UVDBinaryFunctionInstance *>::size_type j = 0; j < function->m_representations.size(); ++j )
	{	
		UVDBinaryFunctionInstance *functionCode = function->m_representations[j];
		uv_assert_err_ret(saveFunctionCodeSharedData(function, functionCode, outputDir, j, config));
		//config += "VER_MIN=" + 1 + "\n";
		//config += "VER_MAX=" + 2 + "\n";
		if( j + 1 < function->m_representations.size() )
		{
			config += "\n";
		}
	}

	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDBArchive::saveData(std::string &outputDbFile)
{
	std::string outputDir;
	
	UV_ENTER();
	
	//Assume directory
	if( UV_FAILED(isDir(outputDbFile)) )
	{
		uv_assert_err_ret(createDir(outputDbFile, false));
	}
	//May be temp dir later to create archive
	outputDir = outputDbFile;
	
	//Do one conglamerate file for now, but allow breaking up if required
	std::string configFile = outputDbFile + "/index.func";
	std::string config;
	
	printf_debug("this = %p\n", this);
	printf_debug("Iterating over functions: %d\n", m_functions.size());
	
	//Loop for each function
	for( std::vector<UVDBinaryFunctionShared *>::size_type i = 0; i < m_functions.size(); ++i )
	{
		UVDBinaryFunctionShared *function = m_functions[i];

		printf_debug("m_functions[%d] = %p\n", i, function);
		uv_assert_err_ret(saveFunctionData(function, outputDir, config));

		if( i < m_functions.size() )
		{
			config += "\n";
			config += "\n";
		}
	}	

	uv_assert_err_ret(writeFile(configFile, config));	

	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDBArchive::queryFunctionByBinary(UVDDataChunk *dataChunk, std::vector<UVDBinaryFunctionShared *> &funcs, bool bClear)
{
	if( bClear )
	{
		funcs.clear();
	}
	for( std::vector<UVDBinaryFunctionShared *>::size_type i = 0; i < m_functions.size(); ++i )
	{
		UVDBinaryFunctionShared *function = m_functions[i];
		
		uv_assert_ret(function);
		
		for( std::vector<UVDBinaryFunctionInstance *>::size_type j = 0; j < function->m_representations.size(); ++j )
		{
			UVDBinaryFunctionInstance *functionShared = NULL;
			
			functionShared = function->m_representations[j];
			uv_assert_ret(functionShared);
			
			//Match?
			if( dataChunk == functionShared->m_dataChunk )
			{
				//Yipee!
				funcs.push_back(function);
				//We already have a match for this function
				break;
			}
		}
	}
	
	return UV_ERR_OK;
}

UVDAnalysisDBConcentrator::UVDAnalysisDBConcentrator()
{
}

UVDAnalysisDBConcentrator::~UVDAnalysisDBConcentrator()
{
}

uv_err_t UVDAnalysisDBConcentrator::init()
{
	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDBConcentrator::loadData(std::string &file)
{
	return UV_DEBUG(UV_ERR_GENERAL);
}

uv_err_t UVDAnalysisDBConcentrator::saveData(std::string &file)
{
	return UV_DEBUG(UV_ERR_GENERAL);
}

uv_err_t UVDAnalysisDBConcentrator::queryFunctionByBinary(UVDDataChunk *dataChunk, std::vector<UVDBinaryFunctionShared *> &funcs, bool bClear)
{
	if( bClear )
	{
		funcs.clear();
	}
	for( std::vector<UVDAnalysisDB *>::size_type i = 0; i < m_dbs.size(); ++i )
	{
		UVDAnalysisDB *db = m_dbs[i];
		
		uv_assert_ret(db);
		uv_assert_err_ret(db->queryFunctionByBinary(dataChunk, funcs, false));
	}
	
	return UV_ERR_OK;
}

uv_err_t UVDAnalysisDBConcentrator::getAnalyzedProgramDB(UVDAnalysisDB **db)
{
	for( std::vector<UVDAnalysisDB *>::size_type i = 0; i < m_dbs.size(); ++i )
	{
		UVDAnalysisDB *db = m_dbs[i];
		
		uv_assert_ret(db);
		
		//FIXME: add implemetnation
	}
	
	return UV_DEBUG(UV_ERR_GENERAL);
}

uv_err_t UVDAnalysisDBConcentrator::clear()
{
	//Clear all encapsulated DBs
	for( std::vector<UVDAnalysisDB *>::size_type i = 0; i < m_dbs.size(); ++i )
	{
		UVDAnalysisDB *db = m_dbs[i];
		
		uv_assert_ret(db);
		uv_assert_err_ret(db-clear());
	}
	
	return UV_ERR_OK;
}
