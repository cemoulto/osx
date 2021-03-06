/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOHibernatePrivate.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/graphics/IOGraphicsTypes.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/IOHibernatePrivate.h>
#if !TARGET_OS_EMBEDDED
#include <IOKit/platform/IOPlatformSupportPrivate.h>
#endif
#include <Security/SecTask.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <grp.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>
#include <asl.h>
#include <membership.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <unistd.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include <notify.h>

#include "Platform.h"
#include "PrivateLib.h"
#include "BatteryTimeRemaining.h"
#include "PMAssertions.h"
#include "PMSettings.h"
#include "PMAssertions.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

enum
{
    // 2GB
    kStandbyDesktopHibernateFileSize = 2ULL*1024*1024*1024,
    // 1GB
    kStandbyPortableHibernateFileSize = 1ULL*1024*1024*1024
};


long     physicalBatteriesCount = 0;

static int getAggressivenessFactorsFromProfile(
                                               CFDictionaryRef                 p,
                                               IOPMAggressivenessFactors       *agg);
static int ProcessHibernateSettings(
                                    CFDictionaryRef                 dict,
                                    bool                            standby,
                                    bool                            desktop,
                                    io_registry_entry_t             rootDomain);

/*
 * _batteryCount
 */
__private_extern__ int  _batteryCount(void)
{
    return ((int)physicalBatteriesCount);
}

__private_extern__
const char *stringForLWCode(uint8_t code)
{
    const char *string;
    switch (code)
    {
        default:
            string = "OK";
    }
    return string;
}

__private_extern__
const char *stringForPMCode(uint8_t code)
{
    const char *string = "";

    switch (code)
    {
        case kIOPMTracePointSystemUp:
            string = "On";
            break;
        case kIOPMTracePointSleepStarted:
            string = "SleepStarted";
            break;
        case kIOPMTracePointSleepApplications:
            string = "SleepApps";
            break;
        case kIOPMTracePointSleepPriorityClients:
            string = "SleepPriority";
            break;
        case kIOPMTracePointSleepWillChangeInterests:
            string = "SleepWillChangeInterests";
            break;
        case kIOPMTracePointSleepPowerPlaneDrivers:
            string = "SleepDrivers";
            break;
        case kIOPMTracePointSleepDidChangeInterests:
            string = "SleepDidChangeInterests";
            break;
        case kIOPMTracePointSleepCapabilityClients:
            string = "SleepCapabilityClients";
            break;
        case kIOPMTracePointSleepPlatformActions:
            string = "SleepPlatformActions";
            break;
        case kIOPMTracePointSleepCPUs:
            string = "SleepCPUs";
            break;
        case kIOPMTracePointSleepPlatformDriver:
            string = "SleepPlatformDriver";
            break;
        case kIOPMTracePointSystemSleep:
            string = "SleepPlatform";
            break;
        case kIOPMTracePointHibernate:
            string = "Hibernate";
            break;
        case kIOPMTracePointWakePlatformDriver:
            string = "WakePlatformDriver";
            break;
        case kIOPMTracePointWakePlatformActions:
            string = "WakePlatformActions";
            break;
        case kIOPMTracePointWakeCPUs:
            string = "WakeCPUs";
            break;
        case kIOPMTracePointWakeWillPowerOnClients:
            string = "WakeWillPowerOnClients";
            break;
        case kIOPMTracePointWakeWillChangeInterests:
            string = "WakeWillChangeInterests";
            break;
        case kIOPMTracePointWakeDidChangeInterests:
            string = "WakeDidChangeInterests";
            break;
        case kIOPMTracePointWakePowerPlaneDrivers:
            string = "WakeDrivers";
            break;
        case kIOPMTracePointWakeCapabilityClients:
            string = "WakeCapabilityClients";
            break;
        case kIOPMTracePointWakeApplications:
            string = "WakeApps";
            break;
        case kIOPMTracePointSystemLoginwindowPhase:
            string = "WakeLoginWindow";
            break;
        case kIOPMTracePointDarkWakeEntry:
            string = "DarkWakeEntry";
            break;
        case kIOPMTracePointDarkWakeExit:
            string = "DarkWakeExit";
            break;
    }
    return string;
}


