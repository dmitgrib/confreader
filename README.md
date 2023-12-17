# Сonfreader

Confreader provides functionality to parse text config file with `key=value` pairs and to retrieve parameters values. Confreader supports sections. Configuration parameters can be located within sections or outside sections.
In addition to the usual .conf or .ini file format
```
# Service configuration here
 
Port = 2222
log_error = Yes

[dbaccess]
; Networking access to the database
server = 192.168.0.60
port = 3333
```
confreader supports comments at the end of lines, after the parameter value. The comment must be separated by at least one space character.

## 1. Description

This is a really easy to use library for C and C++ projects. The library is implemented as a header-only library, so it is very easy to add it to a project. The functions and methods of the class are not inline, which will not cause excessive increase code with many calls. The implementation uses only standard functions for memory allocation, file reading, string comparison and value conversion, so it doesn't add significant size to the executable. Note: Unicode will be read, but not processed properly.

Initially, the .conf file is completely loaded into memory. The parser parses lines and forms reference structures to strings directly in this memory block. Type conversion is not done beforehand, because it is not known beforehand in what form the calling code will receive the values of the parameters. The conversion takes place when the calling code requests the value of the parameter.

Finding the requested parameter is done by looping through the array and comparing strings. I assume that the reading of parameters is done by the application once at startup, the number of parameters in the .conf file is usually not large, so there is no need to be very fast when retrieving parameters.

As a result of this approach, the library code is simple, small, and does not require huge containers and functions from STL.

In one of my C++ projects I just needed to get the values of parameters from a regular .conf file. The format is old and quite common. I searched for ready-made solutions and didn't find anything suitable. This led to the development of a confrider library.

Unlike other libraries, confreader:

- Supports sections. Some other libraries did not know how to do this.
- The set of functions (methods) is minimal, only parsing and getting parameter values. Confreader does not have write to .conf file, as most projects only need to read the .conf file.
- Does not use containers and functions of STL, so you don't need to add all of this into your project and the executable file doesn't increases much.
- The function call interface is concise and therefore easy to understand without long documentation study.
- It does not require separate building. You won't have to figure out what you need to install in the system to build the project.

## 2. Usage

Assuming the confreader header files are in your search path, simply add the following to the source file

for C
```C
#include <confreader.h>
```
for C++
```cpp
#include <confreader.hpp>
```

#### The sequence of actions is demonstrated on C++.

```cpp
// Creating an object and loading a file
Confreader *cfgFile = new Confreader();
cfgFile->parseFile("testconf.conf");

// Checking the presence of a section
cfgFile->hasSection("Sect");

// Checking the presence of a parameter outside the section
cfgFile->has("IntParameter");

// Checking the presence of a parameter in a section
cfgFile->has("IntParameter", "Sect");

// Getting parameters outside the section
char charValue = getChar("CharParameter");
char *strValue = cfgFile->getString("StringParameter");
int intValue = cfgFile->getInt("IntParameter");
double doubleValue = cfgFile->getDouble("DoubleParameter");
bool boolValue = cfgFile->getBool("BoolParameter");

// Getting parameters from a section
charValue = getChar("CharParameter", "Sect");
strValue = cfgFile->getString("StringParameter", "Sect");
intValue = cfgFile->getInt("IntParameter", "Sect");
doubleValue = cfgFile->getDouble("DoubleParameter", "Sect");
boolValue = cfgFile->getBool("BoolParameter", "Sect");

// Getting parameters from a section with default values
charValue = getChar("CharParameter", "Sect", 'A');
strValue = cfgFile->getString("StringParameter", "Sect", "default string");
intValue = cfgFile->getInt("IntParameter", "Sect", 1000);
doubleValue = cfgFile->getDouble("DoubleParameter", "Sect", 3.14);
boolValue = cfgFile->getBool("BoolParameter", "Sect", false);

// Cleaning an object without deleting it
cfgFile->clear();

// Delete object
delete cfgFile;
```

#### Return values of the function (method) `parseFile`

parseFile returns CONFREADER_OK or CONFREADER_ERROR.
In this case errorNum = CONFREADER_OK.

In case of an error, errorNum takes one of the following values:
CONFREADER_EREADFILE
CONFREADER_ENOMEM
CONFREADER_EPARSINGFILE
CONFREADER_EBUSY

If there is a syntax error in any line of the configuration file, errorLineNum will contain the line number of that line.

#### Return values of the functions (methods) `get...`

Functions (methods) to get the values of parameters return the corresponding values if they are found.
In this case errorNum = CONFREADER_OK.

If the parameter is not found, the default value is returned.
In this case errorNum = CONFREADER_ENOPARAM.

If the value cannot be converted errorNum = CONFREADER_EINVVAL.

#### Get all parameters from the configuration file
Loop through an array of sects and the array of params within each section.

```cpp
// sects[0] contains parameters outside the section. sects[0] does not have a name.
if(cfgFile->sects[0].size > 0){
	for(j=0; j<cfgFile->sects[0].size; j++){
		printf("%s = %s\n", cfgFile->sects[0].params[j].key, cfgFile->sects[0].params[j].value);
	}
	printf("\n");
}

for(i=1; i<cfgFile->sectCount; i++){
	printf("[%s]\n", cfgFile->sects[i].name);
	
	for(j=0; j<cfgFile->sects[i].size; j++){
		printf("%s = %s\n", cfgFile->sects[i].params[j].key, cfgFile->sects[i].params[j].value);
	}
	printf("\n");
}
```

#### Functions for C
They have names corresponding to the methods of the class with the added confreader prefix.

`
confreaderParseFile, confreaderHasSection, confreaderHas, confreaderClear, confreaderGetChar, confreaderGetString, confreaderGetInt, confreaderGetDouble, confreaderGetBool.
`

## 3. Conclusion

Let this library remain simple and easy to use.

