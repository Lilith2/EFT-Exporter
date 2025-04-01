//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "menuCmdID.h"
#include <string>
#include <regex>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <set>
#include <windows.h>
#include <tchar.h>
#include "Scintilla.h"

// Define Scintilla messages if not already defined
#ifndef SCI_GETLENGTH
#define SCI_GETLENGTH 2006
#endif

#ifndef SCI_GETTEXTRANGE
#define SCI_GETTEXTRANGE 2162
#endif

#ifndef SCI_GETSELTEXT
#define SCI_GETSELTEXT 2161
#endif

#ifndef SCI_GETSELECTIONSTART
#define SCI_GETSELECTIONSTART 2143
#endif

#ifndef SCI_GETSELECTIONEND
#define SCI_GETSELECTIONEND 2145
#endif

#ifndef SCI_REPLACESEL
#define SCI_REPLACESEL 2170
#endif

// Define language type for C#
#ifndef L_CS
#define L_CS 3
#endif

// Define Scintilla types if not already defined
struct CharacterRange {
    long cpMin;
    long cpMax;
};

struct TextRange {
    CharacterRange chrg;
    char* lpstrText;
};

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE /*hModule*/)
{
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
bool commandMenuInit()
{
    ShortcutKey *sk = new ShortcutKey;
    sk->_isAlt = true;
    sk->_isCtrl = true;
    sk->_isShift = true;
    sk->_key = 'E';

    TCHAR* exportToSDK = _tcsdup(TEXT("Export Selection to Master SDK"));
    TCHAR* exportAsFile = _tcsdup(TEXT("Export Selection as Individual File"));
    TCHAR* exportEntireFile = _tcsdup(TEXT("Export Entire File to SDK"));

    setCommand(0, exportToSDK, exportToMasterSDK, NULL, false);
    setCommand(1, exportAsFile, exportAsIndividualFile, NULL, false);
    setCommand(2, exportEntireFile, exportEntireFileToSDK, NULL, false);

    return true;
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

// Helper function to show error messages
void showError(const TCHAR* message) {
    ::MessageBox(nppData._nppHandle, message, TEXT("SDK Formatter Error"), MB_OK | MB_ICONERROR);
}

// Helper function to get the current selection
std::string getCurrentSelection() {
    try {
        HWND curScintilla;
        int which = -1;
        ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
        if (which == -1) {
            showError(TEXT("Failed to get current editor window."));
            return "";
        }
        curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

        // Get selection start and end positions
        size_t selStart = ::SendMessage(curScintilla, SCI_GETSELECTIONSTART, 0, 0);
        size_t selEnd = ::SendMessage(curScintilla, SCI_GETSELECTIONEND, 0, 0);
        if (selStart == selEnd) return "";

        // Get the selected text
        size_t textLength = selEnd - selStart;
        if (textLength == 0) { // Only check for empty selection
            showError(TEXT("Invalid selection length."));
            return "";
        }

        char* buffer = nullptr;
        try {
            buffer = new char[textLength + 1];
            ::SendMessage(curScintilla, SCI_GETSELTEXT, 0, (LPARAM)buffer);
            std::string selectedText(buffer);
            delete[] buffer;
            return selectedText;
        }
        catch (...) {
            delete[] buffer;
            showError(TEXT("Failed to allocate memory for text selection."));
            return "";
        }
    }
    catch (...) {
        showError(TEXT("Unexpected error while getting text selection."));
        return "";
    }
}

// Helper function to replace the current selection
void replaceSelection(const std::string& newText) {
    HWND curScintilla;
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1) return;
    curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

    ::SendMessage(curScintilla, SCI_REPLACESEL, 0, (LPARAM)newText.c_str());
}

// Function to format a line as an Offset
std::string formatAsOffset(const std::string& line) {
    try {
        if (line.empty()) return "";

        // Regular expression to match the format [offset][type] fieldName : dataType
        std::regex pattern(R"(\[([0-9A-F]+)\](?:\[[CS]\])?\s+(\w+)\s*:\s*(.+))");
        std::smatch matches;

        if (std::regex_search(line, matches, pattern)) {
            // Validate the offset is a valid hex number
            std::string offset = matches[1].str();
            std::string fieldName = matches[2].str();
            std::string dataType = matches[3].str();

            // Additional validation
            if (offset.empty() || fieldName.empty() || dataType.empty()) {
                return "";
            }

            // Validate hex format
            try {
                unsigned long hexValue = std::stoul(offset, nullptr, 16);
                if (hexValue == 0 && offset != "0") {
                    // Invalid hex value (zero but not "0")
                    return "";
                }
            }
            catch (...) {
                return "";
            }

            // Format the output
            std::stringstream ss;
            ss << "\t\tpublic const uint " << fieldName << " = 0x" << offset << "; // " << dataType;
            return ss.str();
        }
    }
    catch (...) {
        // Silently skip problematic lines
        return "";
    }
    return "";
}

// Helper function to check if a class exists in the SDK file and get its line range
bool checkClassExists(const std::string& className, const std::string& sdkPath, size_t& startLine, size_t& endLine) {
    std::ifstream file(sdkPath);
    if (!file.is_open()) return false;

    std::string line;
    std::regex classPattern("public\\s+readonly\\s+partial\\s+struct\\s+" + className);
    size_t currentLine = 0;
    bool found = false;
    int braceCount = 0;
    
    while (std::getline(file, line)) {
        currentLine++;
        
        if (!found && std::regex_search(line, classPattern)) {
            found = true;
            startLine = currentLine;
            braceCount = 0;
            for (char c : line) {
                if (c == '{') braceCount++;
            }
            continue;
        }
        
        if (found) {
            for (char c : line) {
                if (c == '{') braceCount++;
                if (c == '}') braceCount--;
            }
            if (braceCount <= 0) {
                endLine = currentLine;
                return true;
            }
        }
    }
    return found;
}

// Helper function to check if file ends with proper namespace closure
bool hasProperNamespaceClosure(const std::string& sdkPath) {
    std::ifstream file(sdkPath);
    if (!file.is_open()) return false;

    // Read the entire file into a string
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Trim whitespace from the end
    content.erase(std::find_if(content.rbegin(), content.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), content.end());

    // Check if the file ends with a closing brace
    return content.empty() || content.back() == '}';
}

// Helper function to format warning message with line numbers
std::wstring formatWarningMessage(const std::string& className, size_t startLine, size_t endLine) {
    std::wstringstream ss;
    std::wstring wClassName;
    int classLen = static_cast<int>(MultiByteToWideChar(CP_UTF8, 0, className.c_str(), -1, NULL, 0) - 1);
    wClassName.resize(classLen);
    MultiByteToWideChar(CP_UTF8, 0, className.c_str(), -1, &wClassName[0], classLen + 1);
    
    ss << L"The class '" << wClassName
       << L"' already exists in the SDK file (lines " << startLine << L"-" << endLine 
       << L").\n\nDo you want to replace the existing definition?";
    return ss.str();
}

// Helper function to remove existing class from SDK file
void removeExistingClass(const std::string& className, const std::string& sdkPath) {
    std::ifstream inFile(sdkPath);
    std::stringstream buffer;
    std::string line;
    bool skipping = false;
    int braceCount = 0;

    while (std::getline(inFile, line)) {
        if (!skipping) {
            std::regex classPattern("public\\s+readonly\\s+partial\\s+struct\\s+" + className);
            if (std::regex_search(line, classPattern)) {
                skipping = true;
                braceCount = 0;
                continue;
            }
            buffer << line << "\n";
        }
        else {
            // Count braces to find the end of the class
            for (char c : line) {
                if (c == '{') braceCount++;
                if (c == '}') braceCount--;
            }
            if (braceCount <= 0) {
                skipping = false;
            }
        }
    }
    inFile.close();

    std::ofstream outFile(sdkPath);
    outFile << buffer.str();
}

// Function to format selected text into SDK format
std::string formatSelectedText(std::string& className, int& processedLines) {
    std::string selectedText = getCurrentSelection();
    if (selectedText.empty()) {
        showError(TEXT("Please select some text first."));
        return "";
    }

    std::stringstream input(selectedText);
    std::stringstream output;
    std::string line;
    processedLines = 0;
    std::string currentClass;
    std::stringstream currentClassOutput;
    bool insideClass = false;

    while (std::getline(input, line)) {
        // Check if this is a class declaration line
        std::regex classPattern(R"(\[Class\]\s+([^:]+)(?:\s*:\s*([^{]+))?)");
        std::smatch classMatches;
        
        if (std::regex_search(line, classMatches, classPattern)) {
            // If we were processing a previous class, close and add it to the output
            if (insideClass) {
                currentClassOutput << "    }\n\n";
                output << currentClassOutput.str();
                currentClassOutput.str(""); // Clear the buffer
            }

            std::string originalClassName = classMatches[1].str();
            currentClass = originalClassName;
            
            // Clean up the class name for C# compatibility
            // Remove special characters except underscores
            currentClass.erase(std::remove_if(currentClass.begin(), currentClass.end(), 
                [](char c) { return !std::isalnum(c) && c != '_' && c != '.'; }), currentClass.end());
            
            // Replace dots with underscores
            std::replace(currentClass.begin(), currentClass.end(), '.', '_');
            
            // Ensure the name starts with a letter or underscore
            if (!currentClass.empty() && !std::isalpha(currentClass[0]) && currentClass[0] != '_') {
                currentClass = "_" + currentClass;
            }
            
            insideClass = true;
            className = currentClass; // Store the last processed class name

            // Start building the output for this class
            currentClassOutput << "    // " << line << "\n";
            currentClassOutput << "    public readonly partial struct " << currentClass << "\n    {\n";
            continue;
        }

        // If we're inside a class definition, process the line
        if (insideClass) {
            std::string formattedLine = formatAsOffset(line);
            if (!formattedLine.empty()) {
                currentClassOutput << "        " << formattedLine.substr(formattedLine.find_first_not_of("\t ")) << "\n";
                processedLines++;
            }
        }
    }

    // Close and add the last class if we were processing one
    if (insideClass) {
        currentClassOutput << "    }\n\n";
        output << currentClassOutput.str();
    }

    return processedLines > 0 ? output.str() : "";
}

//
// Plugin command functions
//
void exportToMasterSDK()
{
    try {
        std::string className;
        int processedLines = 0;
        std::string formattedText = formatSelectedText(className, processedLines);
        
        if (formattedText.empty()) return;

        // Get current file's directory path
        TCHAR currentPath[MAX_PATH];
        if (::SendMessage(nppData._nppHandle, NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)currentPath) == 0) {
            showError(TEXT("Failed to get current directory."));
            return;
        }
        
        // Construct path to custom_SDK.cs
        std::wstring sdkPathW = std::wstring(currentPath) + L"\\custom_SDK.cs";
        std::string sdkPath;
        int pathLen = static_cast<int>(WideCharToMultiByte(CP_UTF8, 0, sdkPathW.c_str(), -1, NULL, 0, NULL, NULL) - 1);
        sdkPath.resize(pathLen);
        WideCharToMultiByte(CP_UTF8, 0, sdkPathW.c_str(), -1, &sdkPath[0], pathLen + 1, NULL, NULL);
        
        // Check if class already exists
        size_t startLine = 0, endLine = 0;
        if (checkClassExists(className, sdkPath, startLine, endLine)) {
            std::wstring warningMsg = formatWarningMessage(className, startLine, endLine);
            if (::MessageBox(NULL, warningMsg.c_str(),
                TEXT("SDK Formatter - Class Already Exists"),
                MB_YESNO | MB_ICONWARNING) == IDNO) {
                return;
            }
            // Remove existing class definition
            removeExistingClass(className, sdkPath);
        }

        // Check if file exists and validate its structure
        bool fileExists = std::filesystem::exists(sdkPath);
        if (fileExists && !hasProperNamespaceClosure(sdkPath)) {
            showError(TEXT("The SDK file appears to be corrupted or improperly formatted.\nPlease check the file structure."));
            return;
        }

        // Open file in append mode if it exists, create new otherwise
        std::ofstream file;
        if (fileExists) {
            file.open(sdkPath, std::ios::app);
        } else {
            file.open(sdkPath);
            file << "namespace SDK\n{\n";
        }

        if (!file.is_open()) {
            showError(TEXT("Failed to open or create SDK file. Please check file permissions."));
            return;
        }

        // Add the new class
        file << formattedText;
        file.close();

        if (!file.good()) {
            showError(TEXT("An error occurred while writing to the SDK file."));
            return;
        }

        // Open the SDK file in Notepad++
        if (::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)sdkPathW.c_str()) == 0) {
            showError(TEXT("Failed to open the SDK file after writing."));
            return;
        }
        
        // Set the language to C# for proper syntax highlighting
        ::SendMessage(nppData._nppHandle, NPPM_SETCURRENTLANGTYPE, 0, L_CS);
    }
    catch (const std::filesystem::filesystem_error& e) {
        std::wstringstream ss;
        ss << L"Filesystem error: " << e.what();
        showError(ss.str().c_str());
    }
    catch (const std::exception& e) {
        std::wstringstream ss;
        ss << L"Error: " << e.what();
        showError(ss.str().c_str());
    }
    catch (...) {
        showError(TEXT("An unexpected error occurred while processing the text."));
    }
}

