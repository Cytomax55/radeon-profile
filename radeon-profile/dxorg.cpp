// copyright marazmista @ 29.03.2014

#include "dxorg.h"
#include <QFile>
#include <QTextStream>

// define static members //
dXorg::tempSensor dXorg::currentTempSensor = dXorg::TS_UNKNOWN;
dXorg::powerMethod dXorg::currentPowerMethod;
int dXorg::sensorsGPUtempIndex;

dXorg::driverFilePaths dXorg::filePaths;
globalStuff::driverFeatures dXorg::xrogDriverFeatures;
// end //

void dXorg::configure(QString gpuName) {
    figureOutGpuDataFilePaths(gpuName);
    currentTempSensor = testSensor();
    currentPowerMethod = getPowerMethod();
}

void dXorg::figureOutGpuDataFilePaths(QString gpuName) {
    filePaths.powerMethodFilePath = "/sys/class/drm/"+gpuName+"/device/power_method",
            filePaths.profilePath = "/sys/class/drm/"+gpuName+"/device/power_profile",
            filePaths.dpmStateFilePath = "/sys/class/drm/"+gpuName+"/device/power_dpm_state",
            filePaths.clocksPath = "/sys/kernel/debug/dri/"+QString(gpuName.at(gpuName.length()-1))+"/radeon_pm_info", // this path contains only index
            filePaths.forcePowerLevelFilePath = "/sys/class/drm/"+gpuName+"/device/power_dpm_force_performance_level",
            filePaths.moduleParamsPath = "/sys/class/drm/"+gpuName+"/device/driver/module/holders/radeon/parameters/";

    QString hwmonDevice = globalStuff::grabSystemInfo("ls /sys/class/drm/"+gpuName+"/device/hwmon/")[0]; // look for hwmon devices in card dir
    filePaths.sysfsHwmonPath = "/sys/class/drm/"+gpuName+"/device/hwmon/" + QString((hwmonDevice.isEmpty()) ? "hwmon0" : hwmonDevice) + "/temp1_input";
}

globalStuff::gpuClocksStruct dXorg::getClocks() {
    QFile clocksFile(filePaths.clocksPath);
    globalStuff::gpuClocksStruct tData;
    tData.coreClk = tData.coreVolt = tData.memClk = tData.memVolt = tData.powerLevel = tData.uvdCClk = tData.uvdDClk = -1;

    if (clocksFile.open(QIODevice::ReadOnly)) {  // check for debugfs access
        QStringList out = QString(clocksFile.readAll()).split('\n');

        switch (currentPowerMethod) {
        case DPM: {
            QRegExp rx;

            rx.setPattern("power\\slevel\\s\\d");
            rx.indexIn(out[1]);
            if (!rx.cap(0).isEmpty())
                tData.powerLevel = rx.cap(0).split(' ')[2].toShort();

            rx.setPattern("sclk:\\s\\d+");
            rx.indexIn(out[1]);
            if (!rx.cap(0).isEmpty())
                tData.coreClk = rx.cap(0).split(' ',QString::SkipEmptyParts)[1].toDouble() / 100;

            rx.setPattern("mclk:\\s\\d+");
            rx.indexIn(out[1]);
            if (!rx.cap(0).isEmpty())
                tData.memClk = rx.cap(0).split(' ',QString::SkipEmptyParts)[1].toDouble() / 100;

            rx.setPattern("vclk:\\s\\d+");
            rx.indexIn(out[0]);
            if (!rx.cap(0).isEmpty()) {
                tData.uvdCClk = rx.cap(0).split(' ',QString::SkipEmptyParts)[1].toDouble() / 100;
                tData.uvdCClk  = (tData.uvdCClk  == 0) ? -1 :  tData.uvdCClk;
            }

            rx.setPattern("dclk:\\s\\d+");
            rx.indexIn(out[0]);
            if (!rx.cap(0).isEmpty()) {
                tData.uvdDClk = rx.cap(0).split(' ',QString::SkipEmptyParts)[1].toDouble() / 100;
                tData.uvdDClk = (tData.uvdDClk == 0) ? -1 : tData.uvdDClk;
            }

            rx.setPattern("vddc:\\s\\d+");
            rx.indexIn(out[1]);
            if (!rx.cap(0).isEmpty())
                tData.coreVolt = rx.cap(0).split(' ',QString::SkipEmptyParts)[1].toDouble();

            rx.setPattern("vddci:\\s\\d+");
            rx.indexIn(out[1]);
            if (!rx.cap(0).isEmpty())
                tData.memVolt = rx.cap(0).split(' ',QString::SkipEmptyParts)[1].toDouble();

            return tData;
            break;
        }
        case PROFILE: {
            for (int i=0; i< out.count(); i++) {
                switch (i) {
                case 1: {
                    if (out[i].contains("current engine clock")) {
                        tData.coreClk = QString().setNum(out[i].split(' ',QString::SkipEmptyParts,Qt::CaseInsensitive)[3].toFloat() / 1000).toDouble();
                        break;
                    }
                };
                case 3: {
                    if (out[i].contains("current memory clock")) {
                        tData.memClk = QString().setNum(out[i].split(' ',QString::SkipEmptyParts,Qt::CaseInsensitive)[3].toFloat() / 1000).toDouble();
                        break;
                    }
                }
                case 4: {
                    if (out[i].contains("voltage")) {
                        tData.coreVolt = QString().setNum(out[i].split(' ',QString::SkipEmptyParts,Qt::CaseInsensitive)[1].toFloat()).toDouble();
                        break;
                    }
                }
                }
            }
            return tData;
            break;
        }
        case PM_UNKNOWN: {
            return tData;
            break;
        }
        }
    }
    return tData;
}