__private_extern__ CFAbsoluteTime _CFAbsoluteTimeFromPMEventTimeStamp(uint64_t kernelPackedTime)
{
    uint32_t    cal_sec = (uint32_t)(kernelPackedTime >> 32);
    uint32_t    cal_micro = (uint32_t)(kernelPackedTime & 0xFFFFFFFF);
    CFAbsoluteTime timeKernelEpoch = (CFAbsoluteTime)(double)cal_sec + (double)cal_micro/1000.0;

    // Adjust from kernel 1970 epoch to CF 2001 epoch
    timeKernelEpoch -= kCFAbsoluteTimeIntervalSince1970;

    return timeKernelEpoch;
}

/* getAggressivenessValue
 *
 * returns true if the setting existed in the dictionary
 */
__private_extern__ bool getAggressivenessValue(
                                               CFDictionaryRef     dict,
                                               CFStringRef         key,
                                               CFNumberType        type,
                                               uint32_t           *ret)
{
    CFTypeRef           obj = CFDictionaryGetValue(dict, key);
    
    *ret = 0;
    if (isA_CFNumber(obj))
    {
        CFNumberGetValue(obj, type, ret);
        return true;
    }
    else if (isA_CFBoolean(obj))
    {
        *ret = CFBooleanGetValue(obj);
        return true;
    }
    return false;
}

/* For internal use only */
static int getAggressivenessFactorsFromProfile(
                                               CFDictionaryRef p,
                                               IOPMAggressivenessFactors *agg)
{
    if( !agg || !p ) {
        return -1;
    }
    
    getAggressivenessValue(p, CFSTR(kIOPMDisplaySleepKey), kCFNumberSInt32Type, &agg->fMinutesToDim);
    getAggressivenessValue(p, CFSTR(kIOPMDiskSleepKey), kCFNumberSInt32Type, &agg->fMinutesToSpin);
    getAggressivenessValue(p, CFSTR(kIOPMSystemSleepKey), kCFNumberSInt32Type, &agg->fMinutesToSleep);
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnLANKey), kCFNumberSInt32Type, &agg->fWakeOnLAN);
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnRingKey), kCFNumberSInt32Type, &agg->fWakeOnRing);
    getAggressivenessValue(p, CFSTR(kIOPMRestartOnPowerLossKey), kCFNumberSInt32Type, &agg->fAutomaticRestart);
    getAggressivenessValue(p, CFSTR(kIOPMSleepOnPowerButtonKey), kCFNumberSInt32Type, &agg->fSleepOnPowerButton);
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnClamshellKey), kCFNumberSInt32Type, &agg->fWakeOnClamshell);
    getAggressivenessValue(p, CFSTR(kIOPMWakeOnACChangeKey), kCFNumberSInt32Type, &agg->fWakeOnACChange);
    getAggressivenessValue(p, CFSTR(kIOPMDisplaySleepUsesDimKey), kCFNumberSInt32Type, &agg->fDisplaySleepUsesDimming);
    getAggressivenessValue(p, CFSTR(kIOPMMobileMotionModuleKey), kCFNumberSInt32Type, &agg->fMobileMotionModule);
    getAggressivenessValue(p, CFSTR(kIOPMGPUSwitchKey), kCFNumberSInt32Type, &agg->fGPU);
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepEnabledKey), kCFNumberSInt32Type, &agg->fDeepSleepEnable);
    getAggressivenessValue(p, CFSTR(kIOPMDeepSleepDelayKey), kCFNumberSInt32Type, &agg->fDeepSleepDelay);
    getAggressivenessValue(p, CFSTR(kIOPMAutoPowerOffEnabledKey), kCFNumberSInt32Type, &agg->fAutoPowerOffEnable);
    getAggressivenessValue(p, CFSTR(kIOPMAutoPowerOffDelayKey), kCFNumberSInt32Type, &agg->fAutoPowerOffDelay);
    
    return 0;
}