void exportAsIndividualFile()
{
    try {
        std::string className;
        int processedLines = 0;
        std::string formattedText = formatSelectedText(className, processedLines);
        
        if (formattedText.empty()) return;

        // Get current file's directory path
        TCHAR currentPath[MAX_PATH];
        ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)currentPath);
        
        // Create suggested filename based on class name
        std::wstring suggestedName;
        int nameLen = static_cast<int>(MultiByteToWideChar(CP_UTF8, 0, className.c_str(), -1, NULL, 0) - 1);
        suggestedName.resize(nameLen);
        MultiByteToWideChar(CP_UTF8, 0, className.c_str(), -1, &suggestedName[0], nameLen + 1);
        suggestedName += L"_Offsets.cs";
        
        // Setup save dialog
        TCHAR fileName[MAX_PATH];
        wcscpy_s(fileName, suggestedName.c_str());
        
        OPENFILENAME ofn = { 0 };
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = nppData._nppHandle;
        ofn.lpstrFilter = TEXT("C# Files (*.cs)\0*.cs\0All Files (*.*)\0*.*\0");
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrInitialDir = currentPath;
        ofn.lpstrDefExt = TEXT("cs");
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

        // Show save dialog
        if (GetSaveFileName(&ofn)) {
            // Add namespace structure to individual file
            std::stringstream fullOutput;
            fullOutput << "namespace SDK\n{\n" << formattedText << "}\n";

            // Open the file
            FILE* fp = nullptr;
            if (_wfopen_s(&fp, fileName, L"w") == 0 && fp != nullptr) {
                std::string finalText = fullOutput.str();
                fwrite(finalText.c_str(), 1, finalText.length(), fp);
                fclose(fp);

                // Open the saved file in Notepad++
                ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)fileName);
                
                // Set the language to C# for proper syntax highlighting
                ::SendMessage(nppData._nppHandle, NPPM_SETCURRENTLANGTYPE, 0, L_CS);
            }
            else {
                showError(TEXT("Failed to write to the selected file."));
            }
        }
    }
    catch (...) {
        showError(TEXT("An unexpected error occurred while processing the text."));
    }
}

