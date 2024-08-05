// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <stdlib.h>
#include <iostream>
#include "Utils.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include "enums.h"
//#pragma warning(disable:4996) 

// Link with Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

enum TriggerModes
{
    Off = 0,
    Rigid = 1,
    Pulse = 5,
    Rigid_A = 2,
    Rigid_B = 3,
    Rigid_AB = 4,
    Pulse_A = 6,
    Pulse_B = 7,
    Pulse_AB = 8,
};

enum PlayerLED
{
    PLED_OFF = 0,
    PLAYER_1 = 1,
    PLAYER_2 = 2,
    PLAYER_3 = 3,
    PLAYER_4 = 4,
    ALL = 5
};

enum MicrophoneLED
{
    MICLED_ON = 1,
    MICLED_OFF = 0
};

int rumbleFlag = 0xFC;
TriggerModes leftMode = Off;
TriggerModes rightMode = Off;
int leftTriggerThreshold = 0;
int rightTriggerThreshold = 0;
int leftTriggerForce[7];
int rightTriggerForce[7];
MicrophoneLED micLed = MICLED_OFF;
PlayerLED playerLed = PLAYER_1;
int R;
int G;
int B;
int leftMotor = 0;
int rightMotor = 0;
int speakerVolume; // volume ranges from 0 to 100
int leftActuatorVolume = 100; // volume ranges from 0 to 100
int rightActuatorVolume = 100; // volume ranges from 0 to 100
bool clearBuffer = false;
bool lightbarTransition = false;
int transitionSteps = 0;
int transitionDelay = 0;
const char* directory = "dualsense-service\\haptics\\";
const char* wavFileLocation = "";

//{"instructions":[{"type":1,"parameters":[0,2,2]}]}