__private_extern__ io_registry_entry_t getRootDomain(void)
{
    static io_registry_entry_t gRoot = MACH_PORT_NULL;
    
    if (MACH_PORT_NULL == gRoot)
        gRoot = IORegistryEntryFromPath( kIOMasterPortDefault,
                                        kIOPowerPlane ":/IOPowerConnection/IOPMrootDomain");
    
    return gRoot;
}

#define kIOPMSystemDefaultOverrideKey    "SystemPowerProfileOverrideDict"

__private_extern__ bool platformPluginLoaded(void)
{
    static bool         gPlatformPluginLoaded = false;
    io_registry_entry_t rootDomain;
    CFTypeRef           prop;
    
    if (gPlatformPluginLoaded) return (true);
    
    rootDomain = getRootDomain();
    if (MACH_PORT_NULL == rootDomain) return (false);
    prop = IORegistryEntryCreateCFProperty(rootDomain, CFSTR(kIOPMSystemDefaultOverrideKey),
                                           kCFAllocatorDefault, kNilOptions);
    if (prop)
    {
        gPlatformPluginLoaded = true;
        CFRelease(prop);
    }
    return (gPlatformPluginLoaded);
}

/* extern symbol defined in IOKit.framework
 * IOCFURLAccess.c
 */
extern Boolean _IOReadBytesFromFile(CFAllocatorRef alloc, const char *path, void **bytes, CFIndex *length, CFIndex maxLength);

static int ProcessHibernateSettings(CFDictionaryRef dict, bool standby, bool isDesktop, io_registry_entry_t rootDomain)
{
#if !TARGET_OS_EMBEDDED
    IOReturn    ret;
    CFTypeRef   obj;
    CFNumberRef modeNum;
    CFNumberRef num;
    SInt32      modeValue = 0;
    CFURLRef    url = NULL;
    Boolean createFile = false;
    Boolean haveFile = false;
    struct stat statBuf;
    char    path[MAXPATHLEN];
    int        fd;
    long long    size;
    size_t    len;
    fstore_t    prealloc;
    off_t    filesize;
    off_t    minFileSize = 0;
    off_t    maxFileSize = 0;
    bool     apo_available = false;
    SInt32   apo_enabled = 0;
    CFNumberRef apo_enabled_cf = NULL;
    
    if (!platformPluginLoaded()) return (0);
    
    if ( !IOPMFeatureIsAvailable( CFSTR(kIOHibernateFeatureKey), NULL ) )
    {
        // Hibernation is not supported; return before we touch anything.
        return 0;
    }
    
    
    if ((modeNum = CFDictionaryGetValue(dict, CFSTR(kIOHibernateModeKey)))
        && isA_CFNumber(modeNum))
        CFNumberGetValue(modeNum, kCFNumberSInt32Type, &modeValue);
    else
        modeNum = NULL;
    
    apo_available = IOPMFeatureIsAvailable(CFSTR(kIOPMAutoPowerOffEnabledKey), NULL);
    if (apo_available &&
        (apo_enabled_cf = CFDictionaryGetValue(dict, CFSTR(kIOPMAutoPowerOffEnabledKey ))) &&
        isA_CFNumber(apo_enabled_cf))
    {
        CFNumberGetValue(apo_enabled_cf, kCFNumberSInt32Type, &apo_enabled);
    }
    
    if ((modeValue || (apo_available && apo_enabled))
        && (obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFileKey)))
        && isA_CFString(obj))
        do
        {
            url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, obj, kCFURLPOSIXPathStyle, true);
            
            if (!url || !CFURLGetFileSystemRepresentation(url, TRUE, (UInt8 *) path, MAXPATHLEN))
                break;
            
            len = sizeof(size);
            if (sysctlbyname("hw.memsize", &size, &len, NULL, 0))
                break;
            
            filesize = (size >> 1);
            if (isDesktop)
            {
                if (standby && (filesize > kStandbyDesktopHibernateFileSize)) filesize = kStandbyDesktopHibernateFileSize;
            }
            else
            {
                if (standby && (filesize > kStandbyPortableHibernateFileSize)) filesize = kStandbyPortableHibernateFileSize;
            }
            minFileSize = filesize;
            maxFileSize = 0;
            
            if (0 != stat(path, &statBuf)) createFile = true;
            else
            {
                if ((S_IFBLK == (S_IFMT & statBuf.st_mode))
                    || (S_IFCHR == (S_IFMT & statBuf.st_mode)))
                {
                    haveFile = true;
                }
                else if (S_IFREG == (S_IFMT & statBuf.st_mode))
                {
                    if ((statBuf.st_size == filesize) || (kIOHibernateModeFileResize & modeValue))
                        haveFile = true;
                    else
                        createFile = true;
                }
                else
                    break;
            }
            
            if (createFile)
            {
                do
                {
                    char *    patchpath, save = 0;
                    struct    statfs sfs;
                    u_int64_t fsfree;
                    
                    fd = -1;
                    
                    /*
                     * get rid of the filename at the end of the file specification
                     * we only want the portion of the pathname that should already exist
                     */
                    if ((patchpath = strrchr(path, '/')))
                    {
                        save = *patchpath;
                        *patchpath = 0;
                    }
                    
                    if (-1 == statfs(path, &sfs))
                        break;
                    
                    fsfree = ((u_int64_t)sfs.f_bfree * (u_int64_t)sfs.f_bsize);
                    if ((fsfree - filesize) < kIOHibernateMinFreeSpace)
                        break;
                    
                    if (patchpath)
                        *patchpath = save;
                    fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 01600);
                    if (-1 == fd)
                        break;
                    if (-1 == fchmod(fd, 01600))
                        break;
                    
                    prealloc.fst_flags = F_ALLOCATEALL; // F_ALLOCATECONTIG
                    prealloc.fst_posmode = F_PEOFPOSMODE;
                    prealloc.fst_offset = 0;
                    prealloc.fst_length = filesize;
                    if (((-1 == fcntl(fd, F_PREALLOCATE, &prealloc))
                         || (-1 == fcntl(fd, F_SETSIZE, &prealloc.fst_length)))
                        && (-1 == ftruncate(fd, prealloc.fst_length)))
                        break;
                    
                    haveFile = true;
                }
                while (false);
                if (-1 != fd)
                {
                    close(fd);
                    if (!haveFile)
                        unlink(path);
                }
            }
            
            if (!haveFile)
                break;
            