float dXorg::getTemperature() {
    QString temp;

    switch (currentTempSensor) {
    case SYSFS_HWMON:
    case CARD_HWMON: {
        QFile hwmon(filePaths.sysfsHwmonPath);
        hwmon.open(QIODevice::ReadOnly);
        temp = hwmon.readLine(20);
        hwmon.close();
        return temp.toDouble() / 1000;
        break;
    }
    case PCI_SENSOR: {
        QStringList out = globalStuff::grabSystemInfo("sensors");
        temp = out[sensorsGPUtempIndex+2].split(" ",QString::SkipEmptyParts)[1].remove("+").remove("C").remove("°");
        return temp.toDouble();
        break;
    }
    case MB_SENSOR: {
        QStringList out = globalStuff::grabSystemInfo("sensors");
        temp = out[sensorsGPUtempIndex].split(" ",QString::SkipEmptyParts)[1].remove("+").remove("C").remove("°");
        return temp.toDouble();
        break;
    }
    case TS_UNKNOWN: {
        return -1;
        break;
    }
    }
}

QList<QTreeWidgetItem *> dXorg::getCardConnectors() {
    QList<QTreeWidgetItem *> cardConnectorsList;
    QStringList out = globalStuff::grabSystemInfo("xrandr -q --verbose"), screens;
    screens = out.filter(QRegExp("Screen\\s\\d"));
    for (int i = 0; i < screens.count(); i++) {
        QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << screens[i].split(':')[0] << screens[i].split(",")[1].remove(" current "));
        cardConnectorsList.append(item);
    }
    cardConnectorsList.append(new QTreeWidgetItem(QStringList() << "------"));

    for(int i = 0; i < out.size(); i++) {
        if(!out[i].startsWith("\t") && out[i].contains("connected")) {
            QString connector = out[i].split(' ')[0],
                    status = out[i].split(' ')[1],
                    res = out[i].split(' ')[2].split('+')[0];

            if(status == "connected") {
                QString monitor, edid = monitor = "";

                // find EDID
                for (int i = out.indexOf(QRegExp(".+EDID.+"))+1; i < out.count(); i++)
                    if (out[i].startsWith(("\t\t")))
                        edid += out[i].remove("\t\t");
                    else
                        break;

                // Parse EDID
                // See http://en.wikipedia.org/wiki/Extended_display_identification_data#EDID_1.3_data_format
                if(edid.size() >= 256) {
                    QStringList hex;
                    bool found = false, ok = true;
                    int i2 = 108;

                    for(int i3 = 0; i3 < 4; i3++) {
                        if(edid.mid(i2, 2).toInt(&ok, 16) == 0 && ok &&
                                edid.mid(i2 + 2, 2).toInt(&ok, 16) == 0) {
                            // Other Monitor Descriptor found
                            if(ok && edid.mid(i2 + 6, 2).toInt(&ok, 16) == 0xFC && ok) {
                                // Monitor name found
                                for(int i4 = i2 + 10; i4 < i2 + 34; i4 += 2)
                                    hex << edid.mid(i4, 2);
                                found = true;
                                break;
                            }
                        }
                        if(!ok)
                            break;
                        i2 += 36;
                    }
                    if(ok && found) {
                        // Hex -> String
                        for(i2 = 0; i2 < hex.size(); i2++) {
                            monitor += QString(hex[i2].toInt(&ok, 16));
                            if(!ok)
                                break;
                        }

                        if(ok)
                            monitor = " (" + monitor.left(monitor.indexOf('\n')) + ")";
                    }
                }
                status += monitor + " @ " + QString((res.contains('x')) ? res : "unknown");
            }

            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << connector << status);
            cardConnectorsList.append(item);
        }
    }
    return cardConnectorsList;
}


