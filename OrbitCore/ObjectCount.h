#pragma once

#include <map>
#include <string>
#include "Threading.h"

//-----------------------------------------------------------------------------
class ObjectCounter
{
public:
	int Inc( const char* a_ObjectType );
	int Dec( const char* a_ObjectType );
	int Count( const char* a_ObjecType );

private:
	std::map< std::string, int > m_ObjectCount;
	Mutex m_Mutex;
};

extern ObjectCounter GObjectCounter;