#if defined (__i386__) || defined(__x86_64__)
#define kBootXPath        "/System/Library/CoreServices/boot.efi"
#define kBootXSignaturePath    "/System/Library/Caches/com.apple.bootefisignature"
#else
#define kBootXPath        "/System/Library/CoreServices/BootX"
#define kBootXSignaturePath    "/System/Library/Caches/com.apple.bootxsignature"
#endif
#define kCachesPath        "/System/Library/Caches"
#define kGenSignatureCommand    "/bin/cat " kBootXPath " | /usr/bin/openssl dgst -sha1 -hex -out " kBootXSignaturePath
            
            
            struct stat bootx_stat_buf;
            struct stat bootsignature_stat_buf;
            
            if (0 != stat(kBootXPath, &bootx_stat_buf))
                break;
            
            if ((0 != stat(kBootXSignaturePath, &bootsignature_stat_buf))
                || (bootsignature_stat_buf.st_mtime != bootx_stat_buf.st_mtime))
            {
                if (-1 == stat(kCachesPath, &bootsignature_stat_buf))
                {
                    mkdir(kCachesPath, 0777);
                    chmod(kCachesPath, 0777);
                }
                
                // generate signature file
                if (0 != system(kGenSignatureCommand))
                    break;
                
                // set mod time to that of source
                struct timeval fileTimes[2];
                TIMESPEC_TO_TIMEVAL(&fileTimes[0], &bootx_stat_buf.st_atimespec);
                TIMESPEC_TO_TIMEVAL(&fileTimes[1], &bootx_stat_buf.st_mtimespec);
                if ((0 != utimes(kBootXSignaturePath, fileTimes)))
                    break;
            }
            
            
            // send signature to kernel
            CFAllocatorRef alloc;
            void *         sigBytes;
            CFIndex        sigLen;
            
            alloc = CFRetain(CFAllocatorGetDefault());
            if (_IOReadBytesFromFile(alloc, kBootXSignaturePath, &sigBytes, &sigLen, 0))
                ret = sysctlbyname("kern.bootsignature", NULL, NULL, sigBytes, sigLen);
            else
                ret = -1;
            if (sigBytes)
                CFAllocatorDeallocate(alloc, sigBytes);
            CFRelease(alloc);
            if (0 != ret)
                break;
            
            IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileKey), obj);
        }
    while (false);
    
    if (modeNum)
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateModeKey), modeNum);
    
    if ((obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFreeRatioKey)))
        && isA_CFNumber(obj))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFreeRatioKey), obj);
    }
    if ((obj = CFDictionaryGetValue(dict, CFSTR(kIOHibernateFreeTimeKey)))
        && isA_CFNumber(obj))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFreeTimeKey), obj);
    }
    if (minFileSize && (num = CFNumberCreate(NULL, kCFNumberLongLongType, &minFileSize)))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileMinSizeKey), num);
        CFRelease(num);
    }
    if (maxFileSize && (num = CFNumberCreate(NULL, kCFNumberLongLongType, &maxFileSize)))
    {
        IORegistryEntrySetCFProperty(rootDomain, CFSTR(kIOHibernateFileMaxSizeKey), num);
        CFRelease(num);
    }
    
    if (url)
        CFRelease(url);
    
