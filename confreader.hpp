/*
The MIT License (MIT)

Copyright (c) 2023 Dmitriy Gribanov https://github.com/dmitgrib

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next paragraph) shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


/*
confreader.hpp	ver 15.12.2023

Confreader provides functionality to parse text config file with `key=value` pairs and to retrieve parameters values. Confreader supports sections. Configuration parameters can be located within sections or without sections.
In contrast to the usual .conf or .ini file format

# first comment
ParamWithoutSection = yes
[SectName]
; second comment
ParamWithSection = 123456

confreader supports comments at the end of lines, after the parameter value. The comment must be separated by at least one space character.

Usage:
1 - Read content into mem and then parse it.
2 - Getting values by names.
3 - Free mem, that is end.

*/

#ifndef __CONFREADER_HPP_
#define __CONFREADER_HPP_

#define CONFREADER_OK				0
#define CONFREADER_ERROR			-1

#define CONFREADER_EREADFILE		1
#define CONFREADER_EPARSINGFILE		2
#define CONFREADER_ENOSECT			3
#define CONFREADER_ENOPARAM			4
#define CONFREADER_EINVVAL			5
#define CONFREADER_EBUSY			6
#define CONFREADER_ENOMEM			7

class Confreader {
private:
	typedef struct param {
		char *key;
		char *value;
	} Param;
	
	typedef struct section {
		int size;
		char *name;
		Param *params;
	} Section;

	char *_fileBuf;
	
	int *_lines;
	int _lineCount;

	Param *_params;
	int _paramCount;

public:
	int errorNum;
	int errorLineNum;
	Section *sects;
	int sectCount;
	
	Confreader(){
		init();
	}
	Confreader(char *filename){
		init();
		parseFile(filename);
	}
	~Confreader(){
		clear();
	}

	void init(){
		sectCount = 0;
		sects = nullptr;
		_params = nullptr;
		_lines = nullptr;
		_fileBuf = nullptr;
		errorNum = 0;
		errorLineNum = 0;
	}

	void clear(){
		sectCount = 0;
		if(sects){
			free(sects);
			sects = nullptr;
		}
		if(_params){
			free(_params);
			_params = nullptr;
		}
		if(_lines){
			free(_lines);
			_lines = nullptr;
		}
		if(_fileBuf){
			free(_fileBuf);
			_fileBuf = nullptr;
		}
	}