dXorg::powerMethod dXorg::getPowerMethod() {
    QFile powerMethodFile(filePaths.powerMethodFilePath);
    if (powerMethodFile.open(QIODevice::ReadOnly)) {
        QString s = powerMethodFile.readLine(20);

        if (s.contains("dpm",Qt::CaseInsensitive))
            return DPM;
        else if (s.contains("profile",Qt::CaseInsensitive))
            return PROFILE;
        else
            return  PM_UNKNOWN;
    } else
        return PM_UNKNOWN;
}

dXorg::tempSensor dXorg::testSensor() {
    QFile hwmon(filePaths.sysfsHwmonPath);

    // first method, try read temp from sysfs in card dir (path from figureOutGPUDataPaths())
    if (hwmon.open(QIODevice::ReadOnly)) {
        if (!QString(hwmon.readLine(20)).isEmpty())
            return CARD_HWMON;
    } else {

        // second method, try find in system hwmon dir for file labeled VGA_TEMP
        filePaths.sysfsHwmonPath = findSysfsHwmonForGPU();
        if (!filePaths.sysfsHwmonPath.isEmpty())
            return SYSFS_HWMON;

        // if above fails, use lm_sensors
        QStringList out = globalStuff::grabSystemInfo("sensors");
        if (out.indexOf(QRegExp("radeon-pci.+")) != -1) {
            sensorsGPUtempIndex = out.indexOf(QRegExp("radeon-pci.+"));  // in order to not search for it again in timer loop
            return PCI_SENSOR;
        }
        else if (out.indexOf(QRegExp("VGA_TEMP.+")) != -1) {
            sensorsGPUtempIndex = out.indexOf(QRegExp("VGA_TEMP.+"));
            return MB_SENSOR;
        } else
            return TS_UNKNOWN;
    }

}

QString dXorg::findSysfsHwmonForGPU() {
    QStringList hwmonDev = globalStuff::grabSystemInfo("ls /sys/class/hwmon/");
    for (int i = 0; i < hwmonDev.count(); i++) {
        QStringList temp = globalStuff::grabSystemInfo("ls /sys/class/hwmon/"+hwmonDev[i]+"/device/").filter("label");

        for (int o = 0; o < temp.count(); o++) {
            QFile f("/sys/class/hwmon/"+hwmonDev[i]+"/device/"+temp[o]);
            if (f.open(QIODevice::ReadOnly))
                if (f.readLine(20).contains("VGA_TEMP")) {
                    f.close();
                    return f.fileName().replace("label", "input");
                }
        }
    }
    return "";
}