#endif
    return (0);
}


static void sendEnergySettingsToKernel(
                                       CFDictionaryRef                 useSettings,
                                       bool                            removeUnsupportedSettings,
                                       IOPMAggressivenessFactors       *p)
{
    io_registry_entry_t             PMRootDomain = getRootDomain();
    io_connect_t                    PM_connection = MACH_PORT_NULL;
    CFDictionaryRef                 _supportedCached = NULL;
    CFStringRef                     providing_power = NULL;
    CFNumberRef                     number1 = NULL;
    CFNumberRef                     number0 = NULL;
    CFNumberRef                     num = NULL;
    uint32_t                        i;
    
    i = 1;
    number1 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
    i = 0;
    number0 = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &i);
    
    if (!number0 || !number1)
        goto exit;
    
    PM_connection = IOPMFindPowerManagement(0);
    
    if (!PM_connection)
        goto exit;
    
#ifdef __I_AM_PMSET__
    
    /* pmset calls IOPSCopyPowerSourcesInfo here; it doesn't link
     * with powerd's alternative call getACtivePSType.
     */
    CFTypeRef power_source_info = IOPSCopyPowerSourcesInfo();
    if(power_source_info) {
        providing_power = IOPSGetProvidingPowerSourceType(power_source_info);
        CFRelease(power_source_info);
    }
    
#else
    /* powerd should call getActivePSType to determine active power source
     * powred cannot call IOPSCopyPowerSourcesInfo() because that's
     * a blocking IPC call into... powerd.
     */
    
    // Determine type of power source
    int powersource = getActivePSType();
    if (kIOPSProvidedByExternalBattery == powersource) {
        providing_power = CFSTR(kIOPMUPSPowerKey);
    } else if (kIOPSProvidedByBattery == powersource) {
        providing_power = CFSTR(kIOPMBatteryPowerKey);
    } else {
        providing_power = CFSTR(kIOPMACPowerKey);
    }