// Helper function to get the entire file content
std::string getCurrentFileContent() {
    try {
        HWND curScintilla;
        int which = -1;
        ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
        if (which == -1) {
            showError(TEXT("Failed to get current editor window."));
            return "";
        }
        curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

        // Get the file length
        size_t textLength = ::SendMessage(curScintilla, SCI_GETLENGTH, 0, 0);
        if (textLength == 0) {
            showError(TEXT("File is empty."));
            return "";
        }

        // Allocate buffer for entire file
        std::vector<char> buffer(textLength + 1);
        
        // Get the text directly
        ::SendMessage(curScintilla, SCI_GETTEXT, textLength + 1, reinterpret_cast<LPARAM>(buffer.data()));
        
        // Convert to string
        return std::string(buffer.data(), textLength);
    }
    catch (...) {
        showError(TEXT("Unexpected error while reading file content."));
        return "";
    }
}

// Function to export entire file to SDK format
void exportEntireFileToSDK() {
    try {
        // Get the entire file content at once
        std::string fileContent = getCurrentFileContent();
        if (fileContent.empty()) {
            return; // Error already shown in getCurrentFileContent
        }

        // Get path to custom_SDK.cs
        TCHAR path[MAX_PATH];
        ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTDIRECTORY, MAX_PATH, (LPARAM)path);
        std::wstring sdkPath = path;
        sdkPath += TEXT("\\custom_SDK.cs");

        // Open output file
        FILE* fp = nullptr;
        if (_wfopen_s(&fp, sdkPath.c_str(), L"w") != 0 || !fp) {
            showError(TEXT("Failed to open custom_SDK.cs for writing."));
            return;
        }

        // Write namespace header
        std::string header = "namespace SDK\n{\n";
        fwrite(header.c_str(), sizeof(char), header.length(), fp);

        // Track processed classes to avoid duplicates
        std::set<std::string> processedClasses;
        int classCount = 0;

        // Process each line
        std::istringstream iss(fileContent);
        std::string line;
        std::ostringstream classContent;
        bool insideClass = false;
        std::string currentClassName;
        std::string originalClassLine;

        while (std::getline(iss, line)) {
            // Check for class declaration
            size_t classPos = line.find("[Class]");
            if (classPos != std::string::npos) {
                // If we were processing a class, write it out
                if (insideClass) {
                    classContent << "    }\n\n";
                    std::string content = classContent.str();
                    fwrite(content.c_str(), sizeof(char), content.length(), fp);
                    classContent.str("");
                    classContent.clear();
                }

                // Extract class name and base class
                std::string className = line.substr(classPos);
                // Trim whitespace
                while (!className.empty() && std::isspace(className.front())) className.erase(0, 1);
                while (!className.empty() && std::isspace(className.back())) className.pop_back();

                // Create valid C# struct name from the class name
                std::string structName = className;
                size_t colonPos = structName.find(':');
                if (colonPos != std::string::npos) {
                    structName = structName.substr(7, colonPos - 7); // 7 is length of "[Class]"
                }
                // Trim whitespace
                while (!structName.empty() && std::isspace(structName.front())) structName.erase(0, 1);
                while (!structName.empty() && std::isspace(structName.back())) structName.pop_back();

                // Make the struct name valid C#
                std::string validStructName = structName;
                // Replace periods with underscores
                std::replace(validStructName.begin(), validStructName.end(), '.', '_');
                // Add underscore prefix if needed
                if (!validStructName.empty() && !std::isalpha(validStructName[0]) && validStructName[0] != '_') {
                    validStructName = '_' + validStructName;
                }
                // Replace any remaining invalid characters with underscore
                for (char& c : validStructName) {
                    if (!std::isalnum(c) && c != '_') {
                        c = '_';
                    }
                }

                // Skip if already processed
                if (processedClasses.find(validStructName) != processedClasses.end()) {
                    insideClass = false;
                    continue;
                }

                processedClasses.insert(validStructName);
                classCount++;
                insideClass = true;
                currentClassName = validStructName;
                originalClassLine = className;

                // Write class header
                classContent << "    // " << className << "\n";
                classContent << "    public readonly partial struct " << validStructName << "\n    {\n";
                continue;
            }

            // Process fields if inside a class
            if (insideClass) {
                size_t offsetStart = line.find('[');
                size_t offsetEnd = line.find(']', offsetStart);
                if (offsetStart != std::string::npos && offsetEnd != std::string::npos) {
                    // Get offset and field
                    std::string offset = line.substr(offsetStart + 1, offsetEnd - offsetStart - 1);
                    std::string field = line.substr(offsetEnd + 1);

                    // Check for [S] or [C] tag and skip it if present
                    size_t tagStart = line.find('[', offsetEnd + 1);
                    size_t tagEnd = line.find(']', tagStart);
                    size_t fieldStart;
                    
                    if (tagStart != std::string::npos && tagEnd != std::string::npos && tagStart < tagEnd) {
                        fieldStart = tagEnd + 1;
                    } else {
                        fieldStart = offsetEnd + 1;
                    }

                    field = line.substr(fieldStart);

                    // Verify it's a valid hex offset
                    bool validHex = true;
                    for (char c : offset) {
                        if (!std::isxdigit(static_cast<unsigned char>(c))) {
                            validHex = false;
                            break;
                        }
                    }

                    if (validHex && !offset.empty()) {
                        // Extract field name and type
                        size_t colonPos = field.find(':');
                        if (colonPos != std::string::npos) {
                            std::string fieldName = field.substr(0, colonPos);
                            std::string fieldType = field.substr(colonPos + 1);

                            // Trim whitespace
                            while (!fieldName.empty() && std::isspace(fieldName.front())) fieldName.erase(0, 1);
                            while (!fieldName.empty() && std::isspace(fieldName.back())) fieldName.pop_back();
                            while (!fieldType.empty() && std::isspace(fieldType.front())) fieldType.erase(0, 1);
                            while (!fieldType.empty() && std::isspace(fieldType.back())) fieldType.pop_back();

                            // Format the field line exactly like the example
                            classContent << "        public const uint " << fieldName << " = 0x" << offset << "; // " << fieldType << "\n";
                        }
                    }
                }
            }
        }

        // Close last class if any
        if (insideClass) {
            classContent << "    }\n";
            std::string content = classContent.str();
            fwrite(content.c_str(), sizeof(char), content.length(), fp);
        }

        // Close namespace
        std::string footer = "}\n";
        fwrite(footer.c_str(), sizeof(char), footer.length(), fp);

        fclose(fp);

        // Show success message
        TCHAR msg[256];
        _stprintf_s(msg, TEXT("Successfully exported %d classes to custom_SDK.cs"), classCount);
        ::MessageBox(nppData._nppHandle, msg, TEXT("SDK Formatter"), MB_OK | MB_ICONINFORMATION);

        // Open the file in Notepad++
        ::SendMessage(nppData._nppHandle, NPPM_DOOPEN, 0, (LPARAM)sdkPath.c_str());
        ::SendMessage(nppData._nppHandle, NPPM_SETCURRENTLANGTYPE, 0, L_CS);
    }
    catch (const std::exception& e) {
        TCHAR msg[256];
        size_t convertedChars = 0;
        mbstowcs_s(&convertedChars, msg, e.what(), _TRUNCATE);
        showError(msg);
    }
    catch (...) {
        showError(TEXT("Unexpected error while exporting file."));
    }
}
