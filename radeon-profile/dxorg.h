// copyright marazmista @ 29.03.2014

// class for support open source driver for Radeon cards //

#ifndef DXORG_H
#define DXORG_H

#include "globalStuff.h"

#include <QString>
#include <QList>
#include <QTreeWidgetItem>

class dXorg
{
public:
    dXorg() {}

    static globalStuff::gpuClocksStruct getClocks();
    static float getTemperature();
    static QList<QTreeWidgetItem *> getCardConnectors();
    static QStringList getGLXInfo(QString gpuName);
    static QList<QTreeWidgetItem *> getModuleInfo();
    static QString getCurrentPowerProfile();
    static QStringList detectCards();
    static void setPowerProfile(globalStuff::powerProfiles _newPowerProfile);
    static void setForcePowerLevel(globalStuff::forcePowerLevels);
    static void figureOutGpuDataFilePaths(QString gpuName);
    static void configure(QString gpuName);

    static globalStuff::driverFeatures xrogDriverFeatures;
private:
    enum powerMethod {
        DPM = 0,  // kernel >= 3.11
        PROFILE = 1,  // kernel <3.11 or dpm disabled
        PM_UNKNOWN = 2
    };

    enum tempSensor {
        SYSFS_HWMON = 0, // try to read temp from /sys/class/hwmonX/device/tempX_input
        CARD_HWMON, // try to read temp from /sys/class/drm/cardX/device/hwmon/hwmonX/temp1_input
        PCI_SENSOR,  // PCI Card, 'radeon-pci' label on sensors output
        MB_SENSOR,  // Card in motherboard, 'VGA' label on sensors output
        TS_UNKNOWN
    };

    static struct driverFilePaths {
    QString powerMethodFilePath, profilePath, dpmStateFilePath ,
    clocksPath , forcePowerLevelFilePath , sysfsHwmonPath, moduleParamsPath ;
    } filePaths;

    static QString findSysfsHwmonForGPU();
    static powerMethod getPowerMethod();
    static tempSensor testSensor();
    static int sensorsGPUtempIndex;
    static  dXorg::tempSensor currentTempSensor;
    static dXorg::powerMethod currentPowerMethod;

    static void setNewValue(const QString &filePath, const QString &newValue);
    static QString findSysFsHwmonForGpu();
};



#endif // DXORG_H