#endif
    
    // Grab a copy of RootDomain's supported energy saver settings
    _supportedCached = IORegistryEntryCreateCFProperty(PMRootDomain, CFSTR("Supported Features"), kCFAllocatorDefault, kNilOptions);
    
    IOPMSetAggressiveness(PM_connection, kPMMinutesToSleep, p->fMinutesToSleep);
    IOPMSetAggressiveness(PM_connection, kPMMinutesToSpinDown, p->fMinutesToSpin);
    IOPMSetAggressiveness(PM_connection, kPMMinutesToDim, p->fMinutesToDim);
    
    
    // Wake on LAN
    if(true == IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnLANKey), providing_power, _supportedCached))
    {
        IOPMSetAggressiveness(PM_connection, kPMEthernetWakeOnLANSettings, p->fWakeOnLAN);
    } else {
        // Even if WakeOnLAN is reported as not supported, broadcast 0 as
        // value. We may be on a supported machine, just on battery power.
        // Wake on LAN is not supported on battery power on PPC hardware.
        IOPMSetAggressiveness(PM_connection, kPMEthernetWakeOnLANSettings, 0);
    }
    
    // Display Sleep Uses Dim
    if ( !removeUnsupportedSettings
        || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMDisplaySleepUsesDimKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMSettingDisplaySleepUsesDimKey),
                                     (p->fDisplaySleepUsesDimming?number1:number0));
    }
    
    // Wake On Ring
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnRingKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMSettingWakeOnRingKey),
                                     (p->fWakeOnRing?number1:number0));
    }
    
    // Automatic Restart On Power Loss, aka FileServer mode
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMRestartOnPowerLossKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMSettingRestartOnPowerLossKey),
                                     (p->fAutomaticRestart?number1:number0));
    }
    
    // Wake on change of AC state -- battery to AC or vice versa
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnACChangeKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMSettingWakeOnACChangeKey),
                                     (p->fWakeOnACChange?number1:number0));
    }
    
    // Disable power button sleep on PowerMacs, Cubes, and iMacs
    // Default is false == power button causes sleep
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMSleepOnPowerButtonKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMSettingSleepOnPowerButtonKey),
                                     (p->fSleepOnPowerButton?kCFBooleanFalse:kCFBooleanTrue));
    }
    
    // Wakeup on clamshell open
    // Default is true == wakeup when the clamshell opens
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMWakeOnClamshellKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMSettingWakeOnClamshellKey),
                                     (p->fWakeOnClamshell?number1:number0));
    }
    
    // Mobile Motion Module
    // Defaults to on
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMMobileMotionModuleKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMSettingMobileMotionModuleKey),
                                     (p->fMobileMotionModule?number1:number0));
    }
    
    /*
     * GPU
     */
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMGPUSwitchKey), providing_power, _supportedCached))
    {
        num = CFNumberCreate(0, kCFNumberIntType, &p->fGPU);
        if (num) {
            IORegistryEntrySetCFProperty(PMRootDomain,
                                         CFSTR(kIOPMGPUSwitchKey),
                                         num);
            CFRelease(num);
        }
    }
    
    // DeepSleepEnable
    // Defaults to on
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMDeepSleepEnabledKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMDeepSleepEnabledKey),
                                     (p->fDeepSleepEnable?kCFBooleanTrue:kCFBooleanFalse));
    }
    
    // DeepSleepDelay
    // In seconds
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMDeepSleepDelayKey), providing_power, _supportedCached))
    {
        num = CFNumberCreate(0, kCFNumberIntType, &p->fDeepSleepDelay);
        if (num) {
            IORegistryEntrySetCFProperty(PMRootDomain,
                                         CFSTR(kIOPMDeepSleepDelayKey),
                                         num);
            CFRelease(num);
        }
    }
    
    // AutoPowerOffEnable
    // Defaults to on
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMAutoPowerOffEnabledKey), providing_power, _supportedCached))
    {
        IORegistryEntrySetCFProperty(PMRootDomain,
                                     CFSTR(kIOPMAutoPowerOffEnabledKey),
                                     (p->fAutoPowerOffEnable?kCFBooleanTrue:kCFBooleanFalse));
    }
    
    // AutoPowerOffDelay
    // In seconds
    if( !removeUnsupportedSettings
       || IOPMFeatureIsAvailableWithSupportedTable(CFSTR(kIOPMAutoPowerOffDelayKey), providing_power, _supportedCached))
    {
        num = CFNumberCreate(0, kCFNumberIntType, &p->fAutoPowerOffDelay);
        if (num) {
            IORegistryEntrySetCFProperty(PMRootDomain,
                                         CFSTR(kIOPMAutoPowerOffDelayKey),
                                         num);
            CFRelease(num);
        }
    }
    
