/* $Id$ */
/** @file
 * OpenHuizeBox - UIMachineSettingsOhbIdentity class implementation.
 * See header file for the high-level design and field mapping.
 */

/* Qt includes: */
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIMachineSettingsOhbIdentity.h"
#include "UIErrorString.h"
#include "UITranslationEventListener.h"

/* COM includes: */
#include "CMachine.h"
#include "CNetworkAdapter.h"
#include "CPlatform.h"
#include "CPlatformX86.h"


/*********************************************************************************************************************************
*   Cache data struct                                                                                                            *
*********************************************************************************************************************************/

/** Identity page data backing struct. One value per editable widget. */
struct UIDataSettingsMachineOhbIdentity
{
    UIDataSettingsMachineOhbIdentity()
        : m_fStealthMaster(false)
        , m_fHideDescTables(false)
        , m_u64FakeIdtrBase(0)
        , m_u16FakeIdtrLimit(0)         /* default zero so CacheData() is the true zero state;
                                         * UI placeholder (0x0FFF) lives only in getFromCache(). */
        , m_u64FakeGdtrBase(0)
        , m_u16FakeGdtrLimit(0)         /* same as above (was 0x007F). */
        , m_i64TscOffsetBias(0)
        , m_enmMacMode(0)
    {}

    bool operator==(const UIDataSettingsMachineOhbIdentity &o) const
    {
        return    m_strProfileLast == o.m_strProfileLast
               && m_fStealthMaster == o.m_fStealthMaster
               && m_fHideDescTables == o.m_fHideDescTables
               && m_u64FakeIdtrBase == o.m_u64FakeIdtrBase
               && m_u16FakeIdtrLimit == o.m_u16FakeIdtrLimit
               && m_u64FakeGdtrBase == o.m_u64FakeGdtrBase
               && m_u16FakeGdtrLimit == o.m_u16FakeGdtrLimit
               && m_i64TscOffsetBias == o.m_i64TscOffsetBias
               && m_strSysVendor == o.m_strSysVendor
               && m_strSysProduct == o.m_strSysProduct
               && m_strSysVersion == o.m_strSysVersion
               && m_strSysSku == o.m_strSysSku
               && m_strSysFamily == o.m_strSysFamily
               && m_strSysSerial == o.m_strSysSerial
               && m_strBoardVendor == o.m_strBoardVendor
               && m_strBoardProduct == o.m_strBoardProduct
               && m_strBoardVersion == o.m_strBoardVersion
               && m_strBoardSerial == o.m_strBoardSerial
               && m_strChassisVendor == o.m_strChassisVendor
               && m_strChassisVersion == o.m_strChassisVersion
               && m_strChassisType == o.m_strChassisType
               && m_strBiosVendor == o.m_strBiosVendor
               && m_strBiosVersion == o.m_strBiosVersion
               && m_strBiosRelease == o.m_strBiosRelease
               && m_strProcMfg == o.m_strProcMfg
               && m_strAcpiOemId == o.m_strAcpiOemId
               && m_strAcpiTableId == o.m_strAcpiTableId
               && m_strAcpiCreatorId == o.m_strAcpiCreatorId
               && m_strCpuidBrand == o.m_strCpuidBrand
               && m_strDiskModel == o.m_strDiskModel
               && m_strDiskSerial == o.m_strDiskSerial
               && m_strDiskFirmware == o.m_strDiskFirmware
               && m_enmMacMode == o.m_enmMacMode
               && m_strMacPoolOui == o.m_strMacPoolOui
               && m_strMacCustomOui == o.m_strMacCustomOui
               && m_strMacFull == o.m_strMacFull;
    }
    bool operator!=(const UIDataSettingsMachineOhbIdentity &o) const { return !(*this == o); }

    /** Name of the last-loaded profile JSON (cosmetic, shown under combobox). */
    QString  m_strProfileLast;

    /** VMM stealth (HM/Ohb* CFGM keys). */
    bool     m_fStealthMaster;
    bool     m_fHideDescTables;
    quint64  m_u64FakeIdtrBase;
    quint16  m_u16FakeIdtrLimit;
    quint64  m_u64FakeGdtrBase;
    quint16  m_u16FakeGdtrLimit;
    qint64   m_i64TscOffsetBias;

    /** SMBIOS / DMI strings (Devices/pcbios/0/Config/Dmi*). */
    QString m_strSysVendor, m_strSysProduct, m_strSysVersion, m_strSysSku, m_strSysFamily, m_strSysSerial;
    QString m_strBoardVendor, m_strBoardProduct, m_strBoardVersion, m_strBoardSerial;
    QString m_strChassisVendor, m_strChassisVersion, m_strChassisType;
    QString m_strBiosVendor, m_strBiosVersion, m_strBiosRelease;
    QString m_strProcMfg;

    /** ACPI (Devices/acpi/0/Config/Acpi*). */
    QString m_strAcpiOemId, m_strAcpiTableId, m_strAcpiCreatorId;

    /** CPUID brand string (auto-encoded into leaves 0x80000002-04). */
    QString m_strCpuidBrand;

    /** Disk identifiers (Devices/ahci/0/LUN#0/AttachedDriver/Config/*). */
    QString m_strDiskModel, m_strDiskSerial, m_strDiskFirmware;

    /** MAC mode: 0 = pool, 1 = custom OUI + det tail, 2 = full custom MAC. */
    int     m_enmMacMode;
    QString m_strMacPoolOui;       /* e.g. "00:22:19" */
    QString m_strMacCustomOui;     /* 6 hex chars */
    QString m_strMacFull;          /* 12 hex chars */
};


/*********************************************************************************************************************************
*   Local extradata key helpers                                                                                                  *
*********************************************************************************************************************************/

namespace
{
    /* Extradata keys we read / write. Centralising as constants saves typo grief
     * and lets us add new fields in one place without grep-and-replace. */
    const char *kStealthMaster      = "VBoxInternal/HM/OhbStealth";
    const char *kHideDescTables     = "VBoxInternal/HM/OhbHideDescTables";
    const char *kFakeIdtrBase       = "VBoxInternal/HM/OhbFakeIdtrBase";
    const char *kFakeIdtrLimit      = "VBoxInternal/HM/OhbFakeIdtrLimit";
    const char *kFakeGdtrBase       = "VBoxInternal/HM/OhbFakeGdtrBase";
    const char *kFakeGdtrLimit      = "VBoxInternal/HM/OhbFakeGdtrLimit";
    const char *kTscOffsetBias      = "VBoxInternal/HM/OhbTscOffsetBias";

    const char *kDmiSysVendor       = "VBoxInternal/Devices/pcbios/0/Config/DmiSystemVendor";
    const char *kDmiSysProduct      = "VBoxInternal/Devices/pcbios/0/Config/DmiSystemProduct";
    const char *kDmiSysVersion      = "VBoxInternal/Devices/pcbios/0/Config/DmiSystemVersion";
    const char *kDmiSysSku          = "VBoxInternal/Devices/pcbios/0/Config/DmiSystemSKU";
    const char *kDmiSysFamily       = "VBoxInternal/Devices/pcbios/0/Config/DmiSystemFamily";
    const char *kDmiSysSerial       = "VBoxInternal/Devices/pcbios/0/Config/DmiSystemSerial";
    const char *kDmiBoardVendor     = "VBoxInternal/Devices/pcbios/0/Config/DmiBoardVendor";
    const char *kDmiBoardProduct    = "VBoxInternal/Devices/pcbios/0/Config/DmiBoardProduct";
    const char *kDmiBoardVersion    = "VBoxInternal/Devices/pcbios/0/Config/DmiBoardVersion";
    const char *kDmiBoardSerial     = "VBoxInternal/Devices/pcbios/0/Config/DmiBoardSerial";
    const char *kDmiChassisVendor   = "VBoxInternal/Devices/pcbios/0/Config/DmiChassisVendor";
    const char *kDmiChassisVersion  = "VBoxInternal/Devices/pcbios/0/Config/DmiChassisVersion";
    const char *kDmiChassisType     = "VBoxInternal/Devices/pcbios/0/Config/DmiChassisType";
    const char *kDmiChassisAssetTag = "VBoxInternal/Devices/pcbios/0/Config/DmiChassisAssetTag";
    const char *kDmiBiosVendor      = "VBoxInternal/Devices/pcbios/0/Config/DmiBIOSVendor";
    const char *kDmiBiosVersion     = "VBoxInternal/Devices/pcbios/0/Config/DmiBIOSVersion";
    const char *kDmiBiosRelease     = "VBoxInternal/Devices/pcbios/0/Config/DmiBIOSReleaseDate";
    const char *kDmiProcMfg         = "VBoxInternal/Devices/pcbios/0/Config/DmiProcManufacturer";

    const char *kAcpiOemId          = "VBoxInternal/Devices/acpi/0/Config/AcpiOemId";
    const char *kAcpiTableId        = "VBoxInternal/Devices/acpi/0/Config/AcpiOemTabId";
    const char *kAcpiCreatorId      = "VBoxInternal/Devices/acpi/0/Config/AcpiCreatorId";
    const char *kAcpiCpuTableId     = "VBoxInternal/Devices/acpi/0/Config/AcpiCpuTableId";

