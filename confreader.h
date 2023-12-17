/*
The MIT License (MIT)

Copyright (c) 2023 Dmitriy Gribanov https://github.com/dmitgrib

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next paragraph) shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


/*
confreader.h	ver 15.12.2023

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

#ifndef __CONFREADER_H_
#define __CONFREADER_H_

#define CONFREADER_OK				0
#define CONFREADER_ERROR			-1

#define CONFREADER_EREADFILE		1
#define CONFREADER_EPARSINGFILE		2
#define CONFREADER_ENOSECT			3
#define CONFREADER_ENOPARAM			4
#define CONFREADER_EINVVAL			5
#define CONFREADER_EBUSY			6
#define CONFREADER_ENOMEM			7

typedef struct confreader_param {
	char *key;
	char *value;
} ConfreaderParam;

typedef struct confreader_section {
	int size;
	char *name;
	ConfreaderParam *params;
} ConfreaderSection;


char *confreader_fileBuf = NULL;

int *confreader_lines;
int confreader_lineCount;

ConfreaderParam *confreader_params;
int confreader_paramCount;

int confreaderErrorNum;
int confreaderErrorLineNum;
ConfreaderSection *confreaderSects;
int confreaderSectCount;

void confreaderInit(){
	confreaderSectCount = 0;
	confreaderSects = NULL;
	confreader_params = NULL;
	confreader_lines = NULL;
	confreader_fileBuf = NULL;
	confreaderErrorNum = 0;
	confreaderErrorLineNum = 0;
}

void confreaderClear(){
	confreaderSectCount = 0;
	if(confreaderSects){
		free(confreaderSects);
		confreaderSects = NULL;
	}
	if(confreader_params){
		free(confreader_params);
		confreader_params = NULL;
	}
	if(confreader_lines){
		free(confreader_lines);
		confreader_lines = NULL;
	}
	if(confreader_fileBuf){
		free(confreader_fileBuf);
		confreader_fileBuf = NULL;
	}
}

int confreaderParseFile(const char *filename){
	int fd, i;
	int lineIdx, sectIdx, paramIdx;
	ssize_t fileBufSize;
	struct stat file_status;
	
	confreaderErrorLineNum = 0;

	if(confreader_fileBuf){
		confreaderErrorNum = CONFREADER_EBUSY;
		return CONFREADER_ERROR;
	}
	
	confreaderInit();
	
	// Open file and read text content.
	fd = open(filename, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
	if(fd == -1){
		confreaderErrorNum = CONFREADER_EREADFILE;
		return CONFREADER_ERROR;
	}
	
	if(fstat(fd, &file_status) != 0){
		confreaderErrorNum = CONFREADER_EREADFILE;
		return CONFREADER_ERROR;
	}
	
	fileBufSize = file_status.st_size;
	if(fileBufSize == 0){
		close(fd);
		confreaderErrorNum = CONFREADER_OK;
		return CONFREADER_OK;		// File is empty.
	}
	
	confreader_fileBuf = (char *)malloc(fileBufSize + 1);		// One byte more.
	if(confreader_fileBuf == NULL){
		confreaderErrorNum = CONFREADER_ENOMEM;
		return CONFREADER_ERROR;
	}
	
	if(read(fd, confreader_fileBuf, fileBufSize) != fileBufSize){
		free(confreader_fileBuf);
		confreader_fileBuf = NULL;
		confreaderErrorNum = CONFREADER_EREADFILE;
		return CONFREADER_ERROR;
	}
	close(fd);
	
	// Let's put 0x0A in the last byte, since the last line can be without a line feed character.
	confreader_fileBuf[fileBufSize] = 0x0A;
	fileBufSize++;
	
	// Let's count how many lines are in the file.
	confreader_lineCount = 0;
	for(i=0; i<fileBufSize; i++){
		if(confreader_fileBuf[i] == 0x0A) confreader_lineCount++;
	}

	// Let's allocate memory for the array of pointers to strings.
	confreader_lines = (int *)malloc(confreader_lineCount * sizeof(int));
	if(confreader_lines == NULL){
		confreaderClear();
		confreaderErrorNum = CONFREADER_ENOMEM;
		return CONFREADER_ERROR;
	}

	// Let's count how many sections and how many parameters are in the file.
	confreader_paramCount = 0;
	confreaderSectCount = 1;			// Section with index 0 for parameters without section.
	lineIdx = 0;
	for(i=0; i<fileBufSize; i++){
		// Skip the whitespace characters at the beginning of the string.
		for(; i<fileBufSize; i++){
			if(confreader_fileBuf[i] != ' ' && confreader_fileBuf[i] != 0x09) break;
		}
		// Remember the index of the beginning of the string.
		confreader_lines[lineIdx++] = i;

		// Check the beginning of the section.
		if(confreader_fileBuf[i] == '['){
			confreaderSectCount++;
		}else
		// Check the beginning of the comment or parameter.
		if(confreader_fileBuf[i] != '#' && confreader_fileBuf[i] != ';' && confreader_fileBuf[i] != 0x0A && confreader_fileBuf[i] != 0x0D){
			confreader_paramCount++;
		}

		for(; i<fileBufSize; i++){
			if(confreader_fileBuf[i] == 0x0D){
				confreader_fileBuf[i++] = 0;
			
				if(confreader_fileBuf[i] != 0x0A){	// After 0x0D, 0x0A must necessarily follow.
					confreaderClear();
					confreaderErrorLineNum = lineIdx;
					confreaderErrorNum = CONFREADER_EPARSINGFILE;
					return CONFREADER_ERROR;
				}
				confreader_fileBuf[i] = 0;
				break;
			//newLine = true;
			}else
			if(confreader_fileBuf[i] == 0x0A){
				confreader_fileBuf[i] = 0;
				break;
			}
		}
	}

	// Allocate memory for an array of pointers to lines with parameters.
	confreader_params = (ConfreaderParam *)malloc(confreader_paramCount * sizeof(ConfreaderParam));
	if(confreader_params == NULL){
		confreaderClear();
		confreaderErrorNum = CONFREADER_ENOMEM;
		return CONFREADER_ERROR;
	}

	// Allocate memory for an array of pointers to sections.
	confreaderSects = (ConfreaderSection *)malloc(confreaderSectCount * sizeof(ConfreaderSection));
	if(confreaderSects == NULL){
		confreaderClear();
		confreaderErrorNum = CONFREADER_ENOMEM;
		return CONFREADER_ERROR;
	}

	// Link all sections and parameters.
	sectIdx = 0;
	confreaderSects[sectIdx].name = NULL;
	confreaderSects[sectIdx].size = 0;
	confreaderSects[sectIdx].params = NULL;
	
	paramIdx = 0;
	for(lineIdx=0; (lineIdx<confreader_lineCount) && (paramIdx < confreader_paramCount); lineIdx++){
		i = confreader_lines[lineIdx];

		if(confreader_fileBuf[i] == '['){			// Found a new section.
			sectIdx++;
			confreaderSects[sectIdx].name = &confreader_fileBuf[++i];
			confreaderSects[sectIdx].size = 0;
			confreaderSects[sectIdx].params = NULL;
			// Let's find the end of the section name.
			for(; i<fileBufSize; i++){
				if(confreader_fileBuf[i] == ']'){
					confreader_fileBuf[i++] = 0;
					break;
				}
				if(confreader_fileBuf[i] == 0){		// Couldn't find the closing parenthesis.
					confreaderClear();
					confreaderErrorLineNum = lineIdx + 1;
					confreaderErrorNum = CONFREADER_EPARSINGFILE;
					return CONFREADER_ERROR;
				}
			}
			
			// If there are whitespace characters in the line from the current position, we skip these characters.
			for(; i<fileBufSize; i++){
				if(confreader_fileBuf[i] != ' ' && confreader_fileBuf[i] != 0x09) break;
			}
			
			// If there is something at the end of the line but it's not a comment, it's an error.
			if(confreader_fileBuf[i] != 0 && confreader_fileBuf[i] != '#' && confreader_fileBuf[i] != ';'){
				confreaderClear();
				confreaderErrorLineNum = lineIdx + 1;
				confreaderErrorNum = CONFREADER_EPARSINGFILE;
				return CONFREADER_ERROR;
			}
		}else
		
		if(confreader_fileBuf[i] != '#' && confreader_fileBuf[i] != ';' && confreader_fileBuf[i] != 0){	// Found a line with a parameter.
			confreader_params[paramIdx].key = &confreader_fileBuf[i];
			
			// If the current section is empty, the detected line will be the first line.
			if(confreaderSects[sectIdx].params == NULL){
				confreaderSects[sectIdx].params = &confreader_params[paramIdx];
			}
			
			// Let's find the end of the parameter name.
			for(; i<fileBufSize; i++){
				if(confreader_fileBuf[i] == 0){		// Unexpected end of line after the parameter name.
					confreaderClear();
					confreaderErrorLineNum = lineIdx + 1;
					confreaderErrorNum = CONFREADER_EPARSINGFILE;
					return CONFREADER_ERROR;
				}
					
				if(confreader_fileBuf[i] == '=' || confreader_fileBuf[i] == ' ' || confreader_fileBuf[i] == 0x09) break;
			}
			confreader_fileBuf[i++] = 0;

			// Let's skip the whitespace characters and get the beginning of the parameter value.
			for(; i<fileBufSize; i++){
				if(confreader_fileBuf[i] != '=' && confreader_fileBuf[i] != ' ' && confreader_fileBuf[i] != 0x09) break;
			}
			if(confreader_fileBuf[i] == 0 || confreader_fileBuf[i] == '#' || confreader_fileBuf[i] == ';'){
				// There is no value for the parameter.
				confreaderClear();
				confreaderErrorLineNum = lineIdx + 1;
				confreaderErrorNum = CONFREADER_EPARSINGFILE;
				return CONFREADER_ERROR;
			}

			confreader_params[paramIdx].value = &confreader_fileBuf[i];
			// Let's find the end of the parameter value.
			for(; i<fileBufSize; i++){
				//if(confreader_fileBuf[i] == 0 || confreader_fileBuf[i] == '#' || confreader_fileBuf[i] == ';') break;
				if(confreader_fileBuf[i] == 0) break;
				if(confreader_fileBuf[i] == '#' || confreader_fileBuf[i] == ';'){
					if(confreader_fileBuf[i-1] != ' ' && confreader_fileBuf[i-1] != 0x09){
						// Error. The comment must be separated by a space character from the value.
						confreaderClear();
						confreaderErrorLineNum = lineIdx + 1;
						confreaderErrorNum = CONFREADER_EPARSINGFILE;
						return CONFREADER_ERROR;
					}
					break;
				}
			}
			
			// We clear the whitespace characters at the end of the value and get the end of the parameter value
			for(--i; i>=0; i--){
				if(confreader_fileBuf[i] != ' ' && confreader_fileBuf[i] != 0x09) break;
			}
			// and then put 0 after the end of the parameter value.
			confreader_fileBuf[++i] = 0;
			
			confreaderSects[sectIdx].size++;
			paramIdx++;
		}
	}

	free(confreader_lines);
	confreader_lines = NULL;
	confreaderErrorNum = CONFREADER_OK;
	return CONFREADER_OK;
}

char * confreaderFind(const char *key, const char *section){
	int j;

	if(confreader_fileBuf){
		if(section == NULL){
			for(j=0; j<confreaderSects[0].size; j++){
				if(strcasecmp(key, confreaderSects[0].params[j].key) == 0){
					confreaderErrorNum = CONFREADER_OK;
					return confreaderSects[0].params[j].value;
				}
			}
		}else{
			for(int i=1; i<confreaderSectCount; i++){
				if(strcasecmp(section, confreaderSects[i].name) == 0){
					for(j=0; j<confreaderSects[i].size; j++){
						if(strcasecmp(key, confreaderSects[i].params[j].key) == 0){
							confreaderErrorNum = CONFREADER_OK;
							return confreaderSects[i].params[j].value;
						}
					}
					break;
				}
			}
		}
	}
	confreaderErrorNum = CONFREADER_ENOPARAM;
	return NULL;
}

int confreaderHasSection(const char *section){
	int i;

	for(i=1; i<confreaderSectCount; i++){
		if(strcasecmp(section, confreaderSects[i].name) == 0){
			return 1;
		}
	}
	confreaderErrorNum = CONFREADER_ENOSECT;
	return 0;
}

int confreaderHas(const char *key, const char *section){
	if(confreaderFind(key, section) != NULL){
		return 1;
	}
	return 0;
}

char confreaderGetChar(const char *key, const char *section, char defaultValue){
	char *val;
	
	if((val = confreaderFind(key, section)) != NULL){
		return val[0];
	}
	return defaultValue;
}

char * confreaderGetString(const char *key, const char *section, const char *defaultValue){
	char *val;
	
	if((val = confreaderFind(key, section)) != NULL){
		return val;
	}
	return (char *)defaultValue;
}

int confreaderGetInt(const char *key, const char *section, int defaultValue){
	char *val;
	int k;
	
	if((val = confreaderFind(key, section)) != NULL){
		// We found the parameter
		if((val[0] < '0' || val[0] > '9') && val[0] != '-'){
			confreaderErrorNum = CONFREADER_EINVVAL;
			return defaultValue;
		}
		for(k=1; val[k]!=0; k++){
			if(val[k] < '0' || val[k] > '9'){
				confreaderErrorNum = CONFREADER_EINVVAL;
				return defaultValue;
			}
		}

		return strtol(val, NULL, 10);
	}
	return defaultValue;
}

double confreaderGetDouble(const char *key, const char *section, double defaultValue){
	char *val;
	int k;
	
	if((val = confreaderFind(key, section)) != NULL){
		// We found the parameter
		if((val[0] < '0' || val[0] > '9') && val[0] != '-'){
			confreaderErrorNum = CONFREADER_EINVVAL;
			return defaultValue;
		}
		for(k=1; val[k]!=0; k++){
			if((val[k] < '0' || val[k] > '9') && val[k] != '.'){
				confreaderErrorNum = CONFREADER_EINVVAL;
				return defaultValue;
			}
		}

		return strtod(val, NULL);
	}
	return defaultValue;
}

int confreaderGetBool(const char *key, const char *section, int defaultValue){
	char *val;
	int ret;
	
	if((val = confreaderFind(key, section)) != NULL){
		// We found the parameter
		if(strcasecmp(val, "yes") == 0 || strcasecmp(val, "true") == 0 || (val[0] == '1' && val[1] == 0)){
			return 1;
		}
		if(strcasecmp(val, "no") == 0 || strcasecmp(val, "false") == 0 || (val[0] == '0' && val[1] == 0)){
			return 0;
		}
		
		confreaderErrorNum = CONFREADER_EINVVAL;
		return defaultValue;
	}
	return defaultValue;
}

#endif	// __CONFREADER_H_
