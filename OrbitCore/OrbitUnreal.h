//-----------------------------------
// Copyright Pierric Gimmig 2013-2017
//-----------------------------------
#pragma once

#include "Core.h"
#include "Message.h"
#include <unordered_map>
#include <string>

class Function;
class Type;

struct OrbitUnreal
{
    void OnTypeAdded( Type* a_Type );
    void OnFunctionAdded( Function* a_Function );
    void NewSession();
    void Clear();

    bool HasFnameInfo();
    const OrbitUnrealInfo & GetUnrealInfo();
    std::unordered_map< DWORD64, std::wstring > & GetObjectNames() { return m_ObjectNames; }

protected:
    bool GenerateUnrealInfo();

    Type*     m_UObjectType;
    Type*     m_FnameEntryType;
    Function* m_GetDisplayNameEntryFunc;
    std::unordered_map< DWORD64, std::wstring > m_ObjectNames; // Don't access when capturing!
    OrbitUnrealInfo m_UnrealInfo;
};

extern OrbitUnreal GOrbitUnreal;