    const char *kCpuidBrand         = "OpenHuizeBox/Identity/CpuidBrand";
    const char *kLastProfile        = "OpenHuizeBox/Identity/LastProfile";
    /* Wizard-stamped intent keys (written by UIWizardNewVM::createVM, read here): */
    const char *kWizardStealthMode  = "OpenHuizeBox/Identity/StealthMode";
    const char *kWizardApplied      = "OpenHuizeBox/Identity/WizardApplied";

    /* Disk identity strings are read by DevAHCI from Config/Port0/* at ATA
     * IDENTIFY time. The DrvVD LUN#X/AttachedDriver/Config path was a dead
     * end (DrvVD does not surface identity to the guest), so writes there
     * had no visible effect in the guest. */
    const char *kDiskModel          = "VBoxInternal/Devices/ahci/0/Config/Port0/ModelNumber";
    const char *kDiskSerial         = "VBoxInternal/Devices/ahci/0/Config/Port0/SerialNumber";
    const char *kDiskFirmware       = "VBoxInternal/Devices/ahci/0/Config/Port0/FirmwareRevision";
    /* CD-ROM identity at Port 1: 8-byte vendor, 16-byte product, 4-byte rev. */
    const char *kCdromVendor        = "VBoxInternal/Devices/ahci/0/Config/Port1/ATAPIVendorId";
    const char *kCdromProduct       = "VBoxInternal/Devices/ahci/0/Config/Port1/ATAPIProductId";
    const char *kCdromRevision      = "VBoxInternal/Devices/ahci/0/Config/Port1/ATAPIRevision";

    /* PCI identity override on the VMMDev device (eliminates the residual
     * VEN_80EE&DEV_CAFE entry in Device Manager). Use a vendor that has no
     * Win10 inbox driver (e.g. 0x1B36 Red Hat) so the device shows as
     * generic "Unknown" rather than triggering an inbox driver install. */
    const char *kVmmDevPciVendor    = "VBoxInternal/Devices/VMMDev/0/Config/PciVendorId";
    const char *kVmmDevPciDevice    = "VBoxInternal/Devices/VMMDev/0/Config/PciDeviceId";
    const char *kVmmDevPciSubVendor = "VBoxInternal/Devices/VMMDev/0/Config/PciSubsysVendorId";
    const char *kVmmDevPciSubDevice = "VBoxInternal/Devices/VMMDev/0/Config/PciSubsysDeviceId";

    const char *kMacMode            = "OpenHuizeBox/Identity/MacMode";
    const char *kMacPoolOui         = "OpenHuizeBox/Identity/MacPoolOui";
    const char *kMacCustomOui       = "OpenHuizeBox/Identity/MacCustomOui";
    const char *kMacFull            = "OpenHuizeBox/Identity/MacFull";

    /* Helper: get an extradata string with an empty-string default.
     * CMachine::GetExtraData is declared non-const in the COM wrapper, so we
     * take the machine by non-const reference. */
    QString readExtra(CMachine &machine, const char *pszKey)
    {
        if (machine.isNull()) return QString();
        return machine.GetExtraData(QString::fromLatin1(pszKey));
    }

    /* Helper: write an extradata key. Empty value -> clear the key entirely
     * (which makes VBox fall back to its stock default for the field). */
    void writeExtra(CMachine &machine, const char *pszKey, const QString &strValue)
    {
        if (machine.isNull()) return;
        machine.SetExtraData(QString::fromLatin1(pszKey), strValue);
    }
}


/*********************************************************************************************************************************
*   ctor / dtor                                                                                                                  *
*********************************************************************************************************************************/

UIMachineSettingsOhbIdentity::UIMachineSettingsOhbIdentity()
    : m_pCache(0)
    , m_pScrollArea(0)
    , m_pCmbStealthLevel(0), m_pBtnApplyStealthLevel(0)
    , m_pComboProfile(0), m_pLblProfileLast(0), m_pBtnLoadProfile(0)
    , m_pChkStealthMaster(0), m_pChkHideDescTables(0)
    , m_pEdtFakeIdtrBase(0), m_pEdtFakeIdtrLimit(0)
    , m_pEdtFakeGdtrBase(0), m_pEdtFakeGdtrLimit(0)
    , m_pEdtTscOffsetBias(0)
    , m_pEdtSysVendor(0), m_pEdtSysProduct(0), m_pEdtSysVersion(0)
    , m_pEdtSysSku(0), m_pEdtSysFamily(0), m_pEdtSysSerial(0)
    , m_pEdtBoardVendor(0), m_pEdtBoardProduct(0), m_pEdtBoardVersion(0), m_pEdtBoardSerial(0)
    , m_pEdtChassisVendor(0), m_pEdtChassisVersion(0), m_pCboChassisType(0)
    , m_pEdtBiosVendor(0), m_pEdtBiosVersion(0), m_pEdtBiosRelease(0)
    , m_pEdtProcMfg(0)
    , m_pEdtAcpiOemId(0), m_pEdtAcpiTableId(0), m_pEdtAcpiCreatorId(0)
    , m_pEdtCpuidBrand(0)
    , m_pEdtDiskModel(0), m_pEdtDiskSerial(0), m_pEdtDiskFirmware(0)
    , m_pRadMacPool(0), m_pRadMacCustomOui(0), m_pRadMacFull(0)
    , m_pCboMacPool(0), m_pEdtMacCustomOui(0), m_pEdtMacFull(0)
    , m_pLblMacEffective(0)
    , m_pBtnShowEffective(0), m_pBtnRestoreDefaults(0)
{
    prepare();
}

UIMachineSettingsOhbIdentity::~UIMachineSettingsOhbIdentity()
{
    cleanup();
}


/*********************************************************************************************************************************
*   UISettingsPageMachine implementation                                                                                         *
*********************************************************************************************************************************/

bool UIMachineSettingsOhbIdentity::changed() const
{
    return m_pCache ? m_pCache->wasChanged() : false;
}

