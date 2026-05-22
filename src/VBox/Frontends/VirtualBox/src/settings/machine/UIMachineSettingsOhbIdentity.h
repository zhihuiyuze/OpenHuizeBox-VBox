/* $Id$ */
/** @file
 * OpenHuizeBox - UIMachineSettingsOhbIdentity class declaration.
 *
 * Consolidated VM settings page for "Realistic Hardware Identity":
 *   - 1-click hardware-profile picker (JSON-driven preset)
 *   - VMM stealth master switch + sub-toggles (HM/Ohb* CFGM keys)
 *   - SMBIOS / DMI strings (Devices/pcbios/0/Config/Dmi*)
 *   - ACPI OEM strings (Devices/acpi/0/Config/Acpi*)
 *   - CPUID brand string (auto-encoded into leaves 0x80000002-04)
 *   - Disk identifiers (Devices/ahci/0/LUN#0/AttachedDriver/Config/*)
 *   - MAC OUI selector (from profile pool, custom OUI, or full MAC)
 *
 * Replaces the per-tab QGroupBoxes injected by buildProfileBox() into
 * the System / Display / Network / Storage pages (Sprint 8-14). Single
 * source of truth lives here; the New VM wizard reuses the inner widget.
 */

#ifndef FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsOhbIdentity_h
#define FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsOhbIdentity_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "UISettingsPage.h"

/* Forward declarations: */
struct UIDataSettingsMachineOhbIdentity;
typedef UISettingsCache<UIDataSettingsMachineOhbIdentity> UISettingsCacheMachineOhbIdentity;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QScrollArea;

/** Machine settings: OpenHuizeBox Identity page. */
class SHARED_LIBRARY_STUFF UIMachineSettingsOhbIdentity : public UISettingsPageMachine
{
    Q_OBJECT;

public:

    UIMachineSettingsOhbIdentity();
    virtual ~UIMachineSettingsOhbIdentity() RT_OVERRIDE;

protected:

    virtual bool changed() const RT_OVERRIDE;

    virtual void loadToCacheFrom(QVariant &data) RT_OVERRIDE;
    virtual void getFromCache() RT_OVERRIDE;
    virtual void putToCache() RT_OVERRIDE;
    virtual void saveFromCacheTo(QVariant &data) RT_OVERRIDE;

    virtual bool validate(QList<UIValidationMessage> &messages) RT_OVERRIDE;

    virtual void polishPage() RT_OVERRIDE;

private slots:

    virtual void sltRetranslateUI() RT_OVERRIDE RT_FINAL;

    /** Profile combobox: user picked a different preset. */
    void sltProfileSelectionChanged(int iIndex);
    /** [Load profile] button clicked: copy profile values into all field widgets. */
    void sltLoadProfileClicked();
    /** Stealth master toggled: cascade sub-toggles to follow when transitioning to ON. */
    void sltStealthMasterToggled(bool fChecked);
    /** MAC mode radio changed: enable/disable OUI combo + full-MAC line edit. */
    void sltMacModeChanged();
    /** [Show effective identity...] button clicked: read-only modal of current extradata. */
    void sltShowEffectiveIdentityClicked();
    /** [Restore VBox defaults...] button clicked: confirm + clear all extradata. */
    void sltRestoreDefaultsClicked();

private:

    /** Prepares all. */
    void prepare();
    /** Builds the (scrolling) inner layout. */
    void prepareWidgets();
    /** Wires slots. */
    void prepareConnections();
    /** Cleans up. */
    void cleanup();

    /** Returns the absolute path to the profiles directory
     *  (modules/01_hardware_fingerprint/profiles relative to the install). */
    static QString profilesDirPath();

    /** Saves the currently-cached data to the CMachine. */
    bool saveData();

    /** Populates the profile combobox from disk. */
    void rescanProfiles();

    /** Replaces __RUNTIME_PER_VM_UUID__ / __RUNTIME_PER_VM_SERIAL__ placeholders.
     *  Salt is m_strVmName so two VMs of the same profile get distinct serials. */
    QString expandPlaceholders(const QString &strRaw, const QString &strPurpose) const;