	int parseFile(const char *filename){
		int fd, i;
		int lineIdx, sectIdx, paramIdx;
		ssize_t fileBufSize;
		struct stat file_status;
		
		errorLineNum = 0;
		
		if(_fileBuf){
			errorNum = CONFREADER_EBUSY;
			return CONFREADER_ERROR;
		}
		
		// Open file and read text content.
		fd = open(filename, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
		if(fd == -1){
			errorNum = CONFREADER_EREADFILE;
			return CONFREADER_ERROR;
		}
		
		if(fstat(fd, &file_status) != 0){
			errorNum = CONFREADER_EREADFILE;
			return CONFREADER_ERROR;
		}
		
		fileBufSize = file_status.st_size;
		if(fileBufSize == 0){
			close(fd);
			errorNum = CONFREADER_OK;
			return CONFREADER_OK;		// File is empty.
		}
		
		_fileBuf = (char *)malloc(fileBufSize + 1);		// One byte more.
		if(_fileBuf == nullptr){
			errorNum = CONFREADER_ENOMEM;
			return CONFREADER_ERROR;
		}
		
		if(read(fd, _fileBuf, fileBufSize) != fileBufSize){
			free(_fileBuf);
			_fileBuf = nullptr;
			errorNum = CONFREADER_EREADFILE;
			return CONFREADER_ERROR;
		}
		close(fd);
		
		// Let's put 0x0A in the last byte, since the last line can be without a line feed character.
		_fileBuf[fileBufSize] = 0x0A;
		fileBufSize++;
		
		// Let's count how many lines are in the file.
		_lineCount = 0;
		for(i=0; i<fileBufSize; i++){
			if(_fileBuf[i] == 0x0A) _lineCount++;
		}

		// Let's allocate memory for the array of pointers to strings.
		_lines = (int *)malloc(_lineCount * sizeof(int));
		if(_lines == nullptr){
			clear();
			errorNum = CONFREADER_ENOMEM;
			return CONFREADER_ERROR;
		}

		// Let's count how many sections and how many parameters are in the file.
		_paramCount = 0;
		sectCount = 1;			// Section with index 0 for parameters without section.
		lineIdx = 0;
		for(i=0; i<fileBufSize; i++){
			// Skip the whitespace characters at the beginning of the string.
			for(; i<fileBufSize; i++){
				if(_fileBuf[i] != ' ' && _fileBuf[i] != 0x09) break;
			}
			// Remember the index of the beginning of the string.
			_lines[lineIdx++] = i;

			// Check the beginning of the section.
			if(_fileBuf[i] == '['){
				sectCount++;
			}else
			// Check the beginning of the comment or parameter.
			if(_fileBuf[i] != '#' && _fileBuf[i] != ';' && _fileBuf[i] != 0x0A && _fileBuf[i] != 0x0D){
				_paramCount++;
			}

			for(; i<fileBufSize; i++){
				if(_fileBuf[i] == 0x0D){
					_fileBuf[i++] = 0;
				
					if(_fileBuf[i] != 0x0A){	// After 0x0D, 0x0A must necessarily follow.
						clear();
						errorLineNum = lineIdx;
						errorNum = CONFREADER_EPARSINGFILE;
						return CONFREADER_ERROR;
					}
					_fileBuf[i] = 0;
					break;
				//newLine = true;
				}else
				if(_fileBuf[i] == 0x0A){
					_fileBuf[i] = 0;
					break;
				}
			}
		}

		// Allocate memory for an array of pointers to lines with parameters.
		_params = (Param *)malloc(_paramCount * sizeof(Param));
		if(_params == nullptr){
			clear();
			errorNum = CONFREADER_ENOMEM;
			return CONFREADER_ERROR;
		}

		// Allocate memory for an array of pointers to sections.
		sects = (Section *)malloc(sectCount * sizeof(Section));
		if(sects == nullptr){
			clear();
			errorNum = CONFREADER_ENOMEM;
			return CONFREADER_ERROR;
		}

		// Link all sections and parameters.
		sectIdx = 0;
		sects[sectIdx].name = nullptr;
		sects[sectIdx].size = 0;
		sects[sectIdx].params = nullptr;
		
		paramIdx = 0;
		for(lineIdx=0; (lineIdx<_lineCount) && (paramIdx < _paramCount); lineIdx++){
			i = _lines[lineIdx];

			if(_fileBuf[i] == '['){			// Found a new section.
				sectIdx++;
				sects[sectIdx].name = &_fileBuf[++i];
				sects[sectIdx].size = 0;
				sects[sectIdx].params = nullptr;
				// Let's find the end of the section name.
				for(; i<fileBufSize; i++){
					if(_fileBuf[i] == ']'){
						_fileBuf[i++] = 0;
						break;
					}
					if(_fileBuf[i] == 0){		// Couldn't find the closing parenthesis.
						clear();
						errorLineNum = lineIdx + 1;
						errorNum = CONFREADER_EPARSINGFILE;
						return CONFREADER_ERROR;
					}
				}
				
				// If there are whitespace characters in the line from the current position, we skip these characters.
				for(; i<fileBufSize; i++){
					if(_fileBuf[i] != ' ' && _fileBuf[i] != 0x09) break;
				}
				
				// If there is something at the end of the line but it's not a comment, it's an error.
				if(_fileBuf[i] != 0 && _fileBuf[i] != '#' && _fileBuf[i] != ';'){
					clear();
					errorLineNum = lineIdx + 1;
					errorNum = CONFREADER_EPARSINGFILE;
					return CONFREADER_ERROR;
				}
			}else
			
			if(_fileBuf[i] != '#' && _fileBuf[i] != ';' && _fileBuf[i] != 0){	// Found a line with a parameter.
				_params[paramIdx].key = &_fileBuf[i];
				
				// If the current section is empty, the detected line will be the first line.
				if(sects[sectIdx].params == nullptr){
					sects[sectIdx].params = &_params[paramIdx];
				}
				
				// Let's find the end of the parameter name.
				for(; i<fileBufSize; i++){
					if(_fileBuf[i] == 0){		// Unexpected end of line after the parameter name.
						clear();
						errorLineNum = lineIdx + 1;
						errorNum = CONFREADER_EPARSINGFILE;
						return CONFREADER_ERROR;
					}
						
					if(_fileBuf[i] == '=' || _fileBuf[i] == ' ' || _fileBuf[i] == 0x09) break;
				}
				_fileBuf[i++] = 0;

				// Let's skip the whitespace characters and get the beginning of the parameter value.
				for(; i<fileBufSize; i++){
					if(_fileBuf[i] != '=' && _fileBuf[i] != ' ' && _fileBuf[i] != 0x09) break;
				}
				if(_fileBuf[i] == 0 || _fileBuf[i] == '#' || _fileBuf[i] == ';'){
					// There is no value for the parameter.
					clear();
					errorLineNum = lineIdx + 1;
					errorNum = CONFREADER_EPARSINGFILE;
					return CONFREADER_ERROR;
				}

				_params[paramIdx].value = &_fileBuf[i];
				// Let's find the end of the parameter value.
				for(; i<fileBufSize; i++){
					//if(_fileBuf[i] == 0 || _fileBuf[i] == '#' || _fileBuf[i] == ';') break;
					if(_fileBuf[i] == 0) break;
					if(_fileBuf[i] == '#' || _fileBuf[i] == ';'){
						if(_fileBuf[i-1] != ' ' && _fileBuf[i-1] != 0x09){
							// Error. The comment must be separated by a space character from the value.
							clear();
							errorLineNum = lineIdx + 1;
							errorNum = CONFREADER_EPARSINGFILE;
							return CONFREADER_ERROR;
						}
						break;
					}
				}
				
				// We clear the whitespace characters at the end of the value and get the end of the parameter value
				for(--i; i>=0; i--){
					if(_fileBuf[i] != ' ' && _fileBuf[i] != 0x09) break;
				}
				// and then put 0 after the end of the parameter value.
				_fileBuf[++i] = 0;
				
				sects[sectIdx].size++;
				paramIdx++;
			}
		}

		free(_lines);
		_lines = nullptr;
		errorNum = CONFREADER_OK;
		return CONFREADER_OK;
	}
	
	char * find(const char *key, const char *section = nullptr){
		int j;

		if(_fileBuf){
			if(section == nullptr){
				for(j=0; j<sects[0].size; j++){
					if(strcasecmp(key, sects[0].params[j].key) == 0){
						errorNum = CONFREADER_OK;
						return sects[0].params[j].value;
					}
				}
			}else{
				for(int i=1; i<sectCount; i++){
					if(strcasecmp(section, sects[i].name) == 0){
						for(j=0; j<sects[i].size; j++){
							if(strcasecmp(key, sects[i].params[j].key) == 0){
								errorNum = CONFREADER_OK;
								return sects[i].params[j].value;
							}
						}
						break;
					}
				}
			}
		}
		errorNum = CONFREADER_ENOPARAM;
		return nullptr;
	}
	
	bool hasSection(const char *section){
		int i;

		for(i=1; i<sectCount; i++){
			if(strcasecmp(section, sects[i].name) == 0){
				return true;
			}
		}
		errorNum = CONFREADER_ENOSECT;
		return false;
	}
	
	bool has(const char *key, const char *section = nullptr){
		if(find(key, section) != nullptr){
			return true;
		}
		return false;
	}

	char getChar(const char *key, const char *section = nullptr, char defaultValue = 0){
		char *val;
		
		if((val = find(key, section)) != nullptr){
			return val[0];
		}
		return defaultValue;
	}
	
	char * getString(const char *key, const char *section = nullptr, const char *defaultValue = nullptr){
		char *val;
		
		if((val = find(key, section)) != nullptr){
			return val;
		}
		return (char *)defaultValue;
	}
	
	int getInt(const char *key, const char *section = nullptr, int defaultValue = 0){
		char *val;
		int k;
		
		if((val = find(key, section)) != nullptr){
			// We found the parameter
			if((val[0] < '0' || val[0] > '9') && val[0] != '-'){
				errorNum = CONFREADER_EINVVAL;
				return defaultValue;
			}
			for(k=1; val[k]!=0; k++){
				if(val[k] < '0' || val[k] > '9'){
					errorNum = CONFREADER_EINVVAL;
					return defaultValue;
				}
			}

			return strtol(val, NULL, 10);
		}
		return defaultValue;
	}
	
	double getDouble(const char *key, const char *section = nullptr, double defaultValue = 0.0){
		char *val;
		int k;
		
		if((val = find(key, section)) != nullptr){
			// We found the parameter
			if((val[0] < '0' || val[0] > '9') && val[0] != '-'){
				errorNum = CONFREADER_EINVVAL;
				return defaultValue;
			}
			for(k=1; val[k]!=0; k++){
				if((val[k] < '0' || val[k] > '9') && val[k] != '.'){
					errorNum = CONFREADER_EINVVAL;
					return defaultValue;
				}
			}

			return strtod(val, NULL);
		}
		return defaultValue;
	}
	
	bool getBool(const char *key, const char *section = nullptr, bool defaultValue = false){
		char *val;
		int ret;
		
		if((val = find(key, section)) != nullptr){
			// We found the parameter
			if(strcasecmp(val, "yes") == 0 || strcasecmp(val, "true") == 0 || (val[0] == '1' && val[1] == 0)){
				return true;
			}
			if(strcasecmp(val, "no") == 0 || strcasecmp(val, "false") == 0 || (val[0] == '0' && val[1] == 0)){
				return false;
			}
			
			errorNum = CONFREADER_EINVVAL;
			return defaultValue;
		}
		return defaultValue;
	}
	
};

#endif	// __CONFREADER_HPP_