void UIMachineSettingsOhbIdentity::loadToCacheFrom(QVariant &data)
{
    /* Pull the COM-side data wrapper open: */
    UISettingsPageMachine::fetchData(data);

    m_pCache->clear();

    UIDataSettingsMachineOhbIdentity init;

    if (!m_machine.isNull())
    {
        init.m_strProfileLast    = readExtra(m_machine, kLastProfile);

        init.m_fStealthMaster    = (readExtra(m_machine, kStealthMaster)   == QLatin1String("1"));
        /* OpenHuizeBox: first-open hand-off from the New VM wizard.
         * If the wizard captured an intent (LastProfile set, sentinel
         * unset) and the user has never touched the live HM stealth CFGM
         * key yet, seed the cache from the wizard's StealthMode intent
         * and schedule a one-shot profile auto-apply. The CFGM keys
         * themselves are written later in saveFromCache(); we do NOT
         * stamp them here. The sentinel is stamped immediately so a
         * crash mid-auto-apply does not cause a replay. */
        if (   !readExtra(m_machine, kWizardApplied).startsWith(QLatin1Char('1'))
            && readExtra(m_machine, kStealthMaster).isEmpty())
        {
            const QString strWizStealth = readExtra(m_machine, kWizardStealthMode);
            if (strWizStealth == QLatin1String("1"))
                init.m_fStealthMaster = true;
            if (!init.m_strProfileLast.isEmpty())
                setProperty("ohbWizardAutoApplyProfile", init.m_strProfileLast);
            writeExtra(m_machine, kWizardApplied, QString("1"));
        }
        /* HideDescTables: cascade from master when key absent, matching the
         * default in HMR3-x86.cpp's CFGM block. */
        {
            const QString sHide = readExtra(m_machine, kHideDescTables);
            init.m_fHideDescTables = sHide.isEmpty() ? init.m_fStealthMaster
                                                    : (sHide == QLatin1String("1"));
        }
        /* IdtrBase/GdtrBase/Limits: when stealth is on and the extradata key
         * is absent, substitute the C-side defaults from HMR3-x86.cpp so the
         * subsequent saveData() round-trip preserves a working VMX descriptor-
         * table-spoof config. Writing 0 back would override the C default and
         * silently disable SIDT/SGDT spoofing. */
        {
            const QString sIdtrBase = readExtra(m_machine, kFakeIdtrBase);
            init.m_u64FakeIdtrBase = sIdtrBase.isEmpty() && init.m_fStealthMaster
                                     ? Q_UINT64_C(0xFFFFF80000000080)
                                     : sIdtrBase.toULongLong(0, 0);
        }
        {
            const QString sIdtrLimit = readExtra(m_machine, kFakeIdtrLimit);
            init.m_u16FakeIdtrLimit = sIdtrLimit.isEmpty() && init.m_fStealthMaster
                                      ? quint16(0xFFF)
                                      : static_cast<quint16>(sIdtrLimit.toUInt(0, 0));
        }
        {
            const QString sGdtrBase = readExtra(m_machine, kFakeGdtrBase);
            init.m_u64FakeGdtrBase = sGdtrBase.isEmpty() && init.m_fStealthMaster
                                     ? Q_UINT64_C(0xFFFFF80000002000)
                                     : sGdtrBase.toULongLong(0, 0);
        }
        {
            const QString sGdtrLimit = readExtra(m_machine, kFakeGdtrLimit);
            init.m_u16FakeGdtrLimit = sGdtrLimit.isEmpty() && init.m_fStealthMaster
                                      ? quint16(0x7F)
                                      : static_cast<quint16>(sGdtrLimit.toUInt(0, 0));
        }
        init.m_i64TscOffsetBias  = readExtra(m_machine, kTscOffsetBias).toLongLong(0, 0);

        init.m_strSysVendor      = readExtra(m_machine, kDmiSysVendor);
        init.m_strSysProduct     = readExtra(m_machine, kDmiSysProduct);
        init.m_strSysVersion     = readExtra(m_machine, kDmiSysVersion);
        init.m_strSysSku         = readExtra(m_machine, kDmiSysSku);
        init.m_strSysFamily      = readExtra(m_machine, kDmiSysFamily);
        init.m_strSysSerial      = readExtra(m_machine, kDmiSysSerial);
        init.m_strBoardVendor    = readExtra(m_machine, kDmiBoardVendor);
        init.m_strBoardProduct   = readExtra(m_machine, kDmiBoardProduct);
        init.m_strBoardVersion   = readExtra(m_machine, kDmiBoardVersion);
        init.m_strBoardSerial    = readExtra(m_machine, kDmiBoardSerial);
        init.m_strChassisVendor  = readExtra(m_machine, kDmiChassisVendor);
        init.m_strChassisVersion = readExtra(m_machine, kDmiChassisVersion);
        init.m_strChassisType    = readExtra(m_machine, kDmiChassisType);
        init.m_strBiosVendor     = readExtra(m_machine, kDmiBiosVendor);
        init.m_strBiosVersion    = readExtra(m_machine, kDmiBiosVersion);
        init.m_strBiosRelease    = readExtra(m_machine, kDmiBiosRelease);
        init.m_strProcMfg        = readExtra(m_machine, kDmiProcMfg);

        init.m_strAcpiOemId      = readExtra(m_machine, kAcpiOemId);
        init.m_strAcpiTableId    = readExtra(m_machine, kAcpiTableId);
        init.m_strAcpiCreatorId  = readExtra(m_machine, kAcpiCreatorId);

        init.m_strCpuidBrand     = readExtra(m_machine, kCpuidBrand);

        init.m_strDiskModel      = readExtra(m_machine, kDiskModel);
        init.m_strDiskSerial     = readExtra(m_machine, kDiskSerial);
        init.m_strDiskFirmware   = readExtra(m_machine, kDiskFirmware);

        init.m_enmMacMode        = readExtra(m_machine, kMacMode).toInt();
        init.m_strMacPoolOui     = readExtra(m_machine, kMacPoolOui);
        init.m_strMacCustomOui   = readExtra(m_machine, kMacCustomOui);
        init.m_strMacFull        = readExtra(m_machine, kMacFull);

        m_strVmName = m_machine.GetName();
    }

    m_pCache->cacheInitialData(init);
    UISettingsPageMachine::uploadData(data);

    /* OpenHuizeBox: if the wizard scheduled a one-shot profile auto-apply,
     * fire it on the next event-loop tick. By then prepareWidgets() has
     * constructed m_pComboProfile and getFromCache() has selected the
     * stored LastProfile in it, so sltLoadProfileClicked() just works. */
    const QString strAutoApply = property("ohbWizardAutoApplyProfile").toString();
    if (!strAutoApply.isEmpty())
    {
        setProperty("ohbWizardAutoApplyProfile", QVariant()); /* one-shot */
        QMetaObject::invokeMethod(this, "sltLoadProfileClicked", Qt::QueuedConnection);
    }
}

void UIMachineSettingsOhbIdentity::getFromCache()
{
    if (!m_pCache) return;
    const UIDataSettingsMachineOhbIdentity &d = m_pCache->base();

    if (m_pComboProfile && !d.m_strProfileLast.isEmpty())
    {
        int idx = m_pComboProfile->findText(d.m_strProfileLast);
        if (idx >= 0) m_pComboProfile->setCurrentIndex(idx);
    }
    if (m_pLblProfileLast)
        m_pLblProfileLast->setText(d.m_strProfileLast.isEmpty()
            ? tr("(no profile loaded yet)") : tr("Last loaded: %1").arg(d.m_strProfileLast));

    if (m_pChkStealthMaster)   m_pChkStealthMaster->setChecked(d.m_fStealthMaster);
    if (m_pChkHideDescTables)  m_pChkHideDescTables->setChecked(d.m_fHideDescTables);
    if (m_pEdtFakeIdtrBase)    m_pEdtFakeIdtrBase->setText(QString::asprintf("0x%016llX", static_cast<unsigned long long>(d.m_u64FakeIdtrBase)));
    if (m_pEdtFakeIdtrLimit)   m_pEdtFakeIdtrLimit->setText(QString::asprintf("0x%04X",  d.m_u16FakeIdtrLimit));
    if (m_pEdtFakeGdtrBase)    m_pEdtFakeGdtrBase->setText(QString::asprintf("0x%016llX", static_cast<unsigned long long>(d.m_u64FakeGdtrBase)));
    if (m_pEdtFakeGdtrLimit)   m_pEdtFakeGdtrLimit->setText(QString::asprintf("0x%04X",  d.m_u16FakeGdtrLimit));
    if (m_pEdtTscOffsetBias)   m_pEdtTscOffsetBias->setText(QString::number(d.m_i64TscOffsetBias));

    if (m_pEdtSysVendor)       m_pEdtSysVendor->setText(d.m_strSysVendor);
    if (m_pEdtSysProduct)      m_pEdtSysProduct->setText(d.m_strSysProduct);
    if (m_pEdtSysVersion)      m_pEdtSysVersion->setText(d.m_strSysVersion);
    if (m_pEdtSysSku)          m_pEdtSysSku->setText(d.m_strSysSku);
    if (m_pEdtSysFamily)       m_pEdtSysFamily->setText(d.m_strSysFamily);
    if (m_pEdtSysSerial)       m_pEdtSysSerial->setText(d.m_strSysSerial);
    if (m_pEdtBoardVendor)     m_pEdtBoardVendor->setText(d.m_strBoardVendor);
    if (m_pEdtBoardProduct)    m_pEdtBoardProduct->setText(d.m_strBoardProduct);
    if (m_pEdtBoardVersion)    m_pEdtBoardVersion->setText(d.m_strBoardVersion);
    if (m_pEdtBoardSerial)     m_pEdtBoardSerial->setText(d.m_strBoardSerial);
    if (m_pEdtChassisVendor)   m_pEdtChassisVendor->setText(d.m_strChassisVendor);
    if (m_pEdtChassisVersion)  m_pEdtChassisVersion->setText(d.m_strChassisVersion);
    if (m_pCboChassisType)
    {
        int idx = m_pCboChassisType->findData(d.m_strChassisType);
        if (idx >= 0) m_pCboChassisType->setCurrentIndex(idx);
    }
    if (m_pEdtBiosVendor)      m_pEdtBiosVendor->setText(d.m_strBiosVendor);
    if (m_pEdtBiosVersion)     m_pEdtBiosVersion->setText(d.m_strBiosVersion);
    if (m_pEdtBiosRelease)     m_pEdtBiosRelease->setText(d.m_strBiosRelease);
    if (m_pEdtProcMfg)         m_pEdtProcMfg->setText(d.m_strProcMfg);

    if (m_pEdtAcpiOemId)       m_pEdtAcpiOemId->setText(d.m_strAcpiOemId);
    if (m_pEdtAcpiTableId)     m_pEdtAcpiTableId->setText(d.m_strAcpiTableId);
    if (m_pEdtAcpiCreatorId)   m_pEdtAcpiCreatorId->setText(d.m_strAcpiCreatorId);

    if (m_pEdtCpuidBrand)      m_pEdtCpuidBrand->setText(d.m_strCpuidBrand);

    if (m_pEdtDiskModel)       m_pEdtDiskModel->setText(d.m_strDiskModel);
    if (m_pEdtDiskSerial)      m_pEdtDiskSerial->setText(d.m_strDiskSerial);
    if (m_pEdtDiskFirmware)    m_pEdtDiskFirmware->setText(d.m_strDiskFirmware);

    if (m_pRadMacPool)         m_pRadMacPool->setChecked(d.m_enmMacMode == 0);
    if (m_pRadMacCustomOui)    m_pRadMacCustomOui->setChecked(d.m_enmMacMode == 1);
    if (m_pRadMacFull)         m_pRadMacFull->setChecked(d.m_enmMacMode == 2);
    if (m_pCboMacPool)
    {
        int idx = m_pCboMacPool->findText(d.m_strMacPoolOui);
        if (idx >= 0) m_pCboMacPool->setCurrentIndex(idx);
    }
    if (m_pEdtMacCustomOui)    m_pEdtMacCustomOui->setText(d.m_strMacCustomOui);
    if (m_pEdtMacFull)         m_pEdtMacFull->setText(d.m_strMacFull);

    sltMacModeChanged();
}