    /** Encodes a CPUID brand string (up to 48 chars) into three leaves
     *  (0x80000002-04) -> 12 uint32 values, applied via modifyvm --cpuidset. */
    void encodeCpuidBrand(const QString &strBrand,
                          QList<QPair<quint32, QList<quint32> > > &leaves) const;

    /** Cached VM name (resolved lazily once m_machine is set). */
    QString m_strVmName;

    /** Cache backing store. */
    UISettingsCacheMachineOhbIdentity *m_pCache;

    /** @name Widgets - top-level
     * @{ */
        QScrollArea *m_pScrollArea;
    /** @} */

    /** @name Widgets - 1-click preset
     * @{ */
        QComboBox   *m_pComboProfile;
        QLabel      *m_pLblProfileLast;
        QPushButton *m_pBtnLoadProfile;
    /** @} */

    /** @name Widgets - VMM stealth
     * @{ */
        QCheckBox *m_pChkStealthMaster;
        QCheckBox *m_pChkHideDescTables;
        QLineEdit *m_pEdtFakeIdtrBase;
        QLineEdit *m_pEdtFakeIdtrLimit;
        QLineEdit *m_pEdtFakeGdtrBase;
        QLineEdit *m_pEdtFakeGdtrLimit;
        QLineEdit *m_pEdtTscOffsetBias;
    /** @} */

    /** @name Widgets - SMBIOS / DMI (17 fields). The values are mapped 1:1
     *  to VBoxInternal/Devices/pcbios/0/Config/Dmi* extradata keys in
     *  loadToCacheFrom() / saveFromCacheTo().
     * @{ */
        QLineEdit *m_pEdtSysVendor;
        QLineEdit *m_pEdtSysProduct;
        QLineEdit *m_pEdtSysVersion;
        QLineEdit *m_pEdtSysSku;
        QLineEdit *m_pEdtSysFamily;
        QLineEdit *m_pEdtSysSerial;        /* deterministic; "(deterministic)" placeholder by default */
        QLineEdit *m_pEdtBoardVendor;
        QLineEdit *m_pEdtBoardProduct;
        QLineEdit *m_pEdtBoardVersion;
        QLineEdit *m_pEdtBoardSerial;
        QLineEdit *m_pEdtChassisVendor;
        QLineEdit *m_pEdtChassisVersion;
        QComboBox *m_pCboChassisType;      /* SMBIOS enum (1-Other, 3-Desktop, 9-Laptop...) */
        QLineEdit *m_pEdtBiosVendor;
        QLineEdit *m_pEdtBiosVersion;
        QLineEdit *m_pEdtBiosRelease;
        QLineEdit *m_pEdtProcMfg;
    /** @} */

    /** @name Widgets - ACPI
     * @{ */
        QLineEdit *m_pEdtAcpiOemId;        /* max 8 chars */
        QLineEdit *m_pEdtAcpiTableId;      /* max 8 chars */
        QLineEdit *m_pEdtAcpiCreatorId;    /* max 4 chars */
    /** @} */

    /** @name Widgets - CPUID
     * @{ */
        QLineEdit *m_pEdtCpuidBrand;       /* up to 48 chars; auto-split on save */
    /** @} */

    /** @name Widgets - Disk
     * @{ */
        QLineEdit *m_pEdtDiskModel;
        QLineEdit *m_pEdtDiskSerial;       /* deterministic-by-default */
        QLineEdit *m_pEdtDiskFirmware;
    /** @} */

    /** @name Widgets - MAC
     * @{ */
        QRadioButton *m_pRadMacPool;
        QRadioButton *m_pRadMacCustomOui;
        QRadioButton *m_pRadMacFull;
        QComboBox    *m_pCboMacPool;       /* shows profile.mac_oui_pool entries */
        QLineEdit    *m_pEdtMacCustomOui;  /* 6 hex chars */
        QLineEdit    *m_pEdtMacFull;       /* 12 hex chars */
        QLabel       *m_pLblMacEffective;  /* live "OUI:tail" preview */
    /** @} */

    /** @name Widgets - bottom toolbar
     * @{ */
        QPushButton *m_pBtnShowEffective;
        QPushButton *m_pBtnRestoreDefaults;
    /** @} */
};

#endif /* !FEQT_INCLUDED_SRC_settings_machine_UIMachineSettingsOhbIdentity_h */