#ifndef __I_AM_PMSET__
    if ( !_platformSleepServiceSupport && !_platformBackgroundTaskSupport)
    {
        bool ssupdate, btupdate, pnupdate;
        
        // On legacy systems, IOPPF publishes PowerNap support using
        // the kIOPMDarkWakeBackgroundTaskKey  and/or
        // kIOPMSleepServicesKey
        btupdate = IOPMFeatureIsAvailableWithSupportedTable(
                                                            CFSTR(kIOPMDarkWakeBackgroundTaskKey),
                                                            providing_power, _supportedCached);
        ssupdate = IOPMFeatureIsAvailableWithSupportedTable(
                                                            CFSTR(kIOPMSleepServicesKey),
                                                            providing_power, _supportedCached);
        
        // But going forward (late 2012 machines and beyond), IOPPF will publish
        // PowerNap support as a PM feature using the kIOPMPowerNapSupportedKey
        pnupdate = IOPMFeatureIsAvailableWithSupportedTable(
                                                            CFSTR(kIOPMPowerNapSupportedKey),
                                                            providing_power, _supportedCached);
        
        // We have to check for one of either 'legacy' or 'modern' PowerNap
        // support and configure BT assertion and other powerd-internal PowerNap
        // settings accordingly
        if (ssupdate || btupdate || pnupdate) {
            _platformSleepServiceSupport = ssupdate;
            _platformBackgroundTaskSupport = btupdate;
            configAssertionType(kBackgroundTaskType, false);
            mt2EvaluateSystemSupport();
        }
    }
#endif
    
    if (useSettings)
    {
        bool isDesktop = (0 == _batteryCount());
        ProcessHibernateSettings(useSettings, p->fDeepSleepEnable, isDesktop, PMRootDomain);
    }
    
exit:
    if (number0) {
        CFRelease(number0);
    }
    if (number1) {
        CFRelease(number1);
    }
    if (IO_OBJECT_NULL != PM_connection) {
        IOServiceClose(PM_connection);
    }
    if (_supportedCached) {
        CFRelease(_supportedCached);
    }
    return;
}

__private_extern__ IOReturn ActivatePMSettings(
    CFDictionaryRef                 useSettings,
    bool                            removeUnsupportedSettings)
{
    IOPMAggressivenessFactors       theFactors;

    if(!isA_CFDictionary(useSettings))
    {
        return kIOReturnBadArgument;
    }

    // Activate settings by sending them to the multiple owning drivers kernel
    getAggressivenessFactorsFromProfile(useSettings, &theFactors);

    sendEnergySettingsToKernel(useSettings, removeUnsupportedSettings, &theFactors);

#ifndef __I_AM_PMSET__
    evalAllUserActivityAssertions(theFactors.fMinutesToDim);
    evalAllNetworkAccessAssertions();
#endif

    return kIOReturnSuccess;
}


/*****************************************************************************/
/*****************************************************************************/

__private_extern__ CFCalendarRef        _gregorian(void)
{
    static CFCalendarRef g = NULL;
    if (!g) {
        g = CFCalendarCreateWithIdentifier(NULL, kCFGregorianCalendar);
    }
    return g;
}

/***************************************************************************/
__private_extern__  asl_object_t open_pm_asl_store(char *store)
{
    asl_object_t        response = NULL;
    size_t              endMessageID;

    if (!store) {
        return NULL;
    }
    asl_object_t query = asl_new(ASL_TYPE_LIST);
    if (query != NULL)
    {
		asl_object_t cq = asl_new(ASL_TYPE_QUERY);
		if (cq != NULL)
		{
			asl_set_query(cq, ASL_KEY_FACILITY, kPMFacility, ASL_QUERY_OP_EQUAL);
			asl_append(query, cq);
			asl_release(cq);

			asl_object_t pmstore = asl_open_path(store, 0);
			if (pmstore != NULL) {
				response = asl_match(pmstore, query, &endMessageID, 0, 0, 0, ASL_MATCH_DIRECTION_FORWARD);
			}
			asl_release(pmstore);
		}
		asl_release(query);
    }

    return response;
}