void UIMachineSettingsOhbIdentity::putToCache()
{
    if (!m_pCache) return;
    UIDataSettingsMachineOhbIdentity d = m_pCache->base();

    if (m_pComboProfile)       d.m_strProfileLast    = m_pComboProfile->currentText();

    if (m_pChkStealthMaster)   d.m_fStealthMaster    = m_pChkStealthMaster->isChecked();
    if (m_pChkHideDescTables)  d.m_fHideDescTables   = m_pChkHideDescTables->isChecked();
    if (m_pEdtFakeIdtrBase)    d.m_u64FakeIdtrBase   = m_pEdtFakeIdtrBase->text().toULongLong(0, 0);
    if (m_pEdtFakeIdtrLimit)   d.m_u16FakeIdtrLimit  = static_cast<quint16>(m_pEdtFakeIdtrLimit->text().toUInt(0, 0));
    if (m_pEdtFakeGdtrBase)    d.m_u64FakeGdtrBase   = m_pEdtFakeGdtrBase->text().toULongLong(0, 0);
    if (m_pEdtFakeGdtrLimit)   d.m_u16FakeGdtrLimit  = static_cast<quint16>(m_pEdtFakeGdtrLimit->text().toUInt(0, 0));
    if (m_pEdtTscOffsetBias)   d.m_i64TscOffsetBias  = m_pEdtTscOffsetBias->text().toLongLong(0, 0);

    if (m_pEdtSysVendor)       d.m_strSysVendor      = m_pEdtSysVendor->text();
    if (m_pEdtSysProduct)      d.m_strSysProduct     = m_pEdtSysProduct->text();
    if (m_pEdtSysVersion)      d.m_strSysVersion     = m_pEdtSysVersion->text();
    if (m_pEdtSysSku)          d.m_strSysSku         = m_pEdtSysSku->text();
    if (m_pEdtSysFamily)       d.m_strSysFamily      = m_pEdtSysFamily->text();
    if (m_pEdtSysSerial)       d.m_strSysSerial      = m_pEdtSysSerial->text();
    if (m_pEdtBoardVendor)     d.m_strBoardVendor    = m_pEdtBoardVendor->text();
    if (m_pEdtBoardProduct)    d.m_strBoardProduct   = m_pEdtBoardProduct->text();
    if (m_pEdtBoardVersion)    d.m_strBoardVersion   = m_pEdtBoardVersion->text();
    if (m_pEdtBoardSerial)     d.m_strBoardSerial    = m_pEdtBoardSerial->text();
    if (m_pEdtChassisVendor)   d.m_strChassisVendor  = m_pEdtChassisVendor->text();
    if (m_pEdtChassisVersion)  d.m_strChassisVersion = m_pEdtChassisVersion->text();
    if (m_pCboChassisType)     d.m_strChassisType    = m_pCboChassisType->currentData().toString();
    if (m_pEdtBiosVendor)      d.m_strBiosVendor     = m_pEdtBiosVendor->text();
    if (m_pEdtBiosVersion)     d.m_strBiosVersion    = m_pEdtBiosVersion->text();
    if (m_pEdtBiosRelease)     d.m_strBiosRelease    = m_pEdtBiosRelease->text();
    if (m_pEdtProcMfg)         d.m_strProcMfg        = m_pEdtProcMfg->text();

    if (m_pEdtAcpiOemId)       d.m_strAcpiOemId      = m_pEdtAcpiOemId->text();
    if (m_pEdtAcpiTableId)     d.m_strAcpiTableId    = m_pEdtAcpiTableId->text();
    if (m_pEdtAcpiCreatorId)   d.m_strAcpiCreatorId  = m_pEdtAcpiCreatorId->text();

    if (m_pEdtCpuidBrand)      d.m_strCpuidBrand     = m_pEdtCpuidBrand->text();

    if (m_pEdtDiskModel)       d.m_strDiskModel      = m_pEdtDiskModel->text();
    if (m_pEdtDiskSerial)      d.m_strDiskSerial     = m_pEdtDiskSerial->text();
    if (m_pEdtDiskFirmware)    d.m_strDiskFirmware   = m_pEdtDiskFirmware->text();

    if (m_pRadMacFull && m_pRadMacFull->isChecked())                d.m_enmMacMode = 2;
    else if (m_pRadMacCustomOui && m_pRadMacCustomOui->isChecked()) d.m_enmMacMode = 1;
    else                                                            d.m_enmMacMode = 0;
    if (m_pCboMacPool)         d.m_strMacPoolOui     = m_pCboMacPool->currentText();
    if (m_pEdtMacCustomOui)    d.m_strMacCustomOui   = m_pEdtMacCustomOui->text();
    if (m_pEdtMacFull)         d.m_strMacFull        = m_pEdtMacFull->text();

    m_pCache->cacheCurrentData(d);
}

void UIMachineSettingsOhbIdentity::saveFromCacheTo(QVariant &data)
{
    UISettingsPageMachine::fetchData(data);

    if (m_pCache && m_pCache->wasChanged())
        saveData();

    UISettingsPageMachine::uploadData(data);
}

bool UIMachineSettingsOhbIdentity::saveData()
{
    if (!m_pCache || !m_pCache->wasChanged() || m_machine.isNull())
        return true;

    const UIDataSettingsMachineOhbIdentity &d = m_pCache->data();

    writeExtra(m_machine, kLastProfile,      d.m_strProfileLast);

    writeExtra(m_machine, kStealthMaster,    d.m_fStealthMaster   ? QString("1") : QString("0"));
    writeExtra(m_machine, kHideDescTables,   d.m_fHideDescTables  ? QString("1") : QString("0"));
    writeExtra(m_machine, kFakeIdtrBase,     QString::number(d.m_u64FakeIdtrBase));
    writeExtra(m_machine, kFakeIdtrLimit,    QString::number(d.m_u16FakeIdtrLimit));
    writeExtra(m_machine, kFakeGdtrBase,     QString::number(d.m_u64FakeGdtrBase));
    writeExtra(m_machine, kFakeGdtrLimit,    QString::number(d.m_u16FakeGdtrLimit));
    writeExtra(m_machine, kTscOffsetBias,    QString::number(d.m_i64TscOffsetBias));

    writeExtra(m_machine, kDmiSysVendor,     d.m_strSysVendor);
    writeExtra(m_machine, kDmiSysProduct,    d.m_strSysProduct);
    writeExtra(m_machine, kDmiSysVersion,    d.m_strSysVersion);
    writeExtra(m_machine, kDmiSysSku,        d.m_strSysSku);
    writeExtra(m_machine, kDmiSysFamily,     d.m_strSysFamily);
    writeExtra(m_machine, kDmiSysSerial,     expandPlaceholders(d.m_strSysSerial, "SystemSerial"));
    writeExtra(m_machine, kDmiBoardVendor,   d.m_strBoardVendor);
    writeExtra(m_machine, kDmiBoardProduct,  d.m_strBoardProduct);
    writeExtra(m_machine, kDmiBoardVersion,  d.m_strBoardVersion);
    writeExtra(m_machine, kDmiBoardSerial,   expandPlaceholders(d.m_strBoardSerial, "BoardSerial"));
    writeExtra(m_machine, kDmiChassisVendor, d.m_strChassisVendor);
    writeExtra(m_machine, kDmiChassisVersion, d.m_strChassisVersion);
    writeExtra(m_machine, kDmiChassisType,   d.m_strChassisType);
    writeExtra(m_machine, kDmiBiosVendor,    d.m_strBiosVendor);
    writeExtra(m_machine, kDmiBiosVersion,   d.m_strBiosVersion);
    writeExtra(m_machine, kDmiBiosRelease,   d.m_strBiosRelease);
    writeExtra(m_machine, kDmiProcMfg,       d.m_strProcMfg);

    writeExtra(m_machine, kAcpiOemId,        d.m_strAcpiOemId);
    writeExtra(m_machine, kAcpiTableId,      d.m_strAcpiTableId);
    writeExtra(m_machine, kAcpiCreatorId,    d.m_strAcpiCreatorId);

    writeExtra(m_machine, kCpuidBrand,       d.m_strCpuidBrand);
    /* CPUID leaves live on IPlatformX86, reached via IMachine::GetPlatform()->GetX86().
     * This is the canonical path used elsewhere in the GUI (UISnapshotDetailsWidget.cpp). */
    if (!d.m_strCpuidBrand.isEmpty())
    {
        CPlatform comPlatform = m_machine.GetPlatform();
        if (comPlatform.isNotNull())
        {
            CPlatformX86 comX86 = comPlatform.GetX86();
            if (comX86.isNotNull())
            {
                QList<QPair<quint32, QList<quint32> > > leaves;
                encodeCpuidBrand(d.m_strCpuidBrand, leaves);
                for (const QPair<quint32, QList<quint32> > &leaf : leaves)
                {
                    if (leaf.second.size() == 4)
                        comX86.SetCPUIDLeaf(leaf.first, 0, leaf.second[0], leaf.second[1], leaf.second[2], leaf.second[3]);
                }
            }
        }
    }

    writeExtra(m_machine, kDiskModel,        d.m_strDiskModel);
    writeExtra(m_machine, kDiskSerial,       expandPlaceholders(d.m_strDiskSerial, "DiskSerial"));
    writeExtra(m_machine, kDiskFirmware,     d.m_strDiskFirmware);

    writeExtra(m_machine, kMacMode,          QString::number(d.m_enmMacMode));
    writeExtra(m_machine, kMacPoolOui,       d.m_strMacPoolOui);
    writeExtra(m_machine, kMacCustomOui,     d.m_strMacCustomOui);
    writeExtra(m_machine, kMacFull,          d.m_strMacFull);

    /* Compose the effective MAC and push it to NIC #1. */
    QString strOui;
    QString strTail;
    switch (d.m_enmMacMode)
    {
        case 2: /* Full */
            if (d.m_strMacFull.size() >= 12)
                m_machine.GetNetworkAdapter(0).SetMACAddress(d.m_strMacFull.toUpper().remove(QChar(':')).left(12));
            break;
        case 1: /* Custom OUI + deterministic tail */
            strOui = d.m_strMacCustomOui.toUpper().remove(QChar(':')).left(6);
            strTail = expandPlaceholders(QString("__RUNTIME_PER_VM_SERIAL__"), "MacTail").left(6);
            m_machine.GetNetworkAdapter(0).SetMACAddress(strOui + strTail);
            break;
        case 0: /* Pool */
        default:
            strOui = d.m_strMacPoolOui.toUpper().remove(QChar(':')).left(6);
            if (!strOui.isEmpty())
            {
                strTail = expandPlaceholders(QString("__RUNTIME_PER_VM_SERIAL__"), "MacTail").left(6);
                m_machine.GetNetworkAdapter(0).SetMACAddress(strOui + strTail);
            }
            break;
    }
    return true;
}