QStringList dXorg::getGLXInfo(QString gpuName) {
    QStringList data, gpus = globalStuff::grabSystemInfo("lspci").filter("Radeon",Qt::CaseInsensitive);
    gpus.removeAt(gpus.indexOf(QRegExp(".+Audio.+"))); //remove radeon audio device

    // loop for multi gpu
    for (int i = 0; i < gpus.count(); i++)
        data << "VGA:"+gpus[i].split(":",QString::SkipEmptyParts)[2];

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("DRI_PRIME",gpuName.at(gpuName.length()-1));

    QStringList driver = globalStuff::grabSystemInfo("xdriinfo",env).filter("Screen 0:",Qt::CaseInsensitive);
    if (!driver.isEmpty())  // because of segfault when no xdriinfo
        data << "Driver:"+ driver.filter("Screen 0:",Qt::CaseInsensitive)[0].split(":",QString::SkipEmptyParts)[1];

    data << globalStuff::grabSystemInfo("glxinfo",env).filter(QRegExp("direct|OpenGL.+:.+"));
    return data;
}

QList<QTreeWidgetItem *> dXorg::getModuleInfo() {
    QList<QTreeWidgetItem *> data;
    QStringList modInfo = globalStuff::grabSystemInfo("modinfo -p radeon");
    modInfo.sort();

    for (int i =0; i < modInfo.count(); i++) {
        if (modInfo[i].contains(":")) {
            // show nothing in case of an error
            if (modInfo[i].startsWith("modinfo: ERROR: ")) {
                continue;
            }
            // read module param name and description from modinfo command
            QString modName = modInfo[i].split(":",QString::SkipEmptyParts)[0],
                    modDesc = modInfo[i].split(":",QString::SkipEmptyParts)[1],
                    modValue;

            // read current param values
            QFile mp(filePaths.moduleParamsPath+modName);
            modValue = (mp.open(QIODevice::ReadOnly)) ?  modValue = mp.readLine(20) : "unknown";

            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList() << modName.left(modName.indexOf('\n')) << modValue.left(modValue.indexOf('\n')) << modDesc.left(modDesc.indexOf('\n')));
            data.append(item);
            mp.close();
        }
    }
    return data;
}

QStringList dXorg::detectCards() {
    QStringList data;
    QStringList out = globalStuff::grabSystemInfo("ls /sys/class/drm/").filter("card");
    for (char i = 0; i < out.count(); i++) {
        QFile f("/sys/class/drm/"+out[i]+"/device/uevent");
        if (f.open(QIODevice::ReadOnly)) {
            if (f.readLine(50).contains("DRIVER=radeon"))
                data.append(f.fileName().split('/')[4]);
        }
    }
    return data;
}

QString dXorg::getCurrentPowerProfile() {
    QFile forceProfile(filePaths.forcePowerLevelFilePath);
    QString pp, err = "err";

    switch (currentPowerMethod) {
    case DPM: {
        QFile profile(filePaths.dpmStateFilePath);
        if (profile.open(QIODevice::ReadOnly)) {
            pp = profile.readLine(13);
            if (forceProfile.open(QIODevice::ReadOnly))
                pp += " | " + forceProfile.readLine(5);
        } else
            pp = err;
        break;
    }
    case PROFILE: {
        QFile profile(filePaths.profilePath);
        if (profile.open(QIODevice::ReadOnly))
            pp = profile.readLine(13);
        break;
    }
    case PM_UNKNOWN: {
        pp = err;
        break;
    }
    }
    return pp.remove('\n');
}

void dXorg::setNewValue(const QString &filePath, const QString &newValue)
{
    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream stream(&file);
    stream << newValue + "\n";
    file.flush();
    file.close();
}

void dXorg::setPowerProfile(globalStuff::powerProfiles _newPowerProfile) {
    QString newValue;
    switch (_newPowerProfile) {
    case globalStuff::BATTERY:
        newValue = "battery";
        break;
    case globalStuff::BALANCED:
        newValue = "balanced";
        break;
    case globalStuff::PERFORMANCE:
        newValue = "performance";
        break;
    default: break;
    }

    setNewValue(filePaths.dpmStateFilePath,newValue);
}

void dXorg::setForcePowerLevel(globalStuff::forcePowerLevels _newForcePowerLevel) {
    QString newValue;
    switch (_newForcePowerLevel) {
    case globalStuff::F_AUTO:
        newValue = "auto";
        break;
    case globalStuff::F_HIGH:
        newValue = "high";
        break;
    case globalStuff::F_LOW:
        newValue = "low";
    default:
        break;
    }
    setNewValue(filePaths.forcePowerLevelFilePath, newValue);
}