int startSendingToService() {
    WSADATA wsaData;
    SOCKET SendSocket = INVALID_SOCKET;
    sockaddr_in RecvAddr;
    unsigned short Port = 6969;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Create a socket for sending data
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        std::cerr << "socket failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Set up the RecvAddr structure with the IP address and port of the receiver
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(Port);
    RecvAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    std::string packet = "";
    while (true)
    {
        packet = "{\"instructions\":[{\"type\":2,\"parameters\":[0," + std::to_string(R) + "," + std::to_string(G) + "," + std::to_string(B) + "]}]}"; // RGB
        sendto(SendSocket, packet.c_str(), strlen(packet.c_str()), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));

        packet = "{\"instructions\":[{\"type\":1,\"parameters\":[0,2,12," + std::to_string(rightMode) + "," + std::to_string(rightTriggerForce[0]) + "," + std::to_string(rightTriggerForce[1])
            + "," + std::to_string(rightTriggerForce[2]) + "," + std::to_string(rightTriggerForce[3]) + "," + std::to_string(rightTriggerForce[4])
            + "," + std::to_string(rightTriggerForce[5]) + "," + std::to_string(rightTriggerForce[6]) + "]}]}"; 
        sendto(SendSocket, packet.c_str(), strlen(packet.c_str()), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
       
        packet = "{\"instructions\":[{\"type\":4,\"parameters\":[0," + std::to_string(leftTriggerThreshold) + "," + std::to_string(rightTriggerThreshold) + "]}]}";
        sendto(SendSocket, packet.c_str(), strlen(packet.c_str()), 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
        
        Sleep(1);
    }

    return 0;
}

DWORD64* jmpBackAdaptiveTriggers;
__attribute__((naked))
void adaptiveTriggersHook() {
    _asm {
        mov rax, 0x0
        mov [rcx + 0x00000260], rax
        movsd xmm0, [r9]
        movsd [rcx + 0x00000264], xmm0
        mov rax, qword ptr[jmpBackAdaptiveTriggers]
        jmp rax
    }
}

DWORD64* jmpBackTriggerForce;
unsigned short nativeTriggerForce = 0;
__attribute__((naked))
void triggerForceHook() {
    _asm {
        mov[rbp + 0x000000C0], eax
        mov [nativeTriggerForce], eax
        mov rcx, [rdi + 0x000001D0]
        mov rax, qword ptr[jmpBackTriggerForce]
        jmp rax
    }
}

DWORD64* jmpBackfppAnimation;
unsigned short fppAnimation = 0;
__attribute__((naked))
void fppAnimationHook() {
    __asm {
        mov [rsi + 0x18], ecx
        mov[fppAnimation], ecx
        mov rcx, r15
        mov rax, [r14 + 0x08]
        mov [rsi + 0x08], rax
        mov rdx, qword ptr[jmpBackfppAnimation]
        jmp rdx
    }
}

DWORD64* jmpBackImmunity;
float immunity = 0;
__attribute__((naked))
void immunityHook() {
    __asm {
        movss[rdi + 0x2C], xmm0
        comiss xmm10, xmm7
        movzx ecx, byte ptr [rdi+0x00001435]
        movss [immunity], xmm0
        mov rax, qword ptr[jmpBackImmunity]
        jmp rax
    }
}

DWORD64* jmpBackClipsize;
unsigned short ClipSize = 0;
__attribute__((naked))
void clipsizeHook() {
    __asm {
        movd xmm0, [rdi + 0x10]
        movd [ClipSize], xmm0
        cvtdq2ps xmm0, xmm0
        mov rbx, [rsp + 0x30]
        add rsp, 0x20
        mov rax, qword ptr[jmpBackClipsize]
        jmp rax
    }
}

bool compare_float(float x, float y, float epsilon = 0.01f) {
    if (fabs(x - y) < epsilon)
        return true; //they are same
    return false; //they are not same
}

void setTrigger(TriggerModes triggerMode, int triggerThreshold, bool rightTrigger, int force1, int force2, int force3, int force4, int force5, int force6, int force7) {
    if (rightTrigger) {
        rightMode = triggerMode;
        rightTriggerThreshold = triggerThreshold;
        rightTriggerForce[0] = force1;
        rightTriggerForce[1] = force2;
        rightTriggerForce[2] = force3;
        rightTriggerForce[3] = force4;
        rightTriggerForce[4] = force5;
        rightTriggerForce[5] = force6;
        rightTriggerForce[6] = force7;
    }
    else {
        leftMode = triggerMode;
        leftTriggerThreshold = triggerThreshold;
        leftTriggerForce[0] = force1;
        leftTriggerForce[1] = force2;
        leftTriggerForce[2] = force3;
        leftTriggerForce[3] = force4;
        leftTriggerForce[4] = force5;
        leftTriggerForce[5] = force6;
        leftTriggerForce[6] = force7;
    }
}

void read() {
    float lastImmunity = 0;
    bool firstBeepPlayed = false;
    float immunityBeep = 0;
    unsigned short lastClipsize = 0;
    animationData animation;
    while (true) {
        animation = static_cast<animationData>(fppAnimation);
        switch (animation)
        {
        case EMPTY_HANDED_IDLE:
            setTrigger(Rigid_B, 0, true, 0, 0, 0, 0, 0, 0, 0);
            break;
        case EMPTY_HANDED_RUNNING:
            setTrigger(Rigid_B, 0, true, 0, 0, 0, 0, 0, 0, 0);
            break;
        case ONE_H_DEFAULT:
            setTrigger(Pulse_A, 0, true, 20, 255, 0, 0, 0, 0, 0);
            break;

        case ONE_H_SWING_LEFT:
            break;

        case ONE_H_SWING_RIGHT:
            break;

        case TWO_H_DEFAULT:
            setTrigger(Rigid, 0, true, 0, 255, 0, 0, 0, 0, 0);
            break;

        case TWO_H_SWING_LEFT:
            break;

        case TWO_H_SWING_RIGHT:
            break;

        case LIGHT_TWO_H_DEFAULT:
            break;

        case POLEARM_DEFAULT:
            setTrigger(Pulse_A, 0, true, 20, 255, 0, 0, 0, 0, 0);
            break;

        case FISTS_DEFAULT:
            setTrigger(Rigid,0, true, 125, 0, 0, 0, 0, 0, 0);
            break;

        case BOW_DEFAULT:
            setTrigger(Rigid,0, true, 0,255,0,255,255,255,0);
            break;

        case PISTOL_DEFAULT:
            setTrigger(Pulse, 150, true, 80, 170, 0, 0, 0, 0, 0);
            break;

        case SMG_DEFAULT:
            if (ClipSize > 0) {
                setTrigger(Pulse_B, 150, true, 14, 255, 80, 0, 0, 0, 0);
                lastClipsize = ClipSize;
            }
            else {
                setTrigger(Pulse,255, true, 160, 200, 100, 0, 0, 0, 0);
            }
            break;

        case SMG_RELOADING:
            setTrigger(Pulse,0, true, 160, 200, 100, 0, 0, 0, 0);
            break;

        case AUTOMATIC_RIFLE_DEFAULT:
            if (ClipSize > 0) {
                setTrigger(Pulse_B, 150, true, 10, 255, 80, 0, 0, 0, 0);
            }
            else {
                setTrigger(Pulse, 255,true, 160, 200, 100, 0, 0, 0, 0);
            }
            break;

        case AUTOMATIC_RIFLE_RELOADING:
            setTrigger(Pulse, true,0,160, 200, 100, 0, 0, 0, 0);
            break;

        case SHOTGUN_DEFAULT:
            if (ClipSize > 0) {
                setTrigger(Pulse_AB, 150, true, 255, 0, 255, 255, 255, 0, 0);
            }
            else {
                setTrigger(Pulse_AB,255, true, 1, 0, 255, 255, 255, 0, 0);
            }
            break;

        case SHOTGUN_RELOADING:
            setTrigger(Pulse_AB,0, true, 1, 0, 255, 255, 255, 0, 0);
            break;

        default:
            break;
        }

        if (immunity == 0) {
            if (immunityBeep == 300) {
                immunityBeep = 0;
            }
            else if (immunityBeep > 150) {
                immunityBeep++;
                R = 255;
                G = 0;
                B = 0;
            }
            else {
                immunityBeep++;
                R = 1;
                G = 0;
                B = 0;
            }
        }
        else if (immunity < 0.05 && immunity > 0.45) {
            if (immunityBeep == 50 * immunity) {
                immunityBeep = 0;
            }
            else if (immunityBeep > 25 * immunity) {
                immunityBeep++;
                R = 255;
                G = 0;
                B = 0;
            }
            else {
                immunityBeep++;
                R = 1;
                G = 0;
                B = 0;
            }
        }
        Sleep(1);
    }
}

void inject() {
    Utils utils;

    Sleep(8000);

    HMODULE gameDllModule = GetModuleHandle(L"gamedll_ph_x64_rwdi.dll");
    if (gameDllModule == NULL) {
        std::cerr << "Failed to get module handle." << std::endl;
        std::cout << (DWORD)gameDllModule << std::endl;
    }
    else {
        std::cout << "Module gamedll_ph_x64_rwdi.dll found at: ";
        std::cout << (DWORD)gameDllModule << std::endl;
    }

    HMODULE engineDllModule = GetModuleHandle(L"engine_x64_rwdi.dll");
    if (engineDllModule == NULL) {
        std::cerr << "Failed to get module handle." << std::endl;
        std::cout << (DWORD)engineDllModule << std::endl;
    }
    else {
        std::cout << "Module engine_x64_rwdi.dll found at: ";
        std::cout << (DWORD)engineDllModule << std::endl;
    }

    DWORD64* fppAnimationAddress = utils.scanModuleMemory(gameDllModule, "89 4E 18 49 8B CF");
    std::cout << "fppAnimation function: ";
    std::cout << fppAnimationAddress << std::endl;
    utils.Detour(reinterpret_cast<DWORD*>((void*)fppAnimationAddress), fppAnimationHook, 14);
    jmpBackfppAnimation = utils.scanModuleMemory(gameDllModule, "49 8B 46 10 48 89 46 10 41 8B 46 1C");
    std::cout << "fppAnimation jmp back: ";
    std::cout << jmpBackfppAnimation << std::endl;

    std::cout << "----------------------------------" << std::endl;

    DWORD64* immunityAddress = utils.scanModuleMemory(gameDllModule, "F3 0F 11 47 2C 44 0F");
    std::cout << "immunity function: ";
    std::cout << immunityAddress << std::endl;
    utils.Detour(reinterpret_cast<DWORD*>((void*)immunityAddress), immunityHook, 14);
    jmpBackImmunity = utils.scanModuleMemory(gameDllModule, "0F B6 C1 0F 28 C8 76 0D");
    std::cout << "immunity jmp back: ";
    std::cout << jmpBackImmunity << std::endl;

    std::cout << "----------------------------------" << std::endl;

    DWORD64* clipsizeAddress = utils.scanModuleMemory(gameDllModule, "66 0F 6E 47 10 0F 5B C0");
    std::cout << "clipsize function: ";
    std::cout << clipsizeAddress << std::endl;
    utils.Detour(reinterpret_cast<DWORD*>((void*)clipsizeAddress), clipsizeHook, 17);
    jmpBackClipsize = utils.scanModuleMemory(gameDllModule, "5F C3 48 8B 5C 24 30 0F 57 C0 48 83 C4 20 5F C3 CC CC CC CC CC CC CC CC CC CC CC 40 53");
    std::cout << "clipsize jmp back: ";
    std::cout << jmpBackClipsize << std::endl;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONIN$", "r", stdin);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        freopen_s(&fDummy, "CONOUT$", "w", stdout);

        HANDLE injectThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)inject, hModule, 0, 0);
        HANDLE controllerServiceThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)startSendingToService, hModule, 0, 0);
        HANDLE readThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)read, hModule, 0, 0);
        break;
    }

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