bool UIMachineSettingsOhbIdentity::validate(QList<UIValidationMessage> &messages)
{
    UIValidationMessage msg;
    msg.first = tr("OpenHuizeBox");
    bool fPass = true;

    if (m_pEdtAcpiOemId && m_pEdtAcpiOemId->text().size() > 8)
    {
        msg.second << tr("ACPI OEM Id must be at most 8 characters.");
        fPass = false;
    }
    if (m_pEdtAcpiTableId && m_pEdtAcpiTableId->text().size() > 8)
    {
        msg.second << tr("ACPI Table Id must be at most 8 characters.");
        fPass = false;
    }
    if (m_pEdtAcpiCreatorId && m_pEdtAcpiCreatorId->text().size() > 4)
    {
        msg.second << tr("ACPI Creator Id must be at most 4 characters.");
        fPass = false;
    }
    if (m_pRadMacFull && m_pRadMacFull->isChecked() && m_pEdtMacFull)
    {
        QString s = m_pEdtMacFull->text().remove(QChar(':')).remove(QChar('-'));
        if (s.size() != 12 || !QRegularExpression("^[0-9A-Fa-f]{12}$").match(s).hasMatch())
        {
            msg.second << tr("Full MAC must be 12 hexadecimal characters (e.g. 00:22:19:AA:BB:CC).");
            fPass = false;
        }
    }
    if (m_pRadMacCustomOui && m_pRadMacCustomOui->isChecked() && m_pEdtMacCustomOui)
    {
        QString s = m_pEdtMacCustomOui->text().remove(QChar(':')).remove(QChar('-'));
        if (s.size() != 6 || !QRegularExpression("^[0-9A-Fa-f]{6}$").match(s).hasMatch())
        {
            msg.second << tr("Custom MAC OUI must be 6 hexadecimal characters.");
            fPass = false;
        }
    }
    if (m_pEdtCpuidBrand && m_pEdtCpuidBrand->text().size() > 48)
    {
        msg.second << tr("CPUID brand string must be at most 48 characters.");
        fPass = false;
    }

    if (!msg.second.isEmpty())
        messages << msg;
    return fPass;
}

void UIMachineSettingsOhbIdentity::polishPage()
{
    /* Disable everything when the VM is running -- extradata writes are no-ops during execution. */
    const bool fEditable = isMachineOffline();
    setEnabled(fEditable);
}


/*********************************************************************************************************************************
*   Slots                                                                                                                        *
*********************************************************************************************************************************/

void UIMachineSettingsOhbIdentity::sltRetranslateUI()
{
    /* Static labels set in prepareWidgets(); no per-language reflow needed in v1.0. */
}

void UIMachineSettingsOhbIdentity::sltProfileSelectionChanged(int)
{
    /* Selection alone doesn't mutate fields -- user must click "Load profile". */
}

void UIMachineSettingsOhbIdentity::sltLoadProfileClicked()
{
    if (!m_pComboProfile) return;
    const QString strChosen = m_pComboProfile->currentText();
    if (strChosen.startsWith(QChar('('))) return;

    QFile f(QDir(profilesDirPath()).filePath(strChosen));
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, tr("OpenHuizeBox"),
            tr("Could not read profile file: %1").arg(f.fileName()));
        return;
    }
    QJsonParseError jpe;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &jpe);
    f.close();
    if (jpe.error != QJsonParseError::NoError)
    {
        QMessageBox::warning(this, tr("OpenHuizeBox"),
            tr("Profile JSON parse error: %1").arg(jpe.errorString()));
        return;
    }

    const QJsonObject pd = doc.object();
    const QJsonObject extra = pd.value("extradata").toObject();
    const QJsonObject disk  = pd.value("disk_identifiers").toObject();
    const QJsonArray ouipool = pd.value("mac_oui_pool").toArray();
    const QJsonObject cpuid0 = pd.value("cpuid_overrides").toArray().size() > 0
                                ? pd.value("cpuid_overrides").toArray().at(0).toObject()
                                : QJsonObject();

    auto setFromKey = [&](QLineEdit *edt, const QString &finalSeg)
    {
        if (!edt) return;
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        {
            if (it.key().endsWith(QString("/") + finalSeg))
            {
                edt->setText(it.value().toString());
                return;
            }
        }
    };

    setFromKey(m_pEdtSysVendor,      "DmiSystemVendor");
    setFromKey(m_pEdtSysProduct,     "DmiSystemProduct");
    setFromKey(m_pEdtSysVersion,     "DmiSystemVersion");
    setFromKey(m_pEdtSysSku,         "DmiSystemSKU");
    setFromKey(m_pEdtSysFamily,      "DmiSystemFamily");
    setFromKey(m_pEdtSysSerial,      "DmiSystemSerial");
    setFromKey(m_pEdtBoardVendor,    "DmiBoardVendor");
    setFromKey(m_pEdtBoardProduct,   "DmiBoardProduct");
    setFromKey(m_pEdtBoardVersion,   "DmiBoardVersion");
    setFromKey(m_pEdtBoardSerial,    "DmiBoardSerial");
    setFromKey(m_pEdtChassisVendor,  "DmiChassisVendor");
    setFromKey(m_pEdtChassisVersion, "DmiChassisVersion");
    setFromKey(m_pEdtBiosVendor,     "DmiBIOSVendor");
    setFromKey(m_pEdtBiosVersion,    "DmiBIOSVersion");
    setFromKey(m_pEdtBiosRelease,    "DmiBIOSReleaseDate");
    setFromKey(m_pEdtProcMfg,        "DmiProcManufacturer");

    if (m_pCboChassisType)
    {
        for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
        {
            if (it.key().endsWith(QString("/DmiChassisType")))
            {
                int idx = m_pCboChassisType->findData(it.value().toString());
                if (idx >= 0) m_pCboChassisType->setCurrentIndex(idx);
            }
        }
    }

    setFromKey(m_pEdtAcpiOemId,      "AcpiOemId");
    setFromKey(m_pEdtAcpiTableId,    "AcpiOemTabId");
    setFromKey(m_pEdtAcpiCreatorId,  "AcpiCreatorId");

    if (m_pEdtDiskModel    && disk.contains("hdd_model"))   m_pEdtDiskModel->setText(disk.value("hdd_model").toString());
    if (m_pEdtDiskSerial   && disk.contains("hdd_serial"))  m_pEdtDiskSerial->setText(disk.value("hdd_serial").toString());
    if (m_pEdtDiskFirmware && disk.contains("hdd_firmware")) m_pEdtDiskFirmware->setText(disk.value("hdd_firmware").toString());

    /* CPUID brand: profile JSONs encode the brand in cpuid_overrides[0]._comment. */
    if (m_pEdtCpuidBrand && !cpuid0.isEmpty())
    {
        const QString strComment = cpuid0.value("_comment").toString();
        QRegularExpression rx("'([^']+)'");
        auto match = rx.match(strComment);
        if (match.hasMatch())
            m_pEdtCpuidBrand->setText(match.captured(1));
    }

    if (m_pCboMacPool)
    {
        m_pCboMacPool->clear();
        for (const QJsonValue &v : ouipool)
            m_pCboMacPool->addItem(v.toString());
    }

    if (pd.contains("stealth_mode") && m_pChkStealthMaster)
        m_pChkStealthMaster->setChecked(pd.value("stealth_mode").toBool(true));

    if (m_pLblProfileLast)
        m_pLblProfileLast->setText(tr("Last loaded: %1").arg(strChosen));
}

