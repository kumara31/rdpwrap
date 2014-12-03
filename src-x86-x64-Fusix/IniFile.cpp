/*
  Copyright 2014 Stas'M Corp.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "stdafx.h"
#include <Windows.h>
#include "IniFile.h"

INI_FILE::INI_FILE(wchar_t *FilePath)
{
	DWORD Status = 0;
	DWORD NumberOfBytesRead = 0;

	HANDLE hFile = CreateFile(FilePath, GENERIC_ALL, FILE_SHARE_READ|FILE_SHARE_WRITE,
							NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if(hFile == INVALID_HANDLE_VALUE)
	{
		return;
	}

	FileSize = GetFileSize(hFile, NULL);
	if(FileSize == INVALID_FILE_SIZE)
	{
		return;
	}

	FileRaw = new char[FileSize];
	Status = (bool)ReadFile(hFile, FileRaw, FileSize, &NumberOfBytesRead, NULL);
	if(!Status)
	{
		return;
	}

	CreateStringsMap();
	Parse();
}


INI_FILE::~INI_FILE()
{
	for(DWORD i = 0; i < IniData.SectionCount; i++)
	{
		delete[] IniData.Section[i].Variables;
	}
	delete[] IniData.Section;
	delete[] FileStringsMap;
	delete FileRaw;
}

bool INI_FILE::CreateStringsMap()
{
	DWORD StringsCount = 1;

	for(DWORD i = 0; i < FileSize; i++)
	{
		if(FileRaw[i] == '\r' && FileRaw[i+1] == '\n') StringsCount++;
	}

	FileStringsCount = StringsCount;

	FileStringsMap = new DWORD[StringsCount];
	FileStringsMap[0] = 0;

	StringsCount = 1;

	for(DWORD i = 9; i < FileSize; i++)
	{
		if(FileRaw[i] == '\r' && FileRaw[i+1] == '\n')
		{
			FileStringsMap[StringsCount] = i+2;
			StringsCount++;
		}
	}

	return true;
}

int INI_FILE::StrTrim(char* Str, STRING_TRIM_TYPE Type)
{
    int StrLn = strlen(Str) + 1;
    if ((StrLn == 0) || (Type < TRIM_LEFT) || (Type > TRIM_BOTH)) {
        return 0;
    }
    char *NewStr = new char[StrLn];
    int IdxSrc = -1, IdxDest = 0;
    if ((Type == TRIM_LEFT) || (Type == TRIM_BOTH)) {
        bool InText = false;
        while(Str[++IdxSrc]) {
            if (!InText && (Str[IdxSrc] != ' ') && (Str[IdxSrc] != '\n') && (Str[IdxSrc] != '\t')) {
                InText = true;
            }
            if (InText) {
                NewStr[IdxDest++] = Str[IdxSrc];
            }
        }
        NewStr[IdxDest] = '\0';
    } else {
        IdxDest = StrLn - 1;
        strcpy_s(NewStr, StrLn, Str);
    }
    if ((Type == TRIM_RIGHT) || (Type == TRIM_BOTH)) {
        while(--IdxDest > 0) {
            if ((NewStr[IdxDest] != ' ') && (NewStr[IdxDest] != '\n') && (NewStr[IdxDest] != '\t')) {
                break;
            }
        }
    NewStr[IdxDest] = '\0';
    }
    strcpy_s(Str, StrLn, NewStr);
    delete NewStr;
    return IdxDest;
}

DWORD INI_FILE::GetFileStringFromNum(DWORD StringNumber, char *RetString, DWORD Size)
{
	DWORD CurrentStringNum = 0;
	DWORD EndStringPos = 0;
	DWORD StringSize = 0;

	if(StringNumber > FileStringsCount) return -1;

	for(DWORD i = FileStringsMap[StringNumber]; i < FileSize; i++)
	{
		if((FileRaw[i] == '\r' && FileRaw[i+1] == '\n') || i == (FileSize-1))
		{
			EndStringPos = i;
			break;
		}
	}

	StringSize = EndStringPos-FileStringsMap[StringNumber];

	if(Size < StringSize) return -1;

	memset(RetString, 0x00, Size);
	memcpy(RetString, &(FileRaw[FileStringsMap[StringNumber]]), StringSize);
	return StringSize;
}

bool INI_FILE::IsVariable(char *Str, DWORD StrSize)
{
	bool Quotes = false;

	for(DWORD i = 0; i < StrSize; i++)
	{
		if(Str[i] == '"' || Str[i] == '\'') Quotes = !Quotes;
		if(Str[i] == '=' && !Quotes) return true;
	}
	return false;
}

bool INI_FILE::FillVariable(INI_SECTION_VARIABLE *Variable, char *Str, DWORD StrSize)
{
	bool Quotes = false;

	for(DWORD i = 0; i < StrSize; i++)
	{
		if(Str[i] == '"' || Str[i] == '\'') Quotes = !Quotes;
		if(Str[i] == '=' && !Quotes)
		{
			memcpy(Variable->VariableName, Str, i);
			memcpy(Variable->VariableValue, &(Str[i+1]), StrSize-(i-1));
			//StrTrim(Variable->VariableName, TRIM_BOTH);
			//StrTrim(Variable->VariableValue, TRIM_BOTH);
			break;
		}
	}
	return true;
}

bool INI_FILE::Parse()
{
	DWORD CurrentStringNum = 0;
	char CurrentString[512];
	DWORD CurrentStringSize = 0;

	DWORD SectionsCount = 0;
	DWORD VariablesCount = 0;

	DWORD CurrentSectionNum = -1;
	DWORD CurrentVariableNum = -1;

	// Calculate section count
	for(DWORD CurrentStringNum = 0; CurrentStringNum < FileStringsCount; CurrentStringNum++)
	{
		CurrentStringSize = GetFileStringFromNum(CurrentStringNum, CurrentString, 512);

		if(CurrentString[0] == ';') continue; // It's a comment
		
		if(CurrentString[0] == '[' && CurrentString[CurrentStringSize-1] == ']')	// It's section declaration
		{
			SectionsCount++;
			continue;
		}
	}

	DWORD *SectionVariableCount = new DWORD[SectionsCount];
	memset(SectionVariableCount, 0x00, sizeof(DWORD)*SectionsCount);

	for(DWORD CurrentStringNum = 0; CurrentStringNum < FileStringsCount; CurrentStringNum++)
	{
		CurrentStringSize = GetFileStringFromNum(CurrentStringNum, CurrentString, 512);

		if(CurrentString[0] == ';') continue; // It's a comment
		

		if(CurrentString[0] == '[' && CurrentString[CurrentStringSize-1] == ']')	// It's section declaration
		{
			CurrentSectionNum++;
			continue;
		}
		if(IsVariable(CurrentString, CurrentStringSize))
		{
			VariablesCount++;
			SectionVariableCount[CurrentSectionNum]++;
			continue;
		}
	}

	IniData.SectionCount = SectionsCount;
	IniData.Section = new INI_SECTION[SectionsCount];
	memset(IniData.Section, 0x00, sizeof(PINI_SECTION)*SectionsCount);

	for(DWORD i = 0; i < SectionsCount; i++)
	{
		IniData.Section[i].VariablesCount = SectionVariableCount[i];
		IniData.Section[i].Variables = new INI_SECTION_VARIABLE[SectionVariableCount[i]];
		memset(IniData.Section[i].Variables, 0x00, sizeof(INI_SECTION_VARIABLE)*SectionVariableCount[i]);
	}

	delete[] SectionVariableCount;

	CurrentSectionNum = -1;
	CurrentVariableNum = -1;

	for(DWORD CurrentStringNum = 0; CurrentStringNum < FileStringsCount; CurrentStringNum++)
	{
		CurrentStringSize = GetFileStringFromNum(CurrentStringNum, CurrentString, 512);

		if(CurrentString[0] == ';') // It's a comment
		{
			continue;
		}

		if(CurrentString[0] == '[' && CurrentString[CurrentStringSize-1] == ']')
		{
			CurrentSectionNum++;
			CurrentVariableNum = 0;
			memcpy(IniData.Section[CurrentSectionNum].SectionName, &(CurrentString[1]), (CurrentStringSize-2));
			continue;
		}

		if(IsVariable(CurrentString, CurrentStringSize))
		{
			FillVariable(&(IniData.Section[CurrentSectionNum].Variables[CurrentVariableNum]), CurrentString, CurrentStringSize);
			CurrentVariableNum++;
			continue;
		}
	}

	return true;
}

bool INI_FILE::SectionExists(char *SectionName)
{
	for(DWORD i = 0; i < IniData.SectionCount; i++)
	{
		if(memcmp(IniData.Section[i].SectionName, SectionName, strlen(SectionName)) == 0)
		{
			return true;
		}
	}
	return false;
}

bool INI_FILE::GetVariableInSection(char *SectionName, char *VariableName, INI_VAR_STRING *RetVariable)
{
	INI_SECTION *Section = NULL;
	INI_SECTION_VARIABLE *Variable = NULL;

	// Find section
	for(DWORD i = 0; i < IniData.SectionCount; i++)
	{
		if(memcmp(IniData.Section[i].SectionName, SectionName, strlen(SectionName)) == 0)
		{
			Section = &(IniData.Section[i]);
			break;
		}
	}
	if(Section == NULL)	
	{
		SetLastError(318); // This region is not found
		return false;
	}
	// Find variable
	for(DWORD i = 0; i < Section->VariablesCount; i++)
	{
		if(memcmp(Section->Variables[i].VariableName, VariableName, strlen(VariableName)) == 0)
		{
			Variable = &(Section->Variables[i]);
			break;
		}
	}
	if(Variable == NULL)
	{
		SetLastError(1898); // Member of the group is not found
		return false;
	}

	memset(RetVariable, 0x00, sizeof(*RetVariable));
	memcpy(RetVariable->Name, Variable->VariableName, strlen(Variable->VariableName));
	memcpy(RetVariable->Value, Variable->VariableValue, strlen(Variable->VariableValue));
	
	return true;
}

bool INI_FILE::GetVariableInSection(char *SectionName, char *VariableName, INI_VAR_DWORD *RetVariable)
{
	INI_SECTION *Section = NULL;
	INI_SECTION_VARIABLE *Variable = NULL;

	// Find section
	for(DWORD i = 0; i < IniData.SectionCount; i++)
	{
		if(memcmp(IniData.Section[i].SectionName, SectionName, strlen(SectionName)) == 0)
		{
			Section = &(IniData.Section[i]);
			break;
		}
	}
	if(Section == NULL)	
	{
		SetLastError(318); // This region is not found
		return false;
	}
	// Find variable
	for(DWORD i = 0; i < Section->VariablesCount; i++)
	{
		if(memcmp(Section->Variables[i].VariableName, VariableName, strlen(VariableName)) == 0)
		{
			Variable = &(Section->Variables[i]);
			break;
		}
	}
	if(Variable == NULL)
	{
		SetLastError(1898); // Member of the group is not found
		return false;
	}
	
	memset(RetVariable, 0x00, sizeof(*RetVariable));
	memcpy(RetVariable->Name, Variable->VariableName, strlen(Variable->VariableName));

#ifndef _WIN64
	RetVariable->ValueDec = strtol(Variable->VariableValue, NULL, 10);
	RetVariable->ValueHex = strtol(Variable->VariableValue, NULL, 16);
#else
	RetVariable->ValueDec = _strtoi64(Variable->VariableValue, NULL, 10);
	RetVariable->ValueHex = _strtoi64(Variable->VariableValue, NULL, 16);
#endif
	return true;
}

bool INI_FILE::GetVariableInSection(char *SectionName, char *VariableName, INI_VAR_BYTEARRAY *RetVariable)
{
	INI_SECTION *Section = NULL;
	INI_SECTION_VARIABLE *Variable = NULL;

	// Find section
	for(DWORD i = 0; i < IniData.SectionCount; i++)
	{
		if(memcmp(IniData.Section[i].SectionName, SectionName, strlen(SectionName)) == 0)
		{
			Section = &(IniData.Section[i]);
			break;
		}
	}
	if(Section == NULL)	
	{
		SetLastError(318); // This region is not found
		return false;
	}
	// Find variable
	for(DWORD i = 0; i < Section->VariablesCount; i++)
	{
		if(memcmp(Section->Variables[i].VariableName, VariableName, strlen(VariableName)) == 0)
		{
			Variable = &(Section->Variables[i]);
			break;
		}
	}
	if(Variable == NULL)
	{
		SetLastError(1898); // Member of the group is not found
		return false;
	}

	DWORD ValueLen = strlen(Variable->VariableName);
	if((ValueLen % 2) != 0) return false;

	memset(RetVariable, 0x00, sizeof(*RetVariable));
	memcpy(RetVariable->Name, Variable->VariableName, ValueLen);

	char Mask[15] = {};

	for(DWORD i = 0; i <= ValueLen; i++)
	{
		if((i % 2) != 0) continue;
		
		switch(Variable->VariableValue[i])
		{
			case '0': break;
			case '1': RetVariable->Value[(i/2)] += (1 << 4); break;
			case '2': RetVariable->Value[(i/2)] += (2 << 4); break;
			case '3': RetVariable->Value[(i/2)] += (3 << 4); break;
			case '4': RetVariable->Value[(i/2)] += (4 << 4); break;
			case '5': RetVariable->Value[(i/2)] += (5 << 4); break;
			case '6': RetVariable->Value[(i/2)] += (6 << 4); break;
			case '7': RetVariable->Value[(i/2)] += (7 << 4); break;
			case '8': RetVariable->Value[(i/2)] += (8 << 4); break;
			case '9': RetVariable->Value[(i/2)] += (9 << 4); break;
			case 'A': RetVariable->Value[(i/2)] += (10 << 4); break;
			case 'B': RetVariable->Value[(i/2)] += (11 << 4); break;
			case 'C': RetVariable->Value[(i/2)] += (12 << 4); break;
			case 'D': RetVariable->Value[(i/2)] += (13 << 4); break;
			case 'E': RetVariable->Value[(i/2)] += (14 << 4); break;
			case 'F': RetVariable->Value[(i/2)] += (15 << 4); break;
		}	

		switch(Variable->VariableValue[i+1])
		{
			case '0': break;
			case '1': RetVariable->Value[(i/2)] += 1; break;
			case '2': RetVariable->Value[(i/2)] += 2; break;
			case '3': RetVariable->Value[(i/2)] += 3; break;
			case '4': RetVariable->Value[(i/2)] += 4; break;
			case '5': RetVariable->Value[(i/2)] += 5; break;
			case '6': RetVariable->Value[(i/2)] += 6; break;
			case '7': RetVariable->Value[(i/2)] += 7; break;
			case '8': RetVariable->Value[(i/2)] += 8; break;
			case '9': RetVariable->Value[(i/2)] += 9; break;
			case 'A': RetVariable->Value[(i/2)] += 10; break;
			case 'B': RetVariable->Value[(i/2)] += 11; break;
			case 'C': RetVariable->Value[(i/2)] += 12; break;
			case 'D': RetVariable->Value[(i/2)] += 13; break;
			case 'E': RetVariable->Value[(i/2)] += 14; break;
			case 'F': RetVariable->Value[(i/2)] += 15; break;
		}
	}
	RetVariable->ArraySize = ValueLen/2;
	return true;
}

bool INI_FILE::GetVariableInSection(char *SectionName, char *VariableName, INI_VAR_BOOL *RetVariable)
{
	INI_SECTION *Section = NULL;
	INI_SECTION_VARIABLE *Variable = NULL;

	// Find section
	for(DWORD i = 0; i < IniData.SectionCount; i++)
	{
		if(memcmp(IniData.Section[i].SectionName, SectionName, strlen(SectionName)) == 0)
		{
			Section = &(IniData.Section[i]);
			break;
		}
	}
	if(Section == NULL)	
	{
		SetLastError(318); // This region is not found
		return false;
	}
	// Find variable
	for(DWORD i = 0; i < Section->VariablesCount; i++)
	{
		if(memcmp(Section->Variables[i].VariableName, VariableName, strlen(VariableName)) == 0)
		{
			Variable = &(Section->Variables[i]);
			break;
		}
	}
	if(Variable == NULL)
	{
		SetLastError(1898); // Member of the group is not found
		return false;
	}
	
	memset(RetVariable, 0x00, sizeof(*RetVariable));
	RetVariable->Value = (bool)strtol(Variable->VariableValue, NULL, 10);
	return true;
}