void UIMachineSettingsOhbIdentity::sltApplyStealthLevelClicked()
{
    if (!m_pCmbStealthLevel) return;
    const QString strLvl = m_pCmbStealthLevel->currentData().toString();

    if (strLvl == QLatin1String("none"))
    {
        /* Disable every VMM-level stealth flag; leave SMBIOS / ACPI / MAC /
         * Disk fields alone so the user can clear them manually if they want
         * the rawest possible VBox identity. */
        if (m_pChkStealthMaster)  m_pChkStealthMaster->setChecked(false);
        if (m_pChkHideDescTables) m_pChkHideDescTables->setChecked(false);
        if (m_pEdtTscOffsetBias)  m_pEdtTscOffsetBias->setText(QString::number(0));
        return;
    }

    /* L1 and L2 share the same hardware-identity bundle: load the Lenovo
     * ThinkPad T14 profile (a known-good Notebook-class identity). The
     * existing profile loader already handles SMBIOS / ACPI / MAC / Disk
     * fields; we just trigger it programmatically. */
    if (m_pComboProfile)
    {
        const int iIdx = m_pComboProfile->findText(QLatin1String("lenovo_thinkpad_t14"));
        if (iIdx >= 0) m_pComboProfile->setCurrentIndex(iIdx);
    }
    if (m_pBtnLoadProfile)
        m_pBtnLoadProfile->click();

    /* L1 stops here -- no VMM-level changes. */
    if (strLvl == QLatin1String("l1"))
    {
        if (m_pChkStealthMaster)  m_pChkStealthMaster->setChecked(false);
        if (m_pChkHideDescTables) m_pChkHideDescTables->setChecked(false);
        return;
    }

    /* L2: enable VMM-level descriptor-table spoof + plausible Windows-x64
     * IDTR / GDTR base values (match HMR3-x86.cpp defaults). */
    if (m_pChkStealthMaster)  m_pChkStealthMaster->setChecked(true);
    if (m_pChkHideDescTables) m_pChkHideDescTables->setChecked(true);
    if (m_pEdtFakeIdtrBase  && m_pEdtFakeIdtrBase->text().isEmpty())  m_pEdtFakeIdtrBase->setText(QString::fromLatin1("0xFFFFF80000000080"));
    if (m_pEdtFakeIdtrLimit && m_pEdtFakeIdtrLimit->text().isEmpty()) m_pEdtFakeIdtrLimit->setText(QString::fromLatin1("0x0FFF"));
    if (m_pEdtFakeGdtrBase  && m_pEdtFakeGdtrBase->text().isEmpty())  m_pEdtFakeGdtrBase->setText(QString::fromLatin1("0xFFFFF80000002000"));
    if (m_pEdtFakeGdtrLimit && m_pEdtFakeGdtrLimit->text().isEmpty()) m_pEdtFakeGdtrLimit->setText(QString::fromLatin1("0x007F"));
}

void UIMachineSettingsOhbIdentity::sltStealthMasterToggled(bool fChecked)
{
    if (fChecked && m_pChkHideDescTables && !m_pChkHideDescTables->isChecked())
        m_pChkHideDescTables->setChecked(true);
}

void UIMachineSettingsOhbIdentity::sltMacModeChanged()
{
    const bool fPool   = m_pRadMacPool      && m_pRadMacPool->isChecked();
    const bool fCustom = m_pRadMacCustomOui && m_pRadMacCustomOui->isChecked();
    const bool fFull   = m_pRadMacFull      && m_pRadMacFull->isChecked();

    if (m_pCboMacPool)      m_pCboMacPool->setEnabled(fPool);
    if (m_pEdtMacCustomOui) m_pEdtMacCustomOui->setEnabled(fCustom);
    if (m_pEdtMacFull)      m_pEdtMacFull->setEnabled(fFull);

    if (m_pLblMacEffective)
    {
        QString preview;
        if (fFull && m_pEdtMacFull)
            preview = m_pEdtMacFull->text();
        else
        {
            QString oui = fCustom && m_pEdtMacCustomOui ? m_pEdtMacCustomOui->text()
                       : (m_pCboMacPool ? m_pCboMacPool->currentText() : QString());
            preview = oui + QString(":xx:xx:xx (deterministic, per-VM)");
        }
        m_pLblMacEffective->setText(tr("Effective MAC: %1").arg(preview));
    }
}

void UIMachineSettingsOhbIdentity::sltShowEffectiveIdentityClicked()
{
    if (m_machine.isNull()) return;
    QString s;
    auto add = [&](const QString &label, const char *key)
    {
        const QString v = readExtra(m_machine, key);
        if (!v.isEmpty()) s += QString("%1 = %2\n").arg(label, -22).arg(v);
    };
    add("System Vendor",   kDmiSysVendor);
    add("System Product",  kDmiSysProduct);
    add("System SKU",      kDmiSysSku);
    add("Board Product",   kDmiBoardProduct);
    add("Chassis Type",    kDmiChassisType);
    add("BIOS Vendor",     kDmiBiosVendor);
    add("BIOS Version",    kDmiBiosVersion);
    add("ACPI OEM",        kAcpiOemId);
    add("ACPI Creator",    kAcpiCreatorId);
    add("Disk Model",      kDiskModel);
    add("Disk Serial",     kDiskSerial);
    add("Stealth master",  kStealthMaster);
    add("Hide desc tbls",  kHideDescTables);

    if (s.isEmpty())
        s = tr("(no OpenHuizeBox identity extradata set yet)");

    QMessageBox box(this);
    box.setWindowTitle(tr("OpenHuizeBox - Effective identity"));
    box.setText(tr("Currently-effective identity extradata for this VM:"));
    box.setDetailedText(s);
    box.setIcon(QMessageBox::Information);
    box.exec();
}

void UIMachineSettingsOhbIdentity::sltRestoreDefaultsClicked()
{
    const QMessageBox::StandardButton ans = QMessageBox::question(
        this, tr("OpenHuizeBox"),
        tr("Clear all OpenHuizeBox identity overrides for this VM and revert to "
           "stock VirtualBox defaults?\n\nThis cannot be undone (but you can re-apply "
           "a profile afterwards)."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ans != QMessageBox::Yes) return;

    static const char *all[] = {
        kStealthMaster, kHideDescTables, kFakeIdtrBase, kFakeIdtrLimit,
        kFakeGdtrBase, kFakeGdtrLimit, kTscOffsetBias,
        kDmiSysVendor, kDmiSysProduct, kDmiSysVersion, kDmiSysSku, kDmiSysFamily, kDmiSysSerial,
        kDmiBoardVendor, kDmiBoardProduct, kDmiBoardVersion, kDmiBoardSerial,
        kDmiChassisVendor, kDmiChassisVersion, kDmiChassisType, kDmiChassisAssetTag,
        kDmiBiosVendor, kDmiBiosVersion, kDmiBiosRelease, kDmiProcMfg,
        kAcpiOemId, kAcpiTableId, kAcpiCreatorId, kAcpiCpuTableId,
        kCpuidBrand, kLastProfile,
        kDiskModel, kDiskSerial, kDiskFirmware,
        kCdromVendor, kCdromProduct, kCdromRevision,
        kVmmDevPciVendor, kVmmDevPciDevice, kVmmDevPciSubVendor, kVmmDevPciSubDevice,
        kMacMode, kMacPoolOui, kMacCustomOui, kMacFull
    };
    for (size_t i = 0; i < sizeof(all)/sizeof(all[0]); ++i)
        writeExtra(m_machine, all[i], QString());

    if (m_pCache)
    {
        UIDataSettingsMachineOhbIdentity blank;
        m_pCache->cacheInitialData(blank);
    }
    getFromCache();
    if (m_pLblProfileLast)
        m_pLblProfileLast->setText(tr("(no profile loaded yet)"));
}


/*********************************************************************************************************************************
*   Lifecycle: prepare / cleanup                                                                                                 *
*********************************************************************************************************************************/

void UIMachineSettingsOhbIdentity::prepare()
{
    m_pCache = new UISettingsCacheMachineOhbIdentity;
    prepareWidgets();
    prepareConnections();
    rescanProfiles();
}

void UIMachineSettingsOhbIdentity::cleanup()
{
    delete m_pCache; m_pCache = 0;
}

void UIMachineSettingsOhbIdentity::prepareWidgets()
{
    QVBoxLayout *pOuter = new QVBoxLayout(this);
    pOuter->setContentsMargins(0, 0, 0, 0);

    m_pScrollArea = new QScrollArea(this);
    m_pScrollArea->setWidgetResizable(true);
    m_pScrollArea->setFrameShape(QFrame::NoFrame);
    pOuter->addWidget(m_pScrollArea);

    QWidget *pContent = new QWidget;
    m_pScrollArea->setWidget(pContent);
    QVBoxLayout *pContentLay = new QVBoxLayout(pContent);

    QLabel *pHeader = new QLabel(QString::fromUtf8(
        "<h3>Hardware Identity</h3>"
        "<p style='color:#555;'>Configure the SMBIOS / DMI / ACPI / CPUID / Disk / MAC "
        "identity bytes this VM presents to its guest OS. Pick a one-click preset, edit "
        "individual fields, or both. Changes apply on VM power-on. The VM must be "
        "powered off to edit.</p>"));
    pHeader->setWordWrap(true);
    pContentLay->addWidget(pHeader);

    /* --- Stealth Level (1-click quick preset) ---
     *
     * Sits above the per-profile loader. Each level is a bundle of values
     * for the controls further down on this page; clicking [Apply] writes
     * them into the controls (it does NOT save -- the regular OK / Apply
     * button on the dialog persists). The help label spells out exactly
     * what each level changes so users know what they are enabling.
     */
    {
        QGroupBox *box = new QGroupBox(tr("Stealth Level (one-click preset)"));
        QGridLayout *g = new QGridLayout(box);

        g->addWidget(new QLabel(tr("<b>Pick a level, then Apply:</b>")), 0, 0, 1, 3);

        m_pCmbStealthLevel = new QComboBox;
        m_pCmbStealthLevel->addItem(tr("None  -  raw VBox (VM is detectable, no performance cost)"),
                                    QString("none"));
        m_pCmbStealthLevel->addItem(tr("L1 Light  -  SMBIOS / ACPI / MAC / Disk identity spoof (no performance cost)"),
                                    QString("l1"));
        m_pCmbStealthLevel->addItem(tr("L2 Deep  -  L1 + descriptor-table spoof + TSC compensation + PCI override (~3-5%% perf cost)"),
                                    QString("l2"));
        g->addWidget(m_pCmbStealthLevel, 1, 0, 1, 2);

        m_pBtnApplyStealthLevel = new QPushButton(tr("Apply preset to fields"));
        g->addWidget(m_pBtnApplyStealthLevel, 1, 2);

        QLabel *pLblHelp = new QLabel(tr(
            "<p style='color:#555; margin-top:6px;'>"
            "<b>L1 Light:</b> Reports a Lenovo ThinkPad T14 identity (BIOS / board / "
            "chassis / serial / MAC OUI / SSD model) to the guest OS. No VMM-level "
            "changes -- defeats string-matching detection like \"VBOX\" in BIOS ROM, "
            "<code>08:00:27</code> MAC OUI, <code>VBOX HARDDISK</code> in disk model. "
            "Negligible performance impact, safe to leave on.<br><br>"
            "<b>L2 Deep:</b> Everything in L1, plus VMM-level changes: descriptor-"
            "table exiting (defeats SIDT/SGDT Red-Pill detection used by Pafish / "
            "Al-Khaser / VMaware), TSC handler-cost compensation (defeats RDTSC "
            "timing detection), per-device PCI vendor override (clears VEN_80EE "
            "from Device Manager). Costs roughly 3-5%% on instruction-heavy "
            "workloads because each SIDT / SGDT now traps into the hypervisor."
            "</p>"));
        pLblHelp->setWordWrap(true);
        g->addWidget(pLblHelp, 2, 0, 1, 3);

        pContentLay->addWidget(box);
    }

    /* --- 1-click preset --- */
    {
        QGroupBox *box = new QGroupBox(tr("Hardware profile (granular)"));
        QGridLayout *g = new QGridLayout(box);
        g->addWidget(new QLabel(tr("Profile:")), 0, 0);
        m_pComboProfile = new QComboBox; g->addWidget(m_pComboProfile, 0, 1);
        m_pBtnLoadProfile = new QPushButton(tr("Load profile")); g->addWidget(m_pBtnLoadProfile, 0, 2);
        m_pLblProfileLast = new QLabel(tr("(no profile loaded yet)"));
        m_pLblProfileLast->setStyleSheet("color:#777;");
        g->addWidget(m_pLblProfileLast, 1, 0, 1, 3);
        QLabel *pLblProfileHelp = new QLabel(tr(
            "<p style='color:#555;'>Profiles ship with curated hardware identities "
            "(Lenovo ThinkPad T14, Dell OptiPlex 7080, HP EliteBook 840 G8, MSI "
            "PRO B650, etc.). Loading one fills SMBIOS / ACPI / MAC / Disk fields "
            "below; you can then edit any individual field before saving.</p>"));
        pLblProfileHelp->setWordWrap(true);
        g->addWidget(pLblProfileHelp, 2, 0, 1, 3);
        pContentLay->addWidget(box);
    }

    /* --- VMM stealth --- */
    {
        QGroupBox *box = new QGroupBox(tr("VMM stealth (anti-detection runtime)"));
        QGridLayout *g = new QGridLayout(box);
        m_pChkStealthMaster  = new QCheckBox(tr("Stealth master switch  (HM/OhbStealth)"));
        m_pChkHideDescTables = new QCheckBox(tr("Hide descriptor tables (SIDT/SGDT VT-x exit)"));
        m_pEdtFakeIdtrBase   = new QLineEdit; m_pEdtFakeIdtrBase->setPlaceholderText("0xFFFFF80000000080");
        m_pEdtFakeIdtrLimit  = new QLineEdit; m_pEdtFakeIdtrLimit->setPlaceholderText("0x0FFF");
        m_pEdtFakeGdtrBase   = new QLineEdit; m_pEdtFakeGdtrBase->setPlaceholderText("0xFFFFF80000002000");
        m_pEdtFakeGdtrLimit  = new QLineEdit; m_pEdtFakeGdtrLimit->setPlaceholderText("0x007F");
        m_pEdtTscOffsetBias  = new QLineEdit; m_pEdtTscOffsetBias->setPlaceholderText("0");
        int row = 0;
        g->addWidget(m_pChkStealthMaster,   row++, 0, 1, 4);
        g->addWidget(m_pChkHideDescTables,  row++, 0, 1, 4);
        g->addWidget(new QLabel(tr("Fake IDTR base:")), row, 0); g->addWidget(m_pEdtFakeIdtrBase,  row, 1);
        g->addWidget(new QLabel(tr("Limit:")),          row, 2); g->addWidget(m_pEdtFakeIdtrLimit, row, 3); ++row;
        g->addWidget(new QLabel(tr("Fake GDTR base:")), row, 0); g->addWidget(m_pEdtFakeGdtrBase,  row, 1);
        g->addWidget(new QLabel(tr("Limit:")),          row, 2); g->addWidget(m_pEdtFakeGdtrLimit, row, 3); ++row;
        g->addWidget(new QLabel(tr("TSC offset bias:")), row, 0); g->addWidget(m_pEdtTscOffsetBias, row, 1, 1, 3); ++row;
        pContentLay->addWidget(box);
    }

    /* --- SMBIOS / DMI --- */
    {
        QGroupBox *box = new QGroupBox(tr("SMBIOS / DMI"));
        QFormLayout *f = new QFormLayout(box);
        auto E = [&](const QString &lbl) -> QLineEdit* { QLineEdit *e = new QLineEdit; f->addRow(lbl, e); return e; };
        m_pEdtSysVendor      = E(tr("System Vendor"));
        m_pEdtSysProduct     = E(tr("System Product"));
        m_pEdtSysVersion     = E(tr("System Version"));
        m_pEdtSysSku         = E(tr("System SKU"));
        m_pEdtSysFamily      = E(tr("System Family"));
        m_pEdtSysSerial      = E(tr("System Serial"));
        m_pEdtSysSerial->setPlaceholderText("(leave blank for deterministic per-VM)");
        m_pEdtBoardVendor    = E(tr("Board Vendor"));
        m_pEdtBoardProduct   = E(tr("Board Product"));
        m_pEdtBoardVersion   = E(tr("Board Version"));
        m_pEdtBoardSerial    = E(tr("Board Serial"));
        m_pEdtBoardSerial->setPlaceholderText("(deterministic)");
        m_pEdtChassisVendor  = E(tr("Chassis Vendor"));
        m_pEdtChassisVersion = E(tr("Chassis Version"));
        m_pCboChassisType    = new QComboBox;
        m_pCboChassisType->addItem(tr("1 - Other"),         "1");
        m_pCboChassisType->addItem(tr("3 - Desktop"),       "3");
        m_pCboChassisType->addItem(tr("8 - Portable"),      "8");
        m_pCboChassisType->addItem(tr("9 - Laptop"),        "9");
        m_pCboChassisType->addItem(tr("10 - Notebook"),     "10");
        m_pCboChassisType->addItem(tr("13 - AIO"),          "13");
        m_pCboChassisType->addItem(tr("14 - Sub-Notebook"), "14");
        m_pCboChassisType->addItem(tr("23 - Rack Mount"),   "23");
        f->addRow(tr("Chassis Type"), m_pCboChassisType);
        m_pEdtBiosVendor     = E(tr("BIOS Vendor"));
        m_pEdtBiosVersion    = E(tr("BIOS Version"));
        m_pEdtBiosRelease    = E(tr("BIOS Release Date"));
        m_pEdtProcMfg        = E(tr("Processor Manufacturer"));
        pContentLay->addWidget(box);
    }

    /* --- ACPI --- */
    {
        QGroupBox *box = new QGroupBox(tr("ACPI"));
        QFormLayout *f = new QFormLayout(box);
        m_pEdtAcpiOemId     = new QLineEdit; m_pEdtAcpiOemId->setMaxLength(8);     f->addRow(tr("OEM Id (max 8)"),    m_pEdtAcpiOemId);
        m_pEdtAcpiTableId   = new QLineEdit; m_pEdtAcpiTableId->setMaxLength(8);   f->addRow(tr("Table Id (max 8)"),  m_pEdtAcpiTableId);
        m_pEdtAcpiCreatorId = new QLineEdit; m_pEdtAcpiCreatorId->setMaxLength(4); f->addRow(tr("Creator Id (max 4)"), m_pEdtAcpiCreatorId);
        pContentLay->addWidget(box);
    }

    /* --- CPUID --- */
    {
        QGroupBox *box = new QGroupBox(tr("CPUID"));
        QFormLayout *f = new QFormLayout(box);
        m_pEdtCpuidBrand = new QLineEdit;
        m_pEdtCpuidBrand->setMaxLength(48);
        m_pEdtCpuidBrand->setPlaceholderText("Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz");
        f->addRow(tr("Brand string (auto-encoded to leaves 0x80000002-04)"), m_pEdtCpuidBrand);
        pContentLay->addWidget(box);
    }

    /* --- Disk --- */
    {
        QGroupBox *box = new QGroupBox(tr("Disk identifiers (primary AHCI)"));
        QFormLayout *f = new QFormLayout(box);
        m_pEdtDiskModel    = new QLineEdit; f->addRow(tr("Model"),    m_pEdtDiskModel);
        m_pEdtDiskSerial   = new QLineEdit; m_pEdtDiskSerial->setPlaceholderText("(deterministic per-VM)");
        f->addRow(tr("Serial"),   m_pEdtDiskSerial);
        m_pEdtDiskFirmware = new QLineEdit; f->addRow(tr("Firmware"), m_pEdtDiskFirmware);
        pContentLay->addWidget(box);
    }

    /* --- MAC --- */
    {
        QGroupBox *box = new QGroupBox(tr("Network MAC"));
        QGridLayout *g = new QGridLayout(box);
        m_pRadMacPool       = new QRadioButton(tr("From profile pool"));      m_pRadMacPool->setChecked(true);
        m_pRadMacCustomOui  = new QRadioButton(tr("Custom OUI + deterministic tail"));
        m_pRadMacFull       = new QRadioButton(tr("Full custom MAC"));
        m_pCboMacPool       = new QComboBox;
        m_pEdtMacCustomOui  = new QLineEdit;  m_pEdtMacCustomOui->setMaxLength(8);  m_pEdtMacCustomOui->setPlaceholderText("002219");
        m_pEdtMacFull       = new QLineEdit;  m_pEdtMacFull->setMaxLength(17);      m_pEdtMacFull->setPlaceholderText("00:22:19:AA:BB:CC");
        m_pLblMacEffective  = new QLabel(tr("Effective MAC: (will be computed on Apply)"));
        m_pLblMacEffective->setStyleSheet("color:#777;");
        g->addWidget(m_pRadMacPool,      0, 0);  g->addWidget(m_pCboMacPool,      0, 1);
        g->addWidget(m_pRadMacCustomOui, 1, 0);  g->addWidget(m_pEdtMacCustomOui, 1, 1);
        g->addWidget(m_pRadMacFull,      2, 0);  g->addWidget(m_pEdtMacFull,      2, 1);
        g->addWidget(m_pLblMacEffective, 3, 0, 1, 2);
        pContentLay->addWidget(box);
    }

    /* --- Bottom toolbar --- */
    {
        QHBoxLayout *h = new QHBoxLayout;
        m_pBtnShowEffective   = new QPushButton(tr("Show current effective identity..."));
        m_pBtnRestoreDefaults = new QPushButton(tr("Restore VBox defaults..."));
        h->addWidget(m_pBtnShowEffective);
        h->addWidget(m_pBtnRestoreDefaults);
        h->addStretch();
        pContentLay->addLayout(h);
    }

    pContentLay->addStretch();
}

void UIMachineSettingsOhbIdentity::prepareConnections()
{
    if (m_pComboProfile)
        connect(m_pComboProfile, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &UIMachineSettingsOhbIdentity::sltProfileSelectionChanged);
    if (m_pBtnLoadProfile)
        connect(m_pBtnLoadProfile, &QPushButton::clicked, this, &UIMachineSettingsOhbIdentity::sltLoadProfileClicked);
    if (m_pBtnApplyStealthLevel)
        connect(m_pBtnApplyStealthLevel, &QPushButton::clicked, this, &UIMachineSettingsOhbIdentity::sltApplyStealthLevelClicked);
    if (m_pChkStealthMaster)
        connect(m_pChkStealthMaster, &QCheckBox::toggled, this, &UIMachineSettingsOhbIdentity::sltStealthMasterToggled);
    if (m_pRadMacPool)      connect(m_pRadMacPool,      &QRadioButton::toggled, this, &UIMachineSettingsOhbIdentity::sltMacModeChanged);
    if (m_pRadMacCustomOui) connect(m_pRadMacCustomOui, &QRadioButton::toggled, this, &UIMachineSettingsOhbIdentity::sltMacModeChanged);
    if (m_pRadMacFull)      connect(m_pRadMacFull,      &QRadioButton::toggled, this, &UIMachineSettingsOhbIdentity::sltMacModeChanged);
    if (m_pBtnShowEffective)
        connect(m_pBtnShowEffective, &QPushButton::clicked, this, &UIMachineSettingsOhbIdentity::sltShowEffectiveIdentityClicked);
    if (m_pBtnRestoreDefaults)
        connect(m_pBtnRestoreDefaults, &QPushButton::clicked, this, &UIMachineSettingsOhbIdentity::sltRestoreDefaultsClicked);
}


/*********************************************************************************************************************************
*   Helpers                                                                                                                      *
*********************************************************************************************************************************/

QString UIMachineSettingsOhbIdentity::profilesDirPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + "/../modules/01_hardware_fingerprint/profiles",
        appDir + "/modules/01_hardware_fingerprint/profiles",
        appDir + "/../../modules/01_hardware_fingerprint/profiles"
    };
    for (const QString &p : candidates)
        if (QDir(p).exists())
            return QDir::cleanPath(p);
    return candidates.first();
}

void UIMachineSettingsOhbIdentity::rescanProfiles()
{
    if (!m_pComboProfile) return;
    m_pComboProfile->clear();
    QDir d(profilesDirPath());
    const QStringList files = d.entryList(QStringList() << "*.json", QDir::Files);
    if (files.isEmpty())
        m_pComboProfile->addItem(tr("(no profiles found)"));
    else
        for (const QString &f : files) m_pComboProfile->addItem(f);
}

QString UIMachineSettingsOhbIdentity::expandPlaceholders(const QString &strRaw, const QString &strPurpose) const
{
    if (!strRaw.contains("__RUNTIME_PER_VM_"))
        return strRaw;

    QByteArray seed = m_strVmName.toUtf8() + "|" + strPurpose.toUtf8();
    QByteArray h = QCryptographicHash::hash(seed, QCryptographicHash::Sha256);

    QString result = strRaw;
    if (result.contains("__RUNTIME_PER_VM_UUID__"))
    {
        const QString hex = h.left(16).toHex();
        const QString uuid = QString("%1-%2-%3-%4-%5")
            .arg(hex.mid(0, 8))
            .arg(hex.mid(8, 4))
            .arg(hex.mid(12, 4))
            .arg(hex.mid(16, 4))
            .arg(hex.mid(20, 12));
        result.replace("__RUNTIME_PER_VM_UUID__", uuid);
    }
    if (result.contains("__RUNTIME_PER_VM_SERIAL__"))
    {
        const QString serial = h.left(8).toHex().toUpper();
        result.replace("__RUNTIME_PER_VM_SERIAL__", serial);
    }
    return result;
}

void UIMachineSettingsOhbIdentity::encodeCpuidBrand(const QString &strBrand,
                                                    QList<QPair<quint32, QList<quint32> > > &leaves) const
{
    QByteArray padded = strBrand.toLatin1();
    padded = padded.leftJustified(48, ' ', true);

    auto packReg = [&](int offset) -> quint32 {
        quint32 v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<quint32>(static_cast<unsigned char>(padded.at(offset + i))) << (8 * i);
        return v;
    };
    for (int li = 0; li < 3; ++li)
    {
        QList<quint32> regs;
        for (int ri = 0; ri < 4; ++ri)
            regs << packReg(li * 16 + ri * 4);
        leaves << qMakePair(quint32(0x80000002 + li), regs);
    }
